/* snap-store.c
 *
 * SQLite database layer for Snap versioning system.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-store.h"
#include "snap-hash.h"
#include <glib/gstdio.h>
#include <stdio.h>
#include <zlib.h>
#include <string.h>

struct _SnapStore
{
    GObject    parent_instance;
    sqlite3   *db;
    gchar     *store_path;
    gchar     *db_path;
    gchar     *objects_path;
    gboolean   is_open;
};

G_DEFINE_FINAL_TYPE (SnapStore, snap_store, G_TYPE_OBJECT)

static SnapStore *default_store = NULL;

/* --- Free functions --- */

void
snap_snapshot_free (SnapSnapshot *snapshot)
{
    if (snapshot == NULL)
        return;
    g_free (snapshot->file_path);
    g_free (snapshot->message);
    g_free (snapshot->hash);
    g_free (snapshot->branch);
    g_free (snapshot->parent_hash);
    g_free (snapshot->blob_path);
    g_free (snapshot);
}

void
snap_branch_free (SnapBranch *branch)
{
    if (branch == NULL)
        return;
    g_free (branch->name);
    g_free (branch->file_path);
    g_free (branch->parent_branch);
    g_free (branch);
}

void
snap_group_free (SnapGroup *group)
{
    if (group == NULL)
        return;
    g_free (group->name);
    g_free (group->message);
    g_free (group);
}

void
snap_stash_free (SnapStash *stash)
{
    if (stash == NULL)
        return;
    g_free (stash->file_path);
    g_free (stash->message);
    g_free (stash->content_hash);
    g_free (stash);
}

void
snap_hook_free (SnapHook *hook)
{
    if (hook == NULL)
        return;
    g_free (hook->name);
    g_free (hook->event);
    g_free (hook->script_path);
    g_free (hook);
}

/* --- Compression helpers --- */

static guchar *
compress_data (const gchar *data, gsize length, gsize *out_len)
{
    uLongf compressed_len = compressBound (length);
    guchar *compressed = g_malloc (compressed_len);

    if (compress2 (compressed, &compressed_len,
                   (const Bytef *) data, length, Z_DEFAULT_COMPRESSION) != Z_OK)
    {
        g_free (compressed);
        return NULL;
    }

    *out_len = compressed_len;
    return compressed;
}

static gchar *
decompress_data (const guchar *data, gsize length, gsize original_len, gsize *out_len)
{
    uLongf decompressed_len = original_len;
    gchar *decompressed = g_malloc (decompressed_len + 1);

    if (uncompress ((Bytef *) decompressed, &decompressed_len,
                    data, length) != Z_OK)
    {
        g_free (decompressed);
        return NULL;
    }

    decompressed[decompressed_len] = '\0';
    *out_len = decompressed_len;
    return decompressed;
}

/* --- Database schema --- */

static const gchar *CREATE_TABLES_SQL =
    "CREATE TABLE IF NOT EXISTS snapshots ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  file_path TEXT NOT NULL,"
    "  message TEXT,"
    "  hash TEXT NOT NULL,"
    "  version INTEGER NOT NULL,"
    "  timestamp INTEGER NOT NULL,"
    "  size INTEGER NOT NULL,"
    "  branch TEXT NOT NULL DEFAULT 'main',"
    "  pinned INTEGER NOT NULL DEFAULT 0,"
    "  encrypted INTEGER NOT NULL DEFAULT 0,"
    "  parent_hash TEXT,"
    "  blob_path TEXT NOT NULL,"
    "  original_size INTEGER NOT NULL DEFAULT 0"
    ");"
    "CREATE TABLE IF NOT EXISTS branches ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  name TEXT NOT NULL,"
    "  file_path TEXT NOT NULL,"
    "  created_at INTEGER NOT NULL,"
    "  parent_branch TEXT,"
    "  fork_version INTEGER,"
    "  UNIQUE(name, file_path)"
    ");"
    "CREATE TABLE IF NOT EXISTS groups ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  name TEXT NOT NULL,"
    "  message TEXT,"
    "  timestamp INTEGER NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS group_files ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  group_id INTEGER NOT NULL,"
    "  file_path TEXT NOT NULL,"
    "  snapshot_version INTEGER NOT NULL,"
    "  FOREIGN KEY(group_id) REFERENCES groups(id)"
    ");"
    "CREATE TABLE IF NOT EXISTS stashes ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  file_path TEXT NOT NULL,"
    "  message TEXT,"
    "  content_hash TEXT NOT NULL,"
    "  timestamp INTEGER NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS hooks ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  name TEXT NOT NULL UNIQUE,"
    "  event TEXT NOT NULL,"
    "  script_path TEXT NOT NULL,"
    "  enabled INTEGER NOT NULL DEFAULT 1"
    ");"
    "CREATE TABLE IF NOT EXISTS audit_log ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  file_path TEXT NOT NULL,"
    "  action TEXT NOT NULL,"
    "  hash TEXT,"
    "  previous_hash TEXT,"
    "  timestamp INTEGER NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS file_status ("
    "  file_path TEXT PRIMARY KEY,"
    "  last_hash TEXT NOT NULL,"
    "  last_modified INTEGER NOT NULL"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_snapshots_file ON snapshots(file_path, branch, version);"
    "CREATE INDEX IF NOT EXISTS idx_snapshots_timestamp ON snapshots(timestamp);"
    "CREATE INDEX IF NOT EXISTS idx_branches_file ON branches(file_path);"
    "CREATE INDEX IF NOT EXISTS idx_audit_file ON audit_log(file_path);";

/* --- GObject boilerplate --- */

static void
snap_store_finalize (GObject *object)
{
    SnapStore *self = SNAP_STORE (object);

    snap_store_close (self);
    g_free (self->store_path);
    g_free (self->db_path);
    g_free (self->objects_path);

    G_OBJECT_CLASS (snap_store_parent_class)->finalize (object);
}

static void
snap_store_class_init (SnapStoreClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = snap_store_finalize;
}

static void
snap_store_init (SnapStore *self)
{
    const gchar *home = g_get_home_dir ();
    self->store_path = g_build_filename (home, ".snapstore", NULL);
    self->db_path = g_build_filename (self->store_path, "snap.db", NULL);
    self->objects_path = g_build_filename (self->store_path, "objects", NULL);
    self->is_open = FALSE;
}

SnapStore *
snap_store_get_default (void)
{
    if (default_store == NULL)
    {
        default_store = g_object_new (SNAP_TYPE_STORE, NULL);
    }
    return default_store;
}

