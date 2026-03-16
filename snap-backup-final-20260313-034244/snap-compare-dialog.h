/* snap-compare-dialog.h
 *
 * Dialog for comparing two selected files side by side.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_COMPARE_DIALOG (snap_compare_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapCompareDialog, snap_compare_dialog, SNAP, COMPARE_DIALOG, AdwDialog)

void snap_compare_dialog_present (const gchar *path_a,
                                   const gchar *path_b,
                                   GtkWindow   *parent);

G_END_DECLS
