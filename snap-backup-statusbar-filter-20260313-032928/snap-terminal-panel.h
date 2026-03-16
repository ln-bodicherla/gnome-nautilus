/* snap-terminal-panel.h
 *
 * Embedded terminal panel (like Dolphin's F4 terminal).
 * Uses VTE or falls back to a simple command entry.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SNAP_TYPE_TERMINAL_PANEL (snap_terminal_panel_get_type ())
G_DECLARE_FINAL_TYPE (SnapTerminalPanel, snap_terminal_panel, SNAP, TERMINAL_PANEL, GtkBox)

GtkWidget *snap_terminal_panel_new         (void);
void       snap_terminal_panel_set_directory (SnapTerminalPanel *self,
                                               const gchar       *directory);

G_END_DECLS