/* --- Database lifecycle --- */

gboolean
snap_store_open (SnapStore *self, GError **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), FALSE);

    if (self->is_open)
        return TRUE;

    /* Create directories */
    g_mkdir_with_parents (self->store_path, 0755);
    g_mkdir_with_parents (self->objects_path, 0755);

    /* Open database */
    int rc = sqlite3_open (self->db_path, &self->db);
    if (rc != SQLITE_OK)
    {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     "Failed to open snap database: %s",
                     sqlite3_errmsg (self->db));
        return FALSE;
    }

    /* Enable WAL mode for performance */
    sqlite3_exec (self->db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec (self->db, "PRAGMA foreign_keys=ON;", NULL, NULL, NULL);

    /* Create tables */
    char *err_msg = NULL;
    rc = sqlite3_exec (self->db, CREATE_TABLES_SQL, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK)
    {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     "Failed to create tables: %s", err_msg);
        sqlite3_free (err_msg);
        return FALSE;
    }

    self->is_open = TRUE;
    return TRUE;
}

void
snap_store_close (SnapStore *self)
{
    g_return_if_fail (SNAP_IS_STORE (self));

    if (self->db != NULL)
    {
        sqlite3_close (self->db);
        self->db = NULL;
    }
    self->is_open = FALSE;
}

/* --- Object store --- */

gchar *
snap_store_write_blob (SnapStore   *self,
                       const gchar *content,
                       gsize        length,
                       GError     **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), NULL);
    g_return_val_if_fail (self->is_open, NULL);

    /* Hash the content */
    g_autofree gchar *hash = snap_hash_data ((const guchar *) content, length);
    if (hash == NULL)
    {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "Failed to hash content");
        return NULL;
    }

    /* Build blob path: objects/ab/cdef1234... */
    gchar prefix[3] = { hash[0], hash[1], '\0' };
    g_autofree gchar *prefix_dir = g_build_filename (self->objects_path, prefix, NULL);
    g_mkdir_with_parents (prefix_dir, 0755);

    g_autofree gchar *blob_path = g_build_filename (prefix_dir, hash + 2, NULL);

    /* Skip if blob already exists */
    if (g_file_test (blob_path, G_FILE_TEST_EXISTS))
        return g_steal_pointer (&hash);

    /* Compress and write */
    gsize compressed_len;
    g_autofree guchar *compressed = compress_data (content, length, &compressed_len);
    if (compressed == NULL)
    {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "Failed to compress content");
        return NULL;
    }

    /* Write 8-byte header with original size, then compressed data */
    FILE *f = fopen (blob_path, "wb");
    if (f == NULL)
    {
        g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno),
                     "Failed to write blob: %s", g_strerror (errno));
        return NULL;
    }

    guint64 orig_size = (guint64) length;
    fwrite (&orig_size, sizeof (guint64), 1, f);
    fwrite (compressed, 1, compressed_len, f);
    fclose (f);

    return g_steal_pointer (&hash);
}

gchar *
snap_store_read_blob (SnapStore   *self,
                      const gchar *hash,
                      gsize       *out_length,
                      GError     **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), NULL);
    g_return_val_if_fail (hash != NULL, NULL);

    /* Build blob path */
    gchar prefix[3] = { hash[0], hash[1], '\0' };
    g_autofree gchar *blob_path = g_build_filename (self->objects_path, prefix,
                                                     hash + 2, NULL);

    /* Read file */
    gchar *raw_data = NULL;
    gsize raw_len;
    if (!g_file_get_contents (blob_path, &raw_data, &raw_len, error))
        return NULL;

    /* First 8 bytes are original size */
    if (raw_len < sizeof (guint64))
    {
        g_free (raw_data);
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                             "Blob file too short");
        return NULL;
    }

    guint64 orig_size;
    memcpy (&orig_size, raw_data, sizeof (guint64));

    gsize decompressed_len;
    gchar *content = decompress_data ((const guchar *) raw_data + sizeof (guint64),
                                      raw_len - sizeof (guint64),
                                      (gsize) orig_size,
                                      &decompressed_len);
    g_free (raw_data);

    if (content == NULL)
    {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "Failed to decompress blob");
        return NULL;
    }

    if (out_length)
        *out_length = decompressed_len;

    return content;
}

gboolean
snap_store_delete_blob (SnapStore   *self,
                        const gchar *hash,
                        GError     **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), FALSE);

    gchar prefix[3] = { hash[0], hash[1], '\0' };
    g_autofree gchar *blob_path = g_build_filename (self->objects_path, prefix,
                                                     hash + 2, NULL);

    if (g_unlink (blob_path) != 0 && errno != ENOENT)
    {
        g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno),
                     "Failed to delete blob: %s", g_strerror (errno));
        return FALSE;
    }

    return TRUE;
}

/* --- Snapshot operations --- */

static SnapSnapshot *
snapshot_from_stmt (sqlite3_stmt *stmt)
{
    SnapSnapshot *s = g_new0 (SnapSnapshot, 1);
    s->id = sqlite3_column_int64 (stmt, 0);
    s->file_path = g_strdup ((const gchar *) sqlite3_column_text (stmt, 1));
    s->message = g_strdup ((const gchar *) sqlite3_column_text (stmt, 2));
    s->hash = g_strdup ((const gchar *) sqlite3_column_text (stmt, 3));
    s->version = sqlite3_column_int64 (stmt, 4);
    s->timestamp = sqlite3_column_int64 (stmt, 5);
    s->size = sqlite3_column_int64 (stmt, 6);
    s->branch = g_strdup ((const gchar *) sqlite3_column_text (stmt, 7));
    s->pinned = sqlite3_column_int (stmt, 8) != 0;
    s->encrypted = sqlite3_column_int (stmt, 9) != 0;
    s->parent_hash = g_strdup ((const gchar *) sqlite3_column_text (stmt, 10));
    s->blob_path = g_strdup ((const gchar *) sqlite3_column_text (stmt, 11));
    return s;
}

