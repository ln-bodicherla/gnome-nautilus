/* snap-browse-dialog.h
 *
 * Interactive timeline browser for snapshot versions.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_BROWSE_DIALOG (snap_browse_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapBrowseDialog, snap_browse_dialog, SNAP, BROWSE_DIALOG, AdwDialog)

void snap_browse_dialog_present (const gchar *file_path,
                                 GtkWindow   *parent);

G_END_DECLS
