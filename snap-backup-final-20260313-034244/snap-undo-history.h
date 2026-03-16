/* snap-undo-history.h
 *
 * Tracks file operations for visual undo/redo history.
 * Displays a list of recent operations that can be undone.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SNAP_TYPE_UNDO_HISTORY (snap_undo_history_get_type ())
G_DECLARE_FINAL_TYPE (SnapUndoHistory, snap_undo_history, SNAP, UNDO_HISTORY, GtkBox)

GtkWidget *snap_undo_history_new  (void);
void       snap_undo_history_add  (SnapUndoHistory *self,
                                    const gchar     *operation,
                                    const gchar     *detail);
void       snap_undo_history_clear (SnapUndoHistory *self);

G_END_DECLS
