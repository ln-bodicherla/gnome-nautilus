/* snap-gc-dialog.h
 *
 * Alert dialog for GC/Clean/Repair confirmation and results.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_GC_DIALOG (snap_gc_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapGcDialog, snap_gc_dialog, SNAP, GC_DIALOG, AdwAlertDialog)

void snap_gc_dialog_present (GtkWindow *parent);

G_END_DECLS
