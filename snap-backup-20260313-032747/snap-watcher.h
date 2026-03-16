/* snap-watcher.h
 *
 * File monitoring via GFileMonitor. Debounce via g_timeout_add().
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define SNAP_TYPE_WATCHER (snap_watcher_get_type ())
G_DECLARE_FINAL_TYPE (SnapWatcher, snap_watcher, SNAP, WATCHER, GObject)

SnapWatcher   *snap_watcher_new              (void);

/* Watch a file for changes (auto-saves on modify after debounce) */
gboolean       snap_watcher_watch            (SnapWatcher  *self,
                                              const gchar  *file_path,
                                              GError      **error);

/* Stop watching a file */
void           snap_watcher_unwatch          (SnapWatcher  *self,
                                              const gchar  *file_path);

/* Stop watching all files */
void           snap_watcher_unwatch_all      (SnapWatcher  *self);

/* Check if a file is being watched */
gboolean       snap_watcher_is_watching      (SnapWatcher  *self,
                                              const gchar  *file_path);

/* Get list of watched paths */
GList         *snap_watcher_get_watched      (SnapWatcher  *self);

/* Set debounce interval in milliseconds */
void           snap_watcher_set_debounce     (SnapWatcher  *self,
                                              guint         ms);

G_END_DECLS