SnapSnapshot *
snap_store_save_snapshot (SnapStore   *self,
                          const gchar *file_path,
                          const gchar *content,
                          gsize        length,
                          const gchar *message,
                          const gchar *branch,
                          GError     **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), NULL);
    g_return_val_if_fail (self->is_open, NULL);

    const gchar *use_branch = branch ? branch : "main";

    /* Write blob */
    g_autofree gchar *hash = snap_store_write_blob (self, content, length, error);
    if (hash == NULL)
        return NULL;

    /* Get next version number */
    gint64 version = snap_store_get_latest_version (self, file_path, use_branch) + 1;

    /* Get parent hash */
    g_autofree gchar *parent_hash = NULL;
    if (version > 1)
    {
        g_autoptr (SnapSnapshot) prev = snap_store_get_snapshot (self, file_path,
                                                                  version - 1,
                                                                  use_branch, NULL);
        if (prev != NULL)
            parent_hash = g_strdup (prev->hash);
    }

    /* Build blob path reference */
    gchar prefix[3] = { hash[0], hash[1], '\0' };
    g_autofree gchar *blob_rel = g_strdup_printf ("%s/%s", prefix, hash + 2);

    gint64 now = g_get_real_time () / G_USEC_PER_SEC;

    /* Insert snapshot record */
    sqlite3_stmt *stmt;
    const gchar *sql = "INSERT INTO snapshots "
        "(file_path, message, hash, version, timestamp, size, branch, "
        "parent_hash, blob_path, original_size) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     "SQL prepare failed: %s", sqlite3_errmsg (self->db));
        return NULL;
    }

    sqlite3_bind_text (stmt, 1, file_path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 2, message, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 3, hash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64 (stmt, 4, version);
    sqlite3_bind_int64 (stmt, 5, now);
    sqlite3_bind_int64 (stmt, 6, (gint64) length);
    sqlite3_bind_text (stmt, 7, use_branch, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 8, parent_hash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 9, blob_rel, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64 (stmt, 10, (gint64) length);

    if (sqlite3_step (stmt) != SQLITE_DONE)
    {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     "Failed to save snapshot: %s", sqlite3_errmsg (self->db));
        sqlite3_finalize (stmt);
        return NULL;
    }

    gint64 id = sqlite3_last_insert_rowid (self->db);
    sqlite3_finalize (stmt);

    /* Update file status */
    const gchar *status_sql = "INSERT OR REPLACE INTO file_status "
        "(file_path, last_hash, last_modified) VALUES (?, ?, ?)";
    if (sqlite3_prepare_v2 (self->db, status_sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text (stmt, 1, file_path, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 2, hash, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64 (stmt, 3, now);
        sqlite3_step (stmt);
        sqlite3_finalize (stmt);
    }

    /* Insert audit log */
    const gchar *audit_sql = "INSERT INTO audit_log "
        "(file_path, action, hash, previous_hash, timestamp) VALUES (?, 'save', ?, ?, ?)";
    if (sqlite3_prepare_v2 (self->db, audit_sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text (stmt, 1, file_path, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 2, hash, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 3, parent_hash, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64 (stmt, 4, now);
        sqlite3_step (stmt);
        sqlite3_finalize (stmt);
    }

    /* Build return object */
    SnapSnapshot *snapshot = g_new0 (SnapSnapshot, 1);
    snapshot->id = id;
    snapshot->file_path = g_strdup (file_path);
    snapshot->message = g_strdup (message);
    snapshot->hash = g_strdup (hash);
    snapshot->version = version;
    snapshot->timestamp = now;
    snapshot->size = (gint64) length;
    snapshot->branch = g_strdup (use_branch);
    snapshot->pinned = FALSE;
    snapshot->encrypted = FALSE;
    snapshot->parent_hash = g_strdup (parent_hash);
    snapshot->blob_path = g_strdup (blob_rel);

    return snapshot;
}

SnapSnapshot *
snap_store_get_snapshot (SnapStore   *self,
                         const gchar *file_path,
                         gint64       version,
                         const gchar *branch,
                         GError     **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), NULL);

    const gchar *use_branch = branch ? branch : "main";
    const gchar *sql = "SELECT id, file_path, message, hash, version, timestamp, "
        "size, branch, pinned, encrypted, parent_hash, blob_path "
        "FROM snapshots WHERE file_path = ? AND version = ? AND branch = ?";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return NULL;

    sqlite3_bind_text (stmt, 1, file_path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64 (stmt, 2, version);
    sqlite3_bind_text (stmt, 3, use_branch, -1, SQLITE_TRANSIENT);

    SnapSnapshot *snapshot = NULL;
    if (sqlite3_step (stmt) == SQLITE_ROW)
        snapshot = snapshot_from_stmt (stmt);

    sqlite3_finalize (stmt);
    return snapshot;
}

GList *
snap_store_get_history (SnapStore   *self,
                        const gchar *file_path,
                        const gchar *branch,
                        gint         limit,
                        GError     **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), NULL);

    const gchar *use_branch = branch ? branch : "main";
    const gchar *sql = "SELECT id, file_path, message, hash, version, timestamp, "
        "size, branch, pinned, encrypted, parent_hash, blob_path "
        "FROM snapshots WHERE file_path = ? AND branch = ? "
        "ORDER BY version DESC LIMIT ?";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     "SQL error: %s", sqlite3_errmsg (self->db));
        return NULL;
    }

    sqlite3_bind_text (stmt, 1, file_path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 2, use_branch, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 3, limit > 0 ? limit : 1000);

    GList *list = NULL;
    while (sqlite3_step (stmt) == SQLITE_ROW)
        list = g_list_prepend (list, snapshot_from_stmt (stmt));

    sqlite3_finalize (stmt);
    return g_list_reverse (list);
}

gchar *
snap_store_get_snapshot_content (SnapStore    *self,
                                 SnapSnapshot *snapshot,
                                 GError      **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), NULL);
    g_return_val_if_fail (snapshot != NULL, NULL);

    return snap_store_read_blob (self, snapshot->hash, NULL, error);
}

