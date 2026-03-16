/* snap-diff-dialog.h
 *
 * Dialog showing diff between current file and latest snapshot.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_DIFF_DIALOG (snap_diff_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapDiffDialog, snap_diff_dialog, SNAP, DIFF_DIALOG, AdwDialog)

void snap_diff_dialog_present (const gchar *file_path,
                               GtkWindow   *parent);

G_END_DECLS
