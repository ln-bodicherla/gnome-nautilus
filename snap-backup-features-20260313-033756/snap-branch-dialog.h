/* snap-branch-dialog.h
 *
 * Dialog for branch create/list/delete/merge operations.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_BRANCH_DIALOG (snap_branch_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapBranchDialog, snap_branch_dialog, SNAP, BRANCH_DIALOG, AdwDialog)

void snap_branch_dialog_present (const gchar *file_path,
                                 GtkWindow   *parent);

G_END_DECLS
