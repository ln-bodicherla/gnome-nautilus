/* snap-filter-bar.h
 *
 * Quick filter bar (like Dolphin's Ctrl+I).
 * Filters the current directory view by name as you type.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SNAP_TYPE_FILTER_BAR (snap_filter_bar_get_type ())
G_DECLARE_FINAL_TYPE (SnapFilterBar, snap_filter_bar, SNAP, FILTER_BAR, GtkBox)

GtkWidget  *snap_filter_bar_new        (void);
const gchar*snap_filter_bar_get_text   (SnapFilterBar *self);
void        snap_filter_bar_show       (SnapFilterBar *self);
void        snap_filter_bar_hide       (SnapFilterBar *self);
gboolean    snap_filter_bar_is_active  (SnapFilterBar *self);

G_END_DECLS
