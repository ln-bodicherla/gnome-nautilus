/* snap-save-dialog.h
 *
 * Dialog to save a new snapshot with a message.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_SAVE_DIALOG (snap_save_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapSaveDialog, snap_save_dialog, SNAP, SAVE_DIALOG, AdwDialog)

void snap_save_dialog_present (const gchar *file_path,
                               GtkWindow   *parent);

G_END_DECLS
