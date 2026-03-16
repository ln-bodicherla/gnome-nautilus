/* snap-manager.h
 *
 * Singleton GObject coordinating all snap operations.
 * Main API that UI dialogs call. Async via GTask.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>
#include "snap-store.h"
#include "snap-diff.h"

G_BEGIN_DECLS

#define SNAP_TYPE_MANAGER (snap_manager_get_type ())
G_DECLARE_FINAL_TYPE (SnapManager, snap_manager, SNAP, MANAGER, GObject)

SnapManager   *snap_manager_get_default      (void);

/* Access to the store (for search provider) */
SnapStore     *snap_manager_get_store       (SnapManager  *self);

/* Initialize the snap system */
gboolean       snap_manager_initialize       (SnapManager  *self,
                                              GError      **error);

/* Core operations - synchronous */
SnapSnapshot  *snap_manager_save             (SnapManager  *self,
                                              const gchar  *file_path,
                                              const gchar  *message,
                                              GError      **error);
gboolean       snap_manager_restore          (SnapManager  *self,
                                              const gchar  *file_path,
                                              gint64        version,
                                              const gchar  *branch,
                                              GError      **error);
GList         *snap_manager_history          (SnapManager  *self,
                                              const gchar  *file_path,
                                              gint          limit,
                                              GError      **error);
SnapDiffResult*snap_manager_diff             (SnapManager  *self,
                                              const gchar  *file_path,
                                              gint64        version1,
                                              gint64        version2,
                                              GError      **error);
SnapDiffResult*snap_manager_diff_current     (SnapManager  *self,
                                              const gchar  *file_path,
                                              GError      **error);
gchar         *snap_manager_show             (SnapManager  *self,
                                              const gchar  *file_path,
                                              gint64        version,
                                              const gchar  *branch,
                                              GError      **error);

/* Async operations */
void           snap_manager_save_async       (SnapManager        *self,
                                              const gchar        *file_path,
                                              const gchar        *message,
                                              GCancellable       *cancellable,
                                              GAsyncReadyCallback callback,
                                              gpointer            user_data);
SnapSnapshot  *snap_manager_save_finish      (SnapManager  *self,
                                              GAsyncResult *result,
                                              GError      **error);
void           snap_manager_restore_async    (SnapManager        *self,
                                              const gchar        *file_path,
                                              gint64              version,
                                              const gchar        *branch,
                                              GCancellable       *cancellable,
                                              GAsyncReadyCallback callback,
                                              gpointer            user_data);
gboolean       snap_manager_restore_finish   (SnapManager  *self,
                                              GAsyncResult *result,
                                              GError      **error);

/* Pin */
gboolean       snap_manager_pin              (SnapManager  *self,
                                              const gchar  *file_path,
                                              gint64        version,
                                              GError      **error);
gboolean       snap_manager_unpin            (SnapManager  *self,
                                              const gchar  *file_path,
                                              gint64        version,
                                              GError      **error);

/* Branch */
SnapBranch    *snap_manager_create_branch    (SnapManager  *self,
                                              const gchar  *file_path,
                                              const gchar  *name,
                                              GError      **error);
GList         *snap_manager_list_branches    (SnapManager  *self,
                                              const gchar  *file_path,
                                              GError      **error);
gboolean       snap_manager_delete_branch    (SnapManager  *self,
                                              const gchar  *file_path,
                                              const gchar  *name,
                                              GError      **error);
gboolean       snap_manager_merge_branch     (SnapManager  *self,
                                              const gchar  *file_path,
                                              const gchar  *source,
                                              const gchar  *target,
                                              GError      **error);

/* Group */
SnapGroup     *snap_manager_group_save       (SnapManager  *self,
                                              const gchar  *name,
                                              const gchar  *message,
                                              GList        *file_paths,
                                              GError      **error);
GList         *snap_manager_group_list       (SnapManager  *self,
                                              GError      **error);
gboolean       snap_manager_group_restore    (SnapManager  *self,
                                              gint64        group_id,
                                              GError      **error);

/* Stash */
gboolean       snap_manager_stash            (SnapManager  *self,
                                              const gchar  *file_path,
                                              const gchar  *message,
                                              GError      **error);
gboolean       snap_manager_stash_pop        (SnapManager  *self,
                                              const gchar  *file_path,
                                              GError      **error);
GList         *snap_manager_stash_list       (SnapManager  *self,
                                              const gchar  *file_path,
                                              GError      **error);

/* Encryption */
gboolean       snap_manager_encrypt          (SnapManager  *self,
                                              const gchar  *file_path,
                                              const gchar  *passphrase,
                                              GError      **error);
gboolean       snap_manager_decrypt          (SnapManager  *self,
                                              const gchar  *file_path,
                                              const gchar  *passphrase,
                                              GError      **error);

/* Maintenance */
gint           snap_manager_gc               (SnapManager  *self,
                                              GError      **error);
gboolean       snap_manager_clean            (SnapManager  *self,
                                              const gchar  *file_path,
                                              GError      **error);
gboolean       snap_manager_repair           (SnapManager  *self,
                                              GError      **error);

/* Stats / Info */
gboolean       snap_manager_is_tracked       (SnapManager  *self,
                                              const gchar  *file_path);
gboolean       snap_manager_is_modified      (SnapManager  *self,
                                              const gchar  *file_path);

/* Audit */
gboolean       snap_manager_verify           (SnapManager  *self,
                                              const gchar  *file_path,
                                              GError      **error);

/* Blame */
GList         *snap_manager_blame            (SnapManager  *self,
                                              const gchar  *file_path,
                                              GError      **error);

/* Log */
GList         *snap_manager_global_log       (SnapManager  *self,
                                              gint          limit,
                                              GError      **error);
GList         *snap_manager_search           (SnapManager  *self,
                                              const gchar  *query,
                                              GError      **error);

/* Import */
void           snap_manager_import_git_async (SnapManager        *self,
                                              const gchar        *repo_path,
                                              const gchar        *file_path,
                                              GCancellable       *cancellable,
                                              GAsyncReadyCallback callback,
                                              gpointer            user_data);
gboolean       snap_manager_import_git_finish(SnapManager  *self,
                                              GAsyncResult *result,
                                              GError      **error);

/* Delete snapshot */
gboolean       snap_manager_delete           (SnapManager  *self,
                                              const gchar  *file_path,
                                              gint64        version,
                                              GError      **error);

/* Watch */
gboolean       snap_manager_watch            (SnapManager  *self,
                                              const gchar  *file_path,
                                              GError      **error);
void           snap_manager_unwatch          (SnapManager  *self,
                                              const gchar  *file_path);

G_END_DECLS
