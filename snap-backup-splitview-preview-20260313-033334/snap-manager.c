/* snap-manager.c
 *
 * Singleton GObject coordinating all snap operations.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-manager.h"
#include "snap-config.h"
#include "snap-hooks.h"
#include "snap-hash.h"
#include "snap-encryption.h"
#include "snap-notify.h"
#include "snap-watcher.h"
#include <string.h>

struct _SnapManager
{
    GObject       parent_instance;
    SnapStore    *store;
    SnapConfig   *config;
    SnapWatcher  *watcher;
    gboolean      initialized;
};

G_DEFINE_FINAL_TYPE (SnapManager, snap_manager, G_TYPE_OBJECT)

static SnapManager *default_manager = NULL;

static void
snap_manager_finalize (GObject *object)
{
    SnapManager *self = SNAP_MANAGER (object);
    g_clear_object (&self->watcher);
    G_OBJECT_CLASS (snap_manager_parent_class)->finalize (object);
}

static void
snap_manager_class_init (SnapManagerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = snap_manager_finalize;
}

static void
snap_manager_init (SnapManager *self)
{
    self->store = snap_store_get_default ();
    self->config = snap_config_get_default ();
    self->watcher = snap_watcher_new ();
    self->initialized = FALSE;
}

SnapManager *
snap_manager_get_default (void)
{
    if (default_manager == NULL)
        default_manager = g_object_new (SNAP_TYPE_MANAGER, NULL);
    return default_manager;
}

SnapStore *
snap_manager_get_store (SnapManager *self)
{
    g_return_val_if_fail (SNAP_IS_MANAGER (self), NULL);

    if (!self->initialized)
        snap_manager_initialize (self, NULL);

    return self->store;
}

gboolean
snap_manager_initialize (SnapManager *self, GError **error)
{
    g_return_val_if_fail (SNAP_IS_MANAGER (self), FALSE);

    if (self->initialized)
        return TRUE;

    if (!snap_store_open (self->store, error))
        return FALSE;

    self->initialized = TRUE;
    return TRUE;
}

/* --- Core operations --- */

SnapSnapshot *
snap_manager_save (SnapManager *self,
                   const gchar *file_path,
                   const gchar *message,
                   GError     **error)
{
    g_return_val_if_fail (SNAP_IS_MANAGER (self), NULL);

    if (!snap_manager_initialize (self, error))
        return NULL;

    /* Run pre-save hooks */
    snap_hooks_run (SNAP_HOOK_EVENT_PRE_SAVE, file_path, message, NULL, NULL);

    /* Read file content */
    g_autofree gchar *content = NULL;
    gsize length;
    if (!g_file_get_contents (file_path, &content, &length, error))
        return NULL;

    /* Get branch from config */
    const gchar *branch = snap_config_get_default_branch (self->config);

    /* Save snapshot */
    SnapSnapshot *snapshot = snap_store_save_snapshot (self->store, file_path,
                                                       content, length,
                                                       message, branch, error);
    if (snapshot == NULL)
        return NULL;

    /* Run post-save hooks */
    snap_hooks_run (SNAP_HOOK_EVENT_POST_SAVE, file_path, message, NULL, NULL);

    /* Notify if enabled */
    if (snap_config_get_notifications (self->config))
        snap_notify_saved (file_path, snapshot->version);

    return snapshot;
}