gboolean
snap_store_delete_snapshot (SnapStore   *self,
                            const gchar *file_path,
                            gint64       version,
                            const gchar *branch,
                            GError     **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), FALSE);

    const gchar *use_branch = branch ? branch : "main";

    /* Get hash before deleting */
    g_autoptr (SnapSnapshot) snap = snap_store_get_snapshot (self, file_path,
                                                              version, use_branch, NULL);
    if (snap == NULL)
    {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                             "Snapshot not found");
        return FALSE;
    }

    const gchar *sql = "DELETE FROM snapshots WHERE file_path = ? AND version = ? AND branch = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return FALSE;

    sqlite3_bind_text (stmt, 1, file_path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64 (stmt, 2, version);
    sqlite3_bind_text (stmt, 3, use_branch, -1, SQLITE_TRANSIENT);

    gboolean ok = sqlite3_step (stmt) == SQLITE_DONE;
    sqlite3_finalize (stmt);

    /* Check if blob is still referenced */
    if (ok)
    {
        const gchar *check_sql = "SELECT COUNT(*) FROM snapshots WHERE hash = ?";
        if (sqlite3_prepare_v2 (self->db, check_sql, -1, &stmt, NULL) == SQLITE_OK)
        {
            sqlite3_bind_text (stmt, 1, snap->hash, -1, SQLITE_TRANSIENT);
            if (sqlite3_step (stmt) == SQLITE_ROW && sqlite3_column_int (stmt, 0) == 0)
                snap_store_delete_blob (self, snap->hash, NULL);
            sqlite3_finalize (stmt);
        }
    }

    return ok;
}

gint64
snap_store_get_latest_version (SnapStore   *self,
                                const gchar *file_path,
                                const gchar *branch)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), 0);

    const gchar *use_branch = branch ? branch : "main";
    const gchar *sql = "SELECT MAX(version) FROM snapshots WHERE file_path = ? AND branch = ?";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return 0;

    sqlite3_bind_text (stmt, 1, file_path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 2, use_branch, -1, SQLITE_TRANSIENT);

    gint64 version = 0;
    if (sqlite3_step (stmt) == SQLITE_ROW)
        version = sqlite3_column_int64 (stmt, 0);

    sqlite3_finalize (stmt);
    return version;
}

gint64
snap_store_get_snapshot_count (SnapStore   *self,
                               const gchar *file_path)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), 0);

    const gchar *sql = "SELECT COUNT(*) FROM snapshots WHERE file_path = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return 0;

    sqlite3_bind_text (stmt, 1, file_path, -1, SQLITE_TRANSIENT);

    gint64 count = 0;
    if (sqlite3_step (stmt) == SQLITE_ROW)
        count = sqlite3_column_int64 (stmt, 0);

    sqlite3_finalize (stmt);
    return count;
}

/* --- Pin operations --- */

gboolean
snap_store_pin_snapshot (SnapStore   *self,
                         const gchar *file_path,
                         gint64       version,
                         const gchar *branch,
                         GError     **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), FALSE);

    const gchar *use_branch = branch ? branch : "main";
    const gchar *sql = "UPDATE snapshots SET pinned = 1 WHERE file_path = ? AND version = ? AND branch = ?";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return FALSE;

    sqlite3_bind_text (stmt, 1, file_path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64 (stmt, 2, version);
    sqlite3_bind_text (stmt, 3, use_branch, -1, SQLITE_TRANSIENT);

    gboolean ok = sqlite3_step (stmt) == SQLITE_DONE;
    sqlite3_finalize (stmt);
    return ok;
}

gboolean
snap_store_unpin_snapshot (SnapStore   *self,
                           const gchar *file_path,
                           gint64       version,
                           const gchar *branch,
                           GError     **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), FALSE);

    const gchar *use_branch = branch ? branch : "main";
    const gchar *sql = "UPDATE snapshots SET pinned = 0 WHERE file_path = ? AND version = ? AND branch = ?";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return FALSE;

    sqlite3_bind_text (stmt, 1, file_path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64 (stmt, 2, version);
    sqlite3_bind_text (stmt, 3, use_branch, -1, SQLITE_TRANSIENT);

    gboolean ok = sqlite3_step (stmt) == SQLITE_DONE;
    sqlite3_finalize (stmt);
    return ok;
}

GList *
snap_store_get_pinned (SnapStore   *self,
                       const gchar *file_path,
                       GError     **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), NULL);

    const gchar *sql = "SELECT id, file_path, message, hash, version, timestamp, "
        "size, branch, pinned, encrypted, parent_hash, blob_path "
        "FROM snapshots WHERE file_path = ? AND pinned = 1 ORDER BY version DESC";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return NULL;

    sqlite3_bind_text (stmt, 1, file_path, -1, SQLITE_TRANSIENT);

    GList *list = NULL;
    while (sqlite3_step (stmt) == SQLITE_ROW)
        list = g_list_prepend (list, snapshot_from_stmt (stmt));

    sqlite3_finalize (stmt);
    return g_list_reverse (list);
}

/* --- Branch operations --- */

SnapBranch *
snap_store_create_branch (SnapStore   *self,
                           const gchar *file_path,
                           const gchar *name,
                           const gchar *parent_branch,
                           gint64       fork_version,
                           GError     **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), NULL);

    gint64 now = g_get_real_time () / G_USEC_PER_SEC;
    const gchar *sql = "INSERT INTO branches (name, file_path, created_at, parent_branch, fork_version) "
        "VALUES (?, ?, ?, ?, ?)";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     "SQL error: %s", sqlite3_errmsg (self->db));
        return NULL;
    }

    sqlite3_bind_text (stmt, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 2, file_path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64 (stmt, 3, now);
    sqlite3_bind_text (stmt, 4, parent_branch ? parent_branch : "main", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64 (stmt, 5, fork_version);

    if (sqlite3_step (stmt) != SQLITE_DONE)
    {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     "Failed to create branch: %s", sqlite3_errmsg (self->db));
        sqlite3_finalize (stmt);
        return NULL;
    }

    gint64 id = sqlite3_last_insert_rowid (self->db);
    sqlite3_finalize (stmt);

    SnapBranch *branch = g_new0 (SnapBranch, 1);
    branch->id = id;
    branch->name = g_strdup (name);
    branch->file_path = g_strdup (file_path);
    branch->created_at = now;
    branch->parent_branch = g_strdup (parent_branch ? parent_branch : "main");
    branch->fork_version = fork_version;

    return branch;
}

GList *
snap_store_list_branches (SnapStore   *self,
                           const gchar *file_path,
                           GError     **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), NULL);

    const gchar *sql = "SELECT id, name, file_path, created_at, parent_branch, fork_version "
        "FROM branches WHERE file_path = ? ORDER BY created_at";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return NULL;

    sqlite3_bind_text (stmt, 1, file_path, -1, SQLITE_TRANSIENT);

    GList *list = NULL;
    while (sqlite3_step (stmt) == SQLITE_ROW)
    {
        SnapBranch *b = g_new0 (SnapBranch, 1);
        b->id = sqlite3_column_int64 (stmt, 0);
        b->name = g_strdup ((const gchar *) sqlite3_column_text (stmt, 1));
        b->file_path = g_strdup ((const gchar *) sqlite3_column_text (stmt, 2));
        b->created_at = sqlite3_column_int64 (stmt, 3);
        b->parent_branch = g_strdup ((const gchar *) sqlite3_column_text (stmt, 4));
        b->fork_version = sqlite3_column_int64 (stmt, 5);
        list = g_list_prepend (list, b);
    }

    sqlite3_finalize (stmt);
    return g_list_reverse (list);
}

