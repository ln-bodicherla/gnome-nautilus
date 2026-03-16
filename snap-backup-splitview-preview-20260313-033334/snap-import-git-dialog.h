/* snap-import-git-dialog.h
 *
 * Dialog to import git history with repo path entry and progress.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_IMPORT_GIT_DIALOG (snap_import_git_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapImportGitDialog, snap_import_git_dialog, SNAP, IMPORT_GIT_DIALOG, AdwDialog)

void snap_import_git_dialog_present (GtkWindow *parent);

G_END_DECLS