gboolean
snap_manager_restore (SnapManager *self,
                      const gchar *file_path,
                      gint64       version,
                      const gchar *branch,
                      GError     **error)
{
    g_return_val_if_fail (SNAP_IS_MANAGER (self), FALSE);

    if (!snap_manager_initialize (self, error))
        return FALSE;

    snap_hooks_run (SNAP_HOOK_EVENT_PRE_RESTORE, file_path, NULL, NULL, NULL);

    const gchar *use_branch = branch ? branch :
        snap_config_get_default_branch (self->config);

    g_autoptr (SnapSnapshot) snapshot = snap_store_get_snapshot (self->store,
                                                                  file_path, version,
                                                                  use_branch, error);
    if (snapshot == NULL)
    {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                     "Snapshot v%ld not found", (long) version);
        return FALSE;
    }

    g_autofree gchar *content = snap_store_get_snapshot_content (self->store,
                                                                  snapshot, error);
    if (content == NULL)
        return FALSE;

    if (!g_file_set_contents (file_path, content, -1, error))
        return FALSE;

    snap_hooks_run (SNAP_HOOK_EVENT_POST_RESTORE, file_path, NULL, NULL, NULL);

    if (snap_config_get_notifications (self->config))
        snap_notify_restored (file_path, version);

    return TRUE;
}

GList *
snap_manager_history (SnapManager *self,
                      const gchar *file_path,
                      gint         limit,
                      GError     **error)
{
    g_return_val_if_fail (SNAP_IS_MANAGER (self), NULL);

    if (!snap_manager_initialize (self, error))
        return NULL;

    const gchar *branch = snap_config_get_default_branch (self->config);
    return snap_store_get_history (self->store, file_path, branch, limit, error);
}

SnapDiffResult *
snap_manager_diff (SnapManager *self,
                   const gchar *file_path,
                   gint64       version1,
                   gint64       version2,
                   GError     **error)
{
    g_return_val_if_fail (SNAP_IS_MANAGER (self), NULL);

    if (!snap_manager_initialize (self, error))
        return NULL;

    const gchar *branch = snap_config_get_default_branch (self->config);

    g_autoptr (SnapSnapshot) snap1 = snap_store_get_snapshot (self->store,
                                                               file_path, version1,
                                                               branch, error);
    if (snap1 == NULL)
        return NULL;

    g_autoptr (SnapSnapshot) snap2 = snap_store_get_snapshot (self->store,
                                                               file_path, version2,
                                                               branch, error);
    if (snap2 == NULL)
        return NULL;

    g_autofree gchar *content1 = snap_store_get_snapshot_content (self->store, snap1, error);
    if (content1 == NULL)
        return NULL;

    g_autofree gchar *content2 = snap_store_get_snapshot_content (self->store, snap2, error);
    if (content2 == NULL)
        return NULL;

    return snap_diff_compute (content1, content2, 3);
}

SnapDiffResult *
snap_manager_diff_current (SnapManager *self,
                           const gchar *file_path,
                           GError     **error)
{
    g_return_val_if_fail (SNAP_IS_MANAGER (self), NULL);

    if (!snap_manager_initialize (self, error))
        return NULL;

    const gchar *branch = snap_config_get_default_branch (self->config);
    gint64 latest = snap_store_get_latest_version (self->store, file_path, branch);

    if (latest == 0)
    {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                             "No snapshots found for this file");
        return NULL;
    }

    g_autoptr (SnapSnapshot) snap = snap_store_get_snapshot (self->store,
                                                              file_path, latest,
                                                              branch, error);
    if (snap == NULL)
        return NULL;

    g_autofree gchar *old_content = snap_store_get_snapshot_content (self->store, snap, error);
    if (old_content == NULL)
        return NULL;

    g_autofree gchar *new_content = NULL;
    if (!g_file_get_contents (file_path, &new_content, NULL, error))
        return NULL;

    return snap_diff_compute (old_content, new_content, 3);
}

gchar *
snap_manager_show (SnapManager *self,
                   const gchar *file_path,
                   gint64       version,
                   const gchar *branch,
                   GError     **error)
{
    g_return_val_if_fail (SNAP_IS_MANAGER (self), NULL);

    if (!snap_manager_initialize (self, error))
        return NULL;

    const gchar *use_branch = branch ? branch :
        snap_config_get_default_branch (self->config);

    g_autoptr (SnapSnapshot) snap = snap_store_get_snapshot (self->store,
                                                              file_path, version,
                                                              use_branch, error);
    if (snap == NULL)
        return NULL;

    return snap_store_get_snapshot_content (self->store, snap, error);
}

