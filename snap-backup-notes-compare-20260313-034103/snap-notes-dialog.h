/* snap-notes-dialog.h
 *
 * Dialog for editing file notes and tags.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_NOTES_DIALOG (snap_notes_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapNotesDialog, snap_notes_dialog, SNAP, NOTES_DIALOG, AdwDialog)

void snap_notes_dialog_present (const gchar *path, GtkWindow *parent);

G_END_DECLS
