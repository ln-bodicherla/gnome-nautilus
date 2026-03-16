/* snap-split-view.h
 *
 * Dolphin-style split view (F3) with two directory panes side by side.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SNAP_TYPE_SPLIT_VIEW (snap_split_view_get_type ())
G_DECLARE_FINAL_TYPE (SnapSplitView, snap_split_view, SNAP, SPLIT_VIEW, GtkBox)

GtkWidget *snap_split_view_new          (void);
void       snap_split_view_set_active   (SnapSplitView *self,
                                          gboolean       split_active);
gboolean   snap_split_view_get_active   (SnapSplitView *self);
void       snap_split_view_toggle       (SnapSplitView *self);

/* Set navigation callbacks - when user navigates in the secondary pane */
typedef void (*SnapSplitViewNavigateCb) (const gchar *path, gpointer user_data);
void       snap_split_view_set_navigate_callback (SnapSplitView            *self,
                                                    SnapSplitViewNavigateCb   cb,
                                                    gpointer                  user_data);

/* Set the directory shown in the secondary pane */
void       snap_split_view_set_directory (SnapSplitView *self,
                                           const gchar   *path);
const gchar *snap_split_view_get_directory (SnapSplitView *self);

G_END_DECLS
