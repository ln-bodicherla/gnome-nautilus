/* snap-sync-dialog.h
 *
 * Dialog for sync target path entry and progress tracking.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_SYNC_DIALOG (snap_sync_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapSyncDialog, snap_sync_dialog, SNAP, SYNC_DIALOG, AdwDialog)

void snap_sync_dialog_present (GtkWindow *parent);

G_END_DECLS
