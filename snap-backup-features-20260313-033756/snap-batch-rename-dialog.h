/* snap-batch-rename-dialog.h
 *
 * Batch rename dialog for multiple files.
 * Supports pattern-based renaming, find/replace, numbering.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_BATCH_RENAME_DIALOG (snap_batch_rename_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapBatchRenameDialog, snap_batch_rename_dialog, SNAP, BATCH_RENAME_DIALOG, AdwDialog)

void snap_batch_rename_dialog_present (GList *paths, GtkWindow *parent);

G_END_DECLS
