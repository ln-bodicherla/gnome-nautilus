/* snap-actions.h
 *
 * All GActionEntry arrays and callback stubs for snap integration.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* View-level actions (operate on selected files) */
void  snap_actions_register_view    (GSimpleActionGroup *action_group,
                                     gpointer            view);

/* Window-level actions */
void  snap_actions_register_window  (GActionMap *action_map,
                                     gpointer    window);

/* App-level actions and keyboard accelerators */
void  snap_actions_register_app     (GApplication *app);

/* Get the selected file path from a NautilusFilesView */
gchar *snap_actions_get_selected_path (gpointer view);

/* Get all selected file paths */
GList *snap_actions_get_selected_paths (gpointer view);

G_END_DECLS
