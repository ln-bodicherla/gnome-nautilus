/* snap-encrypt-dialog.h
 *
 * Dialog for passphrase entry and encryption toggle.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_ENCRYPT_DIALOG (snap_encrypt_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapEncryptDialog, snap_encrypt_dialog, SNAP, ENCRYPT_DIALOG, AdwDialog)

void snap_encrypt_dialog_present (const gchar *file_path,
                                  GtkWindow   *parent);

G_END_DECLS
