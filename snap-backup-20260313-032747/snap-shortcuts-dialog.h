/* snap-shortcuts-dialog.h
 *
 * Keyboard shortcuts reference dialog.
 * Shows all available shortcuts organized by category.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_SHORTCUTS_DIALOG (snap_shortcuts_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapShortcutsDialog, snap_shortcuts_dialog, SNAP, SHORTCUTS_DIALOG, AdwDialog)

void snap_shortcuts_dialog_present (GtkWindow *parent);

G_END_DECLS
