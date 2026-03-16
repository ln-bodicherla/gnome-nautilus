/* snap-statusbar.h
 *
 * Status bar showing file count, selection info, and free space.
 * Similar to Dolphin's bottom status bar.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SNAP_TYPE_STATUSBAR (snap_statusbar_get_type ())
G_DECLARE_FINAL_TYPE (SnapStatusbar, snap_statusbar, SNAP, STATUSBAR, GtkBox)

GtkWidget *snap_statusbar_new            (void);
void       snap_statusbar_set_file_count (SnapStatusbar *self,
                                           guint          total,
                                           guint          hidden);
void       snap_statusbar_set_selection  (SnapStatusbar *self,
                                           guint          count,
                                           guint64        total_size);
void       snap_statusbar_set_free_space (SnapStatusbar *self,
                                           guint64        free_bytes);
void       snap_statusbar_set_filter_text(SnapStatusbar *self,
                                           const gchar   *text);

G_END_DECLS
