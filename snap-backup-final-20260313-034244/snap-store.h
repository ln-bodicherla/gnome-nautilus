/* snap-store.h
 *
 * SQLite database layer for Snap versioning system.
 * Port of SnapStore.swift - manages 8 tables at ~/.snapstore/
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>
#include <sqlite3.h>

G_BEGIN_DECLS

#define SNAP_TYPE_STORE (snap_store_get_type ())
G_DECLARE_FINAL_TYPE (SnapStore, snap_store, SNAP, STORE, GObject)

/* Snapshot metadata */
typedef struct _SnapSnapshot SnapSnapshot;
struct _SnapSnapshot
{
    gint64      id;
    gchar      *file_path;
    gchar      *message;
    gchar      *hash;
    gint64      version;
    gint64      timestamp;
    gint64      size;
    gchar      *branch;
    gboolean    pinned;
    gboolean    encrypted;
    gchar      *parent_hash;
    gchar      *blob_path;
};

/* Branch metadata */
typedef struct _SnapBranch SnapBranch;
struct _SnapBranch
{
    gint64  id;
    gchar  *name;
    gchar  *file_path;
    gint64  created_at;
    gchar  *parent_branch;
    gint64  fork_version;
};

/* Group metadata */
typedef struct _SnapGroup SnapGroup;
struct _SnapGroup
{
    gint64  id;
    gchar  *name;
    gchar  *message;
    gint64  timestamp;
};

/* Stash entry */
typedef struct _SnapStash SnapStash;
struct _SnapStash
{
    gint64  id;
    gchar  *file_path;
    gchar  *message;
    gchar  *content_hash;
    gint64  timestamp;
};

/* Hook entry */
typedef struct _SnapHook SnapHook;
struct _SnapHook
{
    gint64  id;
    gchar  *name;
    gchar  *event;
    gchar  *script_path;
    gboolean enabled;
};

