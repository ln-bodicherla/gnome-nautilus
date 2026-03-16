/* snap-log-dialog.h
 *
 * Global timeline log showing all snapshots across all files.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_LOG_DIALOG (snap_log_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapLogDialog, snap_log_dialog, SNAP, LOG_DIALOG, AdwDialog)

void snap_log_dialog_present (GtkWindow *parent);

G_END_DECLS
