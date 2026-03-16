/* snap-restore-dialog.h
 *
 * Confirmation dialog for restoring a snapshot.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_RESTORE_DIALOG (snap_restore_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapRestoreDialog, snap_restore_dialog, SNAP, RESTORE_DIALOG, AdwAlertDialog)

void snap_restore_dialog_present (const gchar *file_path,
                                  GtkWindow   *parent);

G_END_DECLS