gboolean
snap_store_delete_branch (SnapStore   *self,
                           const gchar *file_path,
                           const gchar *name,
                           GError     **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), FALSE);

    /* Delete all snapshots on this branch */
    const gchar *del_snaps = "DELETE FROM snapshots WHERE file_path = ? AND branch = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, del_snaps, -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text (stmt, 1, file_path, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 2, name, -1, SQLITE_TRANSIENT);
        sqlite3_step (stmt);
        sqlite3_finalize (stmt);
    }

    /* Delete branch record */
    const gchar *sql = "DELETE FROM branches WHERE file_path = ? AND name = ?";
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return FALSE;

    sqlite3_bind_text (stmt, 1, file_path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 2, name, -1, SQLITE_TRANSIENT);

    gboolean ok = sqlite3_step (stmt) == SQLITE_DONE;
    sqlite3_finalize (stmt);
    return ok;
}

gboolean
snap_store_merge_branch (SnapStore   *self,
                          const gchar *file_path,
                          const gchar *source,
                          const gchar *target,
                          GError     **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), FALSE);

    /* Get latest snapshot from source branch */
    g_autoptr (SnapSnapshot) src_snap = NULL;
    gint64 latest = snap_store_get_latest_version (self, file_path, source);
    if (latest == 0)
    {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                             "Source branch has no snapshots");
        return FALSE;
    }

    src_snap = snap_store_get_snapshot (self, file_path, latest, source, error);
    if (src_snap == NULL)
        return FALSE;

    /* Read source content and save to target branch */
    g_autofree gchar *content = snap_store_get_snapshot_content (self, src_snap, error);
    if (content == NULL)
        return FALSE;

    g_autofree gchar *msg = g_strdup_printf ("Merged from %s v%ld", source, (long) latest);
    g_autoptr (SnapSnapshot) result = snap_store_save_snapshot (self, file_path, content,
                                                                 strlen (content), msg,
                                                                 target, error);
    return result != NULL;
}

/* --- Group operations --- */

SnapGroup *
snap_store_create_group (SnapStore   *self,
                          const gchar *name,
                          const gchar *message,
                          GList       *file_paths,
                          GError     **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), NULL);

    gint64 now = g_get_real_time () / G_USEC_PER_SEC;

    /* Insert group */
    sqlite3_stmt *stmt;
    const gchar *sql = "INSERT INTO groups (name, message, timestamp) VALUES (?, ?, ?)";
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return NULL;

    sqlite3_bind_text (stmt, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 2, message, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64 (stmt, 3, now);

    if (sqlite3_step (stmt) != SQLITE_DONE)
    {
        sqlite3_finalize (stmt);
        return NULL;
    }

    gint64 group_id = sqlite3_last_insert_rowid (self->db);
    sqlite3_finalize (stmt);

    /* Save each file and link to group */
    for (GList *l = file_paths; l != NULL; l = l->next)
    {
        const gchar *fp = (const gchar *) l->data;

        /* Read current file content */
        g_autofree gchar *content = NULL;
        gsize len;
        if (!g_file_get_contents (fp, &content, &len, NULL))
            continue;

        /* Save snapshot */
        g_autoptr (SnapSnapshot) snap = snap_store_save_snapshot (self, fp, content, len,
                                                                   message, "main", NULL);
        if (snap == NULL)
            continue;

        /* Link to group */
        const gchar *link_sql = "INSERT INTO group_files (group_id, file_path, snapshot_version) "
            "VALUES (?, ?, ?)";
        if (sqlite3_prepare_v2 (self->db, link_sql, -1, &stmt, NULL) == SQLITE_OK)
        {
            sqlite3_bind_int64 (stmt, 1, group_id);
            sqlite3_bind_text (stmt, 2, fp, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64 (stmt, 3, snap->version);
            sqlite3_step (stmt);
            sqlite3_finalize (stmt);
        }
    }

    SnapGroup *group = g_new0 (SnapGroup, 1);
    group->id = group_id;
    group->name = g_strdup (name);
    group->message = g_strdup (message);
    group->timestamp = now;

    return group;
}

GList *
snap_store_list_groups (SnapStore *self, GError **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), NULL);

    const gchar *sql = "SELECT id, name, message, timestamp FROM groups ORDER BY timestamp DESC";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return NULL;

    GList *list = NULL;
    while (sqlite3_step (stmt) == SQLITE_ROW)
    {
        SnapGroup *g = g_new0 (SnapGroup, 1);
        g->id = sqlite3_column_int64 (stmt, 0);
        g->name = g_strdup ((const gchar *) sqlite3_column_text (stmt, 1));
        g->message = g_strdup ((const gchar *) sqlite3_column_text (stmt, 2));
        g->timestamp = sqlite3_column_int64 (stmt, 3);
        list = g_list_prepend (list, g);
    }

    sqlite3_finalize (stmt);
    return g_list_reverse (list);
}

GList *
snap_store_get_group_files (SnapStore *self, gint64 group_id, GError **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), NULL);

    const gchar *sql = "SELECT file_path FROM group_files WHERE group_id = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return NULL;

    sqlite3_bind_int64 (stmt, 1, group_id);

    GList *list = NULL;
    while (sqlite3_step (stmt) == SQLITE_ROW)
        list = g_list_prepend (list, g_strdup ((const gchar *) sqlite3_column_text (stmt, 0)));

    sqlite3_finalize (stmt);
    return g_list_reverse (list);
}

gboolean
snap_store_restore_group (SnapStore *self, gint64 group_id, GError **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), FALSE);

    const gchar *sql = "SELECT file_path, snapshot_version FROM group_files WHERE group_id = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return FALSE;

    sqlite3_bind_int64 (stmt, 1, group_id);

    while (sqlite3_step (stmt) == SQLITE_ROW)
    {
        const gchar *fp = (const gchar *) sqlite3_column_text (stmt, 0);
        gint64 ver = sqlite3_column_int64 (stmt, 1);

        g_autoptr (SnapSnapshot) snap = snap_store_get_snapshot (self, fp, ver, "main", NULL);
        if (snap != NULL)
        {
            g_autofree gchar *content = snap_store_get_snapshot_content (self, snap, NULL);
            if (content != NULL)
                g_file_set_contents (fp, content, -1, NULL);
        }
    }

    sqlite3_finalize (stmt);
    return TRUE;
}

