/* snap-hook-dialog.h
 *
 * Dialog for hook install/remove/list management.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_HOOK_DIALOG (snap_hook_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapHookDialog, snap_hook_dialog, SNAP, HOOK_DIALOG, AdwDialog)

void snap_hook_dialog_present (GtkWindow *parent);

G_END_DECLS
