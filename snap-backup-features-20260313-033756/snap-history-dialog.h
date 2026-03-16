/* snap-history-dialog.h
 *
 * Dialog showing version history for a file.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_HISTORY_DIALOG (snap_history_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapHistoryDialog, snap_history_dialog, SNAP, HISTORY_DIALOG, AdwDialog)

void snap_history_dialog_present (const gchar *file_path,
                                  GtkWindow   *parent);

G_END_DECLS
