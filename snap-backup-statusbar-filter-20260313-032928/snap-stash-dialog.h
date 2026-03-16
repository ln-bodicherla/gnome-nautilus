/* snap-stash-dialog.h
 *
 * Dialog for stash save/pop/list operations.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_STASH_DIALOG (snap_stash_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapStashDialog, snap_stash_dialog, SNAP, STASH_DIALOG, AdwDialog)

void snap_stash_dialog_present (const gchar *file_path,
                                GtkWindow   *parent);

G_END_DECLS
