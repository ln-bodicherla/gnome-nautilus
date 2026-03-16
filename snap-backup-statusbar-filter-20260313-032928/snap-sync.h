/* snap-sync.h
 *
 * Sync/backup to external paths.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define SNAP_TYPE_SYNC (snap_sync_get_type ())
G_DECLARE_FINAL_TYPE (SnapSync, snap_sync, SNAP, SYNC, GObject)

SnapSync      *snap_sync_new                 (void);

/* Sync store to external path */
void           snap_sync_to_path_async       (SnapSync           *self,
                                              const gchar        *target_path,
                                              GCancellable       *cancellable,
                                              GAsyncReadyCallback callback,
                                              gpointer            user_data);
gboolean       snap_sync_to_path_finish      (SnapSync     *self,
                                              GAsyncResult *result,
                                              GError      **error);

/* Create backup archive */
void           snap_sync_backup_async        (SnapSync           *self,
                                              const gchar        *output_path,
                                              GCancellable       *cancellable,
                                              GAsyncReadyCallback callback,
                                              gpointer            user_data);
gboolean       snap_sync_backup_finish       (SnapSync     *self,
                                              GAsyncResult *result,
                                              GError      **error);

/* Import from backup archive */
void           snap_sync_import_async        (SnapSync           *self,
                                              const gchar        *archive_path,
                                              GCancellable       *cancellable,
                                              GAsyncReadyCallback callback,
                                              gpointer            user_data);
gboolean       snap_sync_import_finish       (SnapSync     *self,
                                              GAsyncResult *result,
                                              GError      **error);

/* Get sync progress (0.0 - 1.0) */
gdouble        snap_sync_get_progress        (SnapSync     *self);

G_END_DECLS