/* --- Stash operations --- */

SnapStash *
snap_store_stash_save (SnapStore   *self,
                        const gchar *file_path,
                        const gchar *message,
                        GError     **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), NULL);

    g_autofree gchar *content = NULL;
    gsize len;
    if (!g_file_get_contents (file_path, &content, &len, error))
        return NULL;

    g_autofree gchar *hash = snap_store_write_blob (self, content, len, error);
    if (hash == NULL)
        return NULL;

    gint64 now = g_get_real_time () / G_USEC_PER_SEC;

    sqlite3_stmt *stmt;
    const gchar *sql = "INSERT INTO stashes (file_path, message, content_hash, timestamp) "
        "VALUES (?, ?, ?, ?)";
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return NULL;

    sqlite3_bind_text (stmt, 1, file_path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 2, message, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 3, hash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64 (stmt, 4, now);

    if (sqlite3_step (stmt) != SQLITE_DONE)
    {
        sqlite3_finalize (stmt);
        return NULL;
    }

    gint64 id = sqlite3_last_insert_rowid (self->db);
    sqlite3_finalize (stmt);

    SnapStash *stash = g_new0 (SnapStash, 1);
    stash->id = id;
    stash->file_path = g_strdup (file_path);
    stash->message = g_strdup (message);
    stash->content_hash = g_strdup (hash);
    stash->timestamp = now;

    return stash;
}

SnapStash *
snap_store_stash_pop (SnapStore   *self,
                       const gchar *file_path,
                       GError     **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), NULL);

    /* Get most recent stash */
    const gchar *sql = "SELECT id, file_path, message, content_hash, timestamp "
        "FROM stashes WHERE file_path = ? ORDER BY timestamp DESC LIMIT 1";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return NULL;

    sqlite3_bind_text (stmt, 1, file_path, -1, SQLITE_TRANSIENT);

    if (sqlite3_step (stmt) != SQLITE_ROW)
    {
        sqlite3_finalize (stmt);
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                             "No stash found");
        return NULL;
    }

    SnapStash *stash = g_new0 (SnapStash, 1);
    stash->id = sqlite3_column_int64 (stmt, 0);
    stash->file_path = g_strdup ((const gchar *) sqlite3_column_text (stmt, 1));
    stash->message = g_strdup ((const gchar *) sqlite3_column_text (stmt, 2));
    stash->content_hash = g_strdup ((const gchar *) sqlite3_column_text (stmt, 3));
    stash->timestamp = sqlite3_column_int64 (stmt, 4);
    sqlite3_finalize (stmt);

    /* Restore content */
    g_autofree gchar *content = snap_store_read_blob (self, stash->content_hash, NULL, error);
    if (content != NULL)
        g_file_set_contents (file_path, content, -1, NULL);

    /* Delete stash */
    const gchar *del_sql = "DELETE FROM stashes WHERE id = ?";
    if (sqlite3_prepare_v2 (self->db, del_sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64 (stmt, 1, stash->id);
        sqlite3_step (stmt);
        sqlite3_finalize (stmt);
    }

    return stash;
}

GList *
snap_store_stash_list (SnapStore   *self,
                        const gchar *file_path,
                        GError     **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), NULL);

    const gchar *sql = "SELECT id, file_path, message, content_hash, timestamp "
        "FROM stashes WHERE file_path = ? ORDER BY timestamp DESC";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return NULL;

    sqlite3_bind_text (stmt, 1, file_path, -1, SQLITE_TRANSIENT);

    GList *list = NULL;
    while (sqlite3_step (stmt) == SQLITE_ROW)
    {
        SnapStash *s = g_new0 (SnapStash, 1);
        s->id = sqlite3_column_int64 (stmt, 0);
        s->file_path = g_strdup ((const gchar *) sqlite3_column_text (stmt, 1));
        s->message = g_strdup ((const gchar *) sqlite3_column_text (stmt, 2));
        s->content_hash = g_strdup ((const gchar *) sqlite3_column_text (stmt, 3));
        s->timestamp = sqlite3_column_int64 (stmt, 4);
        list = g_list_prepend (list, s);
    }

    sqlite3_finalize (stmt);
    return g_list_reverse (list);
}

/* --- Hook store operations --- */

gboolean
snap_store_add_hook (SnapStore   *self,
                      const gchar *name,
                      const gchar *event,
                      const gchar *script_path,
                      GError     **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), FALSE);

    const gchar *sql = "INSERT OR REPLACE INTO hooks (name, event, script_path) VALUES (?, ?, ?)";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return FALSE;

    sqlite3_bind_text (stmt, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 2, event, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 3, script_path, -1, SQLITE_TRANSIENT);

    gboolean ok = sqlite3_step (stmt) == SQLITE_DONE;
    sqlite3_finalize (stmt);
    return ok;
}

gboolean
snap_store_remove_hook (SnapStore   *self,
                         const gchar *name,
                         GError     **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), FALSE);

    const gchar *sql = "DELETE FROM hooks WHERE name = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return FALSE;

    sqlite3_bind_text (stmt, 1, name, -1, SQLITE_TRANSIENT);

    gboolean ok = sqlite3_step (stmt) == SQLITE_DONE;
    sqlite3_finalize (stmt);
    return ok;
}

GList *
snap_store_list_hooks (SnapStore *self, GError **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), NULL);

    const gchar *sql = "SELECT id, name, event, script_path, enabled FROM hooks ORDER BY name";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return NULL;

    GList *list = NULL;
    while (sqlite3_step (stmt) == SQLITE_ROW)
    {
        SnapHook *h = g_new0 (SnapHook, 1);
        h->id = sqlite3_column_int64 (stmt, 0);
        h->name = g_strdup ((const gchar *) sqlite3_column_text (stmt, 1));
        h->event = g_strdup ((const gchar *) sqlite3_column_text (stmt, 2));
        h->script_path = g_strdup ((const gchar *) sqlite3_column_text (stmt, 3));
        h->enabled = sqlite3_column_int (stmt, 4) != 0;
        list = g_list_prepend (list, h);
    }

    sqlite3_finalize (stmt);
    return g_list_reverse (list);
}

