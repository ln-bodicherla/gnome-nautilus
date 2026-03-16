/* snap-blame-dialog.h
 *
 * Dialog for line-level version attribution view.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_BLAME_DIALOG (snap_blame_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapBlameDialog, snap_blame_dialog, SNAP, BLAME_DIALOG, AdwDialog)

void snap_blame_dialog_present (const gchar *file_path,
                                GtkWindow   *parent);

G_END_DECLS
