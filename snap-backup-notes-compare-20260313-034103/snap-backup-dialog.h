/* snap-backup-dialog.h
 *
 * Dialog for backup create and import with file picker.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_BACKUP_DIALOG (snap_backup_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapBackupDialog, snap_backup_dialog, SNAP, BACKUP_DIALOG, AdwDialog)

void snap_backup_dialog_present (GtkWindow *parent);

G_END_DECLS
