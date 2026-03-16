/* snap-checksum-dialog.h
 *
 * File checksum verification dialog.
 * Computes MD5, SHA1, SHA256, SHA512 for selected file.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_CHECKSUM_DIALOG (snap_checksum_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapChecksumDialog, snap_checksum_dialog, SNAP, CHECKSUM_DIALOG, AdwDialog)

void snap_checksum_dialog_present (const gchar *path, GtkWindow *parent);

G_END_DECLS