void           snap_snapshot_free            (SnapSnapshot *snapshot);
void           snap_branch_free              (SnapBranch *branch);
void           snap_group_free               (SnapGroup *group);
void           snap_stash_free               (SnapStash *stash);
void           snap_hook_free                (SnapHook *hook);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (SnapSnapshot, snap_snapshot_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (SnapBranch, snap_branch_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (SnapGroup, snap_group_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (SnapStash, snap_stash_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (SnapHook, snap_hook_free)

/* Singleton access */
SnapStore     *snap_store_get_default        (void);

/* Database lifecycle */
gboolean       snap_store_open               (SnapStore    *self,
                                              GError      **error);
void           snap_store_close              (SnapStore    *self);

/* Object store (zlib compressed blobs) */
gchar         *snap_store_write_blob         (SnapStore    *self,
                                              const gchar  *content,
                                              gsize         length,
                                              GError      **error);
gchar         *snap_store_read_blob          (SnapStore    *self,
                                              const gchar  *hash,
                                              gsize        *out_length,
                                              GError      **error);
gboolean       snap_store_delete_blob        (SnapStore    *self,
                                              const gchar  *hash,
                                              GError      **error);

/* Snapshot CRUD */
SnapSnapshot  *snap_store_save_snapshot      (SnapStore    *self,
                                              const gchar  *file_path,
                                              const gchar  *content,
                                              gsize         length,
                                              const gchar  *message,
                                              const gchar  *branch,
                                              GError      **error);
SnapSnapshot  *snap_store_get_snapshot       (SnapStore    *self,
                                              const gchar  *file_path,
                                              gint64        version,
                                              const gchar  *branch,
                                              GError      **error);
GList         *snap_store_get_history        (SnapStore    *self,
                                              const gchar  *file_path,
                                              const gchar  *branch,
                                              gint          limit,
                                              GError      **error);
gchar         *snap_store_get_snapshot_content(SnapStore    *self,
                                              SnapSnapshot *snapshot,
                                              GError      **error);
gboolean       snap_store_delete_snapshot    (SnapStore    *self,
                                              const gchar  *file_path,
                                              gint64        version,
                                              const gchar  *branch,
                                              GError      **error);
gint64         snap_store_get_latest_version (SnapStore    *self,
                                              const gchar  *file_path,
                                              const gchar  *branch);
gint64         snap_store_get_snapshot_count (SnapStore    *self,
                                              const gchar  *file_path);

/* Pin management */
gboolean       snap_store_pin_snapshot       (SnapStore    *self,
                                              const gchar  *file_path,
                                              gint64        version,
                                              const gchar  *branch,
                                              GError      **error);
gboolean       snap_store_unpin_snapshot     (SnapStore    *self,
                                              const gchar  *file_path,
                                              gint64        version,
                                              const gchar  *branch,
                                              GError      **error);
GList         *snap_store_get_pinned         (SnapStore    *self,
                                              const gchar  *file_path,
                                              GError      **error);

/* Branch operations */
SnapBranch    *snap_store_create_branch      (SnapStore    *self,
                                              const gchar  *file_path,
                                              const gchar  *name,
                                              const gchar  *parent_branch,
                                              gint64        fork_version,
                                              GError      **error);
GList         *snap_store_list_branches      (SnapStore    *self,
                                              const gchar  *file_path,
                                              GError      **error);
gboolean       snap_store_delete_branch      (SnapStore    *self,
                                              const gchar  *file_path,
                                              const gchar  *name,
                                              GError      **error);
gboolean       snap_store_merge_branch       (SnapStore    *self,
                                              const gchar  *file_path,
                                              const gchar  *source,
                                              const gchar  *target,
                                              GError      **error);

/* Group operations */
SnapGroup     *snap_store_create_group       (SnapStore    *self,
                                              const gchar  *name,
                                              const gchar  *message,
                                              GList        *file_paths,
                                              GError      **error);
GList         *snap_store_list_groups        (SnapStore    *self,
                                              GError      **error);
GList         *snap_store_get_group_files    (SnapStore    *self,
                                              gint64        group_id,
                                              GError      **error);
gboolean       snap_store_restore_group      (SnapStore    *self,
                                              gint64        group_id,
                                              GError      **error);

/* Stash operations */
SnapStash     *snap_store_stash_save         (SnapStore    *self,
                                              const gchar  *file_path,
                                              const gchar  *message,
                                              GError      **error);
SnapStash     *snap_store_stash_pop          (SnapStore    *self,
                                              const gchar  *file_path,
                                              GError      **error);
GList         *snap_store_stash_list         (SnapStore    *self,
                                              const gchar  *file_path,
                                              GError      **error);

/* Hook operations */
gboolean       snap_store_add_hook           (SnapStore    *self,
                                              const gchar  *name,
                                              const gchar  *event,
                                              const gchar  *script_path,
                                              GError      **error);
gboolean       snap_store_remove_hook        (SnapStore    *self,
                                              const gchar  *name,
                                              GError      **error);
GList         *snap_store_list_hooks         (SnapStore    *self,
                                              GError      **error);

/* Log / timeline */
GList         *snap_store_get_global_log     (SnapStore    *self,
                                              gint          limit,
                                              GError      **error);
GList         *snap_store_search_snapshots   (SnapStore    *self,
                                              const gchar  *query,
                                              GError      **error);

/* Maintenance */
gint           snap_store_gc                 (SnapStore    *self,
                                              gint          keep_versions,
                                              GError      **error);
gboolean       snap_store_clean              (SnapStore    *self,
                                              const gchar  *file_path,
                                              GError      **error);
gboolean       snap_store_repair             (SnapStore    *self,
                                              GError      **error);

/* Stats */
gint64         snap_store_get_total_snapshots(SnapStore    *self);
gint64         snap_store_get_total_files    (SnapStore    *self);
gint64         snap_store_get_store_size     (SnapStore    *self);

/* File status */
gboolean       snap_store_is_tracked         (SnapStore    *self,
                                              const gchar  *file_path);
gboolean       snap_store_is_modified        (SnapStore    *self,
                                              const gchar  *file_path);
GList         *snap_store_get_tracked_files  (SnapStore    *self,
                                              GError      **error);

/* Audit chain */
gboolean       snap_store_verify_chain       (SnapStore    *self,
                                              const gchar  *file_path,
                                              GError      **error);
GList         *snap_store_get_audit_log      (SnapStore    *self,
                                              const gchar  *file_path,
                                              GError      **error);

/* Blame */
GList         *snap_store_blame              (SnapStore    *self,
                                              const gchar  *file_path,
                                              GError      **error);

/* Import */
gboolean       snap_store_import_git_history (SnapStore    *self,
                                              const gchar  *repo_path,
                                              const gchar  *file_path,
                                              GError      **error);

G_END_DECLS