/* --- Async operations --- */

static void
save_thread_func (GTask        *task,
                  gpointer      source_object,
                  gpointer      task_data,
                  GCancellable *cancellable)
{
    SnapManager *self = SNAP_MANAGER (source_object);
    gchar **params = (gchar **) task_data;
    GError *error = NULL;

    SnapSnapshot *snapshot = snap_manager_save (self, params[0], params[1], &error);
    if (snapshot != NULL)
        g_task_return_pointer (task, snapshot, (GDestroyNotify) snap_snapshot_free);
    else
        g_task_return_error (task, error);
}

void
snap_manager_save_async (SnapManager        *self,
                         const gchar        *file_path,
                         const gchar        *message,
                         GCancellable       *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer            user_data)
{
    GTask *task = g_task_new (self, cancellable, callback, user_data);

    gchar **params = g_new0 (gchar *, 3);
    params[0] = g_strdup (file_path);
    params[1] = g_strdup (message);
    g_task_set_task_data (task, params, (GDestroyNotify) g_strfreev);

    g_task_run_in_thread (task, save_thread_func);
    g_object_unref (task);
}

SnapSnapshot *
snap_manager_save_finish (SnapManager *self, GAsyncResult *result, GError **error)
{
    return g_task_propagate_pointer (G_TASK (result), error);
}

static void
restore_thread_func (GTask        *task,
                     gpointer      source_object,
                     gpointer      task_data,
                     GCancellable *cancellable)
{
    SnapManager *self = SNAP_MANAGER (source_object);
    gchar **params = (gchar **) task_data;
    GError *error = NULL;

    gint64 version = g_ascii_strtoll (params[1], NULL, 10);
    gboolean ok = snap_manager_restore (self, params[0], version, params[2], &error);

    if (ok)
        g_task_return_boolean (task, TRUE);
    else
        g_task_return_error (task, error);
}

void
snap_manager_restore_async (SnapManager        *self,
                            const gchar        *file_path,
                            gint64              version,
                            const gchar        *branch,
                            GCancellable       *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer            user_data)
{
    GTask *task = g_task_new (self, cancellable, callback, user_data);

    gchar **params = g_new0 (gchar *, 4);
    params[0] = g_strdup (file_path);
    params[1] = g_strdup_printf ("%ld", (long) version);
    params[2] = g_strdup (branch);
    g_task_set_task_data (task, params, (GDestroyNotify) g_strfreev);

    g_task_run_in_thread (task, restore_thread_func);
    g_object_unref (task);
}