/* --- Log / timeline --- */

GList *
snap_store_get_global_log (SnapStore *self, gint limit, GError **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), NULL);

    const gchar *sql = "SELECT id, file_path, message, hash, version, timestamp, "
        "size, branch, pinned, encrypted, parent_hash, blob_path "
        "FROM snapshots ORDER BY timestamp DESC LIMIT ?";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return NULL;

    sqlite3_bind_int (stmt, 1, limit > 0 ? limit : 100);

    GList *list = NULL;
    while (sqlite3_step (stmt) == SQLITE_ROW)
        list = g_list_prepend (list, snapshot_from_stmt (stmt));

    sqlite3_finalize (stmt);
    return g_list_reverse (list);
}

GList *
snap_store_search_snapshots (SnapStore *self, const gchar *query, GError **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), NULL);

    const gchar *sql = "SELECT id, file_path, message, hash, version, timestamp, "
        "size, branch, pinned, encrypted, parent_hash, blob_path "
        "FROM snapshots WHERE message LIKE ? OR file_path LIKE ? "
        "ORDER BY timestamp DESC LIMIT 100";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return NULL;

    g_autofree gchar *pattern = g_strdup_printf ("%%%s%%", query);
    sqlite3_bind_text (stmt, 1, pattern, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 2, pattern, -1, SQLITE_TRANSIENT);

    GList *list = NULL;
    while (sqlite3_step (stmt) == SQLITE_ROW)
        list = g_list_prepend (list, snapshot_from_stmt (stmt));

    sqlite3_finalize (stmt);
    return g_list_reverse (list);
}

/* --- Maintenance --- */

gint
snap_store_gc (SnapStore *self, gint keep_versions, GError **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), 0);

    if (keep_versions <= 0)
        keep_versions = 10;

    /* Find snapshots to delete: non-pinned, beyond keep limit per file/branch */
    const gchar *sql =
        "DELETE FROM snapshots WHERE id IN ("
        "  SELECT id FROM ("
        "    SELECT id, ROW_NUMBER() OVER (PARTITION BY file_path, branch ORDER BY version DESC) as rn"
        "    FROM snapshots WHERE pinned = 0"
        "  ) WHERE rn > ?"
        ")";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     "GC SQL error: %s", sqlite3_errmsg (self->db));
        return 0;
    }

    sqlite3_bind_int (stmt, 1, keep_versions);

    if (sqlite3_step (stmt) != SQLITE_DONE)
    {
        sqlite3_finalize (stmt);
        return 0;
    }

    gint deleted = sqlite3_changes (self->db);
    sqlite3_finalize (stmt);

    return deleted;
}

gboolean
snap_store_clean (SnapStore *self, const gchar *file_path, GError **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), FALSE);

    const gchar *sql = "DELETE FROM snapshots WHERE file_path = ? AND pinned = 0";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return FALSE;

    sqlite3_bind_text (stmt, 1, file_path, -1, SQLITE_TRANSIENT);

    gboolean ok = sqlite3_step (stmt) == SQLITE_DONE;
    sqlite3_finalize (stmt);
    return ok;
}

gboolean
snap_store_repair (SnapStore *self, GError **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), FALSE);

    /* Integrity check */
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, "PRAGMA integrity_check", -1, &stmt, NULL) != SQLITE_OK)
        return FALSE;

    gboolean ok = TRUE;
    if (sqlite3_step (stmt) == SQLITE_ROW)
    {
        const gchar *result = (const gchar *) sqlite3_column_text (stmt, 0);
        ok = g_strcmp0 (result, "ok") == 0;
    }

    sqlite3_finalize (stmt);

    if (!ok)
    {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "Database integrity check failed");
    }

    /* Vacuum */
    sqlite3_exec (self->db, "VACUUM;", NULL, NULL, NULL);

    return ok;
}

/* --- Stats --- */

gint64
snap_store_get_total_snapshots (SnapStore *self)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), 0);

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, "SELECT COUNT(*) FROM snapshots", -1, &stmt, NULL) != SQLITE_OK)
        return 0;

    gint64 count = 0;
    if (sqlite3_step (stmt) == SQLITE_ROW)
        count = sqlite3_column_int64 (stmt, 0);

    sqlite3_finalize (stmt);
    return count;
}

gint64
snap_store_get_total_files (SnapStore *self)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), 0);

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, "SELECT COUNT(DISTINCT file_path) FROM snapshots",
                            -1, &stmt, NULL) != SQLITE_OK)
        return 0;

    gint64 count = 0;
    if (sqlite3_step (stmt) == SQLITE_ROW)
        count = sqlite3_column_int64 (stmt, 0);

    sqlite3_finalize (stmt);
    return count;
}

gint64
snap_store_get_store_size (SnapStore *self)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), 0);

    /* Sum all blob sizes from the objects directory */
    g_autoptr (GFile) dir = g_file_new_for_path (self->objects_path);
    g_autoptr (GFileEnumerator) enumerator = g_file_enumerate_children (
        dir, G_FILE_ATTRIBUTE_STANDARD_SIZE "," G_FILE_ATTRIBUTE_STANDARD_TYPE,
        G_FILE_QUERY_INFO_NONE, NULL, NULL);

    if (enumerator == NULL)
        return 0;

    gint64 total = 0;
    GFileInfo *info;
    while ((info = g_file_enumerator_next_file (enumerator, NULL, NULL)) != NULL)
    {
        if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
        {
            g_autoptr (GFile) subdir = g_file_enumerator_get_child (enumerator, info);
            g_autoptr (GFileEnumerator) sub_enum = g_file_enumerate_children (
                subdir, G_FILE_ATTRIBUTE_STANDARD_SIZE,
                G_FILE_QUERY_INFO_NONE, NULL, NULL);
            if (sub_enum != NULL)
            {
                GFileInfo *sub_info;
                while ((sub_info = g_file_enumerator_next_file (sub_enum, NULL, NULL)) != NULL)
                {
                    total += g_file_info_get_size (sub_info);
                    g_object_unref (sub_info);
                }
            }
        }
        g_object_unref (info);
    }

    return total;
}

/* --- File status --- */

gboolean
snap_store_is_tracked (SnapStore *self, const gchar *file_path)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), FALSE);

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db,
                            "SELECT 1 FROM file_status WHERE file_path = ?",
                            -1, &stmt, NULL) != SQLITE_OK)
        return FALSE;

    sqlite3_bind_text (stmt, 1, file_path, -1, SQLITE_TRANSIENT);

    gboolean tracked = sqlite3_step (stmt) == SQLITE_ROW;
    sqlite3_finalize (stmt);
    return tracked;
}

