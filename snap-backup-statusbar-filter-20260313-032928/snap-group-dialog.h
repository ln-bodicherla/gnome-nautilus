/* snap-group-dialog.h
 *
 * Dialog for multi-file group save with name and message.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_GROUP_DIALOG (snap_group_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapGroupDialog, snap_group_dialog, SNAP, GROUP_DIALOG, AdwDialog)

void snap_group_dialog_present (GList     *file_paths,
                                GtkWindow *parent);

G_END_DECLS