gboolean
snap_manager_restore_finish (SnapManager *self, GAsyncResult *result, GError **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

/* --- Pin --- */

gboolean
snap_manager_pin (SnapManager *self, const gchar *file_path,
                  gint64 version, GError **error)
{
    if (!snap_manager_initialize (self, error))
        return FALSE;
    return snap_store_pin_snapshot (self->store, file_path, version, NULL, error);
}

gboolean
snap_manager_unpin (SnapManager *self, const gchar *file_path,
                    gint64 version, GError **error)
{
    if (!snap_manager_initialize (self, error))
        return FALSE;
    return snap_store_unpin_snapshot (self->store, file_path, version, NULL, error);
}

/* --- Branch --- */

SnapBranch *
snap_manager_create_branch (SnapManager *self, const gchar *file_path,
                            const gchar *name, GError **error)
{
    if (!snap_manager_initialize (self, error))
        return NULL;

    snap_hooks_run (SNAP_HOOK_EVENT_PRE_BRANCH, file_path, name, NULL, NULL);

    const gchar *branch = snap_config_get_default_branch (self->config);
    gint64 ver = snap_store_get_latest_version (self->store, file_path, branch);

    SnapBranch *result = snap_store_create_branch (self->store, file_path,
                                                     name, branch, ver, error);

    if (result != NULL)
        snap_hooks_run (SNAP_HOOK_EVENT_POST_BRANCH, file_path, name, NULL, NULL);

    return result;
}

GList *
snap_manager_list_branches (SnapManager *self, const gchar *file_path,
                            GError **error)
{
    if (!snap_manager_initialize (self, error))
        return NULL;
    return snap_store_list_branches (self->store, file_path, error);
}

gboolean
snap_manager_delete_branch (SnapManager *self, const gchar *file_path,
                            const gchar *name, GError **error)
{
    if (!snap_manager_initialize (self, error))
        return FALSE;
    return snap_store_delete_branch (self->store, file_path, name, error);
}

gboolean
snap_manager_merge_branch (SnapManager *self, const gchar *file_path,
                           const gchar *source, const gchar *target,
                           GError **error)
{
    if (!snap_manager_initialize (self, error))
        return FALSE;

    snap_hooks_run (SNAP_HOOK_EVENT_PRE_MERGE, file_path, source, NULL, NULL);
    gboolean ok = snap_store_merge_branch (self->store, file_path, source, target, error);
    if (ok)
        snap_hooks_run (SNAP_HOOK_EVENT_POST_MERGE, file_path, source, NULL, NULL);

    return ok;
}

/* --- Group --- */

SnapGroup *
snap_manager_group_save (SnapManager *self, const gchar *name,
                         const gchar *message, GList *file_paths,
                         GError **error)
{
    if (!snap_manager_initialize (self, error))
        return NULL;
    return snap_store_create_group (self->store, name, message, file_paths, error);
}

GList *
snap_manager_group_list (SnapManager *self, GError **error)
{
    if (!snap_manager_initialize (self, error))
        return NULL;
    return snap_store_list_groups (self->store, error);
}

gboolean
snap_manager_group_restore (SnapManager *self, gint64 group_id, GError **error)
{
    if (!snap_manager_initialize (self, error))
        return FALSE;
    return snap_store_restore_group (self->store, group_id, error);
}

/* --- Stash --- */

gboolean
snap_manager_stash (SnapManager *self, const gchar *file_path,
                    const gchar *message, GError **error)
{
    if (!snap_manager_initialize (self, error))
        return FALSE;
    g_autoptr (SnapStash) stash = snap_store_stash_save (self->store, file_path,
                                                          message, error);
    return stash != NULL;
}

gboolean
snap_manager_stash_pop (SnapManager *self, const gchar *file_path, GError **error)
{
    if (!snap_manager_initialize (self, error))
        return FALSE;
    g_autoptr (SnapStash) stash = snap_store_stash_pop (self->store, file_path, error);
    return stash != NULL;
}

GList *
snap_manager_stash_list (SnapManager *self, const gchar *file_path, GError **error)
{
    if (!snap_manager_initialize (self, error))
        return NULL;
    return snap_store_stash_list (self->store, file_path, error);
}

/* --- Encryption --- */

gboolean
snap_manager_encrypt (SnapManager *self, const gchar *file_path,
                      const gchar *passphrase, GError **error)
{
    if (!snap_manager_initialize (self, error))
        return FALSE;

    /* Mark file as encrypted in config */
    snap_config_set_encryption (self->config, TRUE);
    return snap_config_save (self->config, error);
}

gboolean
snap_manager_decrypt (SnapManager *self, const gchar *file_path,
                      const gchar *passphrase, GError **error)
{
    if (!snap_manager_initialize (self, error))
        return FALSE;

    snap_config_set_encryption (self->config, FALSE);
    return snap_config_save (self->config, error);
}

/* --- Maintenance --- */

gint
snap_manager_gc (SnapManager *self, GError **error)
{
    if (!snap_manager_initialize (self, error))
        return 0;
    return snap_store_gc (self->store, snap_config_get_gc_keep (self->config), error);
}

gboolean
snap_manager_clean (SnapManager *self, const gchar *file_path, GError **error)
{
    if (!snap_manager_initialize (self, error))
        return FALSE;
    return snap_store_clean (self->store, file_path, error);
}

gboolean
snap_manager_repair (SnapManager *self, GError **error)
{
    if (!snap_manager_initialize (self, error))
        return FALSE;
    return snap_store_repair (self->store, error);
}

/* --- Status --- */

gboolean
snap_manager_is_tracked (SnapManager *self, const gchar *file_path)
{
    if (!snap_manager_initialize (self, NULL))
        return FALSE;
    return snap_store_is_tracked (self->store, file_path);
}

gboolean
snap_manager_is_modified (SnapManager *self, const gchar *file_path)
{
    if (!snap_manager_initialize (self, NULL))
        return FALSE;
    return snap_store_is_modified (self->store, file_path);
}

/* --- Audit --- */

gboolean
snap_manager_verify (SnapManager *self, const gchar *file_path, GError **error)
{
    if (!snap_manager_initialize (self, error))
        return FALSE;
    return snap_store_verify_chain (self->store, file_path, error);
}

/* --- Blame --- */

GList *
snap_manager_blame (SnapManager *self, const gchar *file_path, GError **error)
{
    if (!snap_manager_initialize (self, error))
        return NULL;
    return snap_store_blame (self->store, file_path, error);
}

/* --- Log --- */

GList *
snap_manager_global_log (SnapManager *self, gint limit, GError **error)
{
    if (!snap_manager_initialize (self, error))
        return NULL;
    return snap_store_get_global_log (self->store, limit, error);
}

GList *
snap_manager_search (SnapManager *self, const gchar *query, GError **error)
{
    if (!snap_manager_initialize (self, error))
        return NULL;
    return snap_store_search_snapshots (self->store, query, error);
}

/* --- Import --- */

static void
import_git_thread_func (GTask        *task,
                        gpointer      source_object,
                        gpointer      task_data,
                        GCancellable *cancellable)
{
    SnapManager *self = SNAP_MANAGER (source_object);
    gchar **params = (gchar **) task_data;
    GError *error = NULL;

    if (!snap_manager_initialize (self, &error))
    {
        g_task_return_error (task, error);
        return;
    }

    gboolean ok = snap_store_import_git_history (self->store, params[0], params[1], &error);
    if (ok)
        g_task_return_boolean (task, TRUE);
    else
        g_task_return_error (task, error);
}

void
snap_manager_import_git_async (SnapManager        *self,
                               const gchar        *repo_path,
                               const gchar        *file_path,
                               GCancellable       *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer            user_data)
{
    GTask *task = g_task_new (self, cancellable, callback, user_data);

    gchar **params = g_new0 (gchar *, 3);
    params[0] = g_strdup (repo_path);
    params[1] = g_strdup (file_path);
    g_task_set_task_data (task, params, (GDestroyNotify) g_strfreev);

    g_task_run_in_thread (task, import_git_thread_func);
    g_object_unref (task);
}

gboolean
snap_manager_import_git_finish (SnapManager *self, GAsyncResult *result, GError **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

/* --- Delete --- */

gboolean
snap_manager_delete (SnapManager *self, const gchar *file_path,
                     gint64 version, GError **error)
{
    if (!snap_manager_initialize (self, error))
        return FALSE;

    snap_hooks_run (SNAP_HOOK_EVENT_PRE_DELETE, file_path, NULL, NULL, NULL);
    gboolean ok = snap_store_delete_snapshot (self->store, file_path, version, NULL, error);
    if (ok)
        snap_hooks_run (SNAP_HOOK_EVENT_POST_DELETE, file_path, NULL, NULL, NULL);

    return ok;
}

/* --- Watch --- */

gboolean
snap_manager_watch (SnapManager *self, const gchar *file_path, GError **error)
{
    return snap_watcher_watch (self->watcher, file_path, error);
}

void
snap_manager_unwatch (SnapManager *self, const gchar *file_path)
{
    snap_watcher_unwatch (self->watcher, file_path);
}