gboolean
snap_store_is_modified (SnapStore *self, const gchar *file_path)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), FALSE);

    /* Get stored hash */
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db,
                            "SELECT last_hash FROM file_status WHERE file_path = ?",
                            -1, &stmt, NULL) != SQLITE_OK)
        return FALSE;

    sqlite3_bind_text (stmt, 1, file_path, -1, SQLITE_TRANSIENT);

    if (sqlite3_step (stmt) != SQLITE_ROW)
    {
        sqlite3_finalize (stmt);
        return FALSE;
    }

    g_autofree gchar *stored_hash = g_strdup ((const gchar *) sqlite3_column_text (stmt, 0));
    sqlite3_finalize (stmt);

    /* Hash current file */
    g_autofree gchar *current_hash = snap_hash_file (file_path, NULL);
    if (current_hash == NULL)
        return FALSE;

    return g_strcmp0 (stored_hash, current_hash) != 0;
}

GList *
snap_store_get_tracked_files (SnapStore *self, GError **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), NULL);

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db,
                            "SELECT file_path FROM file_status ORDER BY file_path",
                            -1, &stmt, NULL) != SQLITE_OK)
        return NULL;

    GList *list = NULL;
    while (sqlite3_step (stmt) == SQLITE_ROW)
        list = g_list_prepend (list, g_strdup ((const gchar *) sqlite3_column_text (stmt, 0)));

    sqlite3_finalize (stmt);
    return g_list_reverse (list);
}

/* --- Audit chain --- */

gboolean
snap_store_verify_chain (SnapStore *self, const gchar *file_path, GError **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), FALSE);

    GList *history = snap_store_get_history (self, file_path, "main", -1, error);
    if (history == NULL)
        return TRUE;  /* No history = valid */

    /* Verify each snapshot's hash matches its content */
    gboolean valid = TRUE;
    for (GList *l = history; l != NULL; l = l->next)
    {
        SnapSnapshot *snap = (SnapSnapshot *) l->data;
        g_autofree gchar *content = snap_store_get_snapshot_content (self, snap, NULL);
        if (content == NULL)
        {
            valid = FALSE;
            break;
        }

        g_autofree gchar *computed_hash = snap_hash_string (content);
        if (g_strcmp0 (computed_hash, snap->hash) != 0)
        {
            valid = FALSE;
            break;
        }

        /* Verify chain linkage */
        if (snap->version > 1 && snap->parent_hash != NULL)
        {
            SnapSnapshot *prev = NULL;
            for (GList *p = l->next; p != NULL; p = p->next)
            {
                SnapSnapshot *ps = (SnapSnapshot *) p->data;
                if (ps->version == snap->version - 1)
                {
                    prev = ps;
                    break;
                }
            }
            if (prev != NULL && g_strcmp0 (snap->parent_hash, prev->hash) != 0)
            {
                valid = FALSE;
                break;
            }
        }
    }

    g_list_free_full (history, (GDestroyNotify) snap_snapshot_free);

    if (!valid)
    {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "Audit chain verification failed");
    }

    return valid;
}

GList *
snap_store_get_audit_log (SnapStore *self, const gchar *file_path, GError **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), NULL);

    const gchar *sql = "SELECT action, hash, previous_hash, timestamp "
        "FROM audit_log WHERE file_path = ? ORDER BY timestamp DESC";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return NULL;

    sqlite3_bind_text (stmt, 1, file_path, -1, SQLITE_TRANSIENT);

    GList *list = NULL;
    while (sqlite3_step (stmt) == SQLITE_ROW)
    {
        /* Return audit entries as SnapSnapshot with action in message field */
        SnapSnapshot *entry = g_new0 (SnapSnapshot, 1);
        entry->file_path = g_strdup (file_path);
        entry->message = g_strdup ((const gchar *) sqlite3_column_text (stmt, 0));
        entry->hash = g_strdup ((const gchar *) sqlite3_column_text (stmt, 1));
        entry->parent_hash = g_strdup ((const gchar *) sqlite3_column_text (stmt, 2));
        entry->timestamp = sqlite3_column_int64 (stmt, 3);
        list = g_list_prepend (list, entry);
    }

    sqlite3_finalize (stmt);
    return g_list_reverse (list);
}

/* --- Blame --- */

GList *
snap_store_blame (SnapStore *self, const gchar *file_path, GError **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), NULL);

    /* Get all versions to build blame info */
    return snap_store_get_history (self, file_path, "main", -1, error);
}

/* --- Import --- */

gboolean
snap_store_import_git_history (SnapStore   *self,
                                const gchar *repo_path,
                                const gchar *file_path,
                                GError     **error)
{
    g_return_val_if_fail (SNAP_IS_STORE (self), FALSE);

    /* Build git log command */
    g_autofree gchar *cmd = g_strdup_printf (
        "git -C '%s' log --format='%%H %%s' --follow -- '%s'",
        repo_path, file_path);

    g_autofree gchar *output = NULL;
    gint exit_status;
    if (!g_spawn_command_line_sync (cmd, &output, NULL, &exit_status, error))
        return FALSE;

    if (exit_status != 0)
    {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "git log command failed");
        return FALSE;
    }

    /* Parse commits (newest first) and import in reverse order */
    g_auto (GStrv) lines = g_strsplit (output, "\n", -1);
    gint n_lines = g_strv_length (lines);

    for (gint i = n_lines - 1; i >= 0; i--)
    {
        if (lines[i][0] == '\0')
            continue;

        /* Get file content at this commit */
        gchar *space = strchr (lines[i], ' ');
        if (space == NULL)
            continue;

        *space = '\0';
        const gchar *commit_hash = lines[i];
        const gchar *commit_msg = space + 1;

        g_autofree gchar *show_cmd = g_strdup_printf (
            "git -C '%s' show '%s:%s'",
            repo_path, commit_hash, file_path);

        g_autofree gchar *content = NULL;
        if (g_spawn_command_line_sync (show_cmd, &content, NULL, &exit_status, NULL) &&
            exit_status == 0 && content != NULL)
        {
            snap_store_save_snapshot (self, file_path, content, strlen (content),
                                      commit_msg, "main", NULL);
        }
    }

    return TRUE;
}
