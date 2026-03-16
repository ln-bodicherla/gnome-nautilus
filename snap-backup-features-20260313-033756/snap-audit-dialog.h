/* snap-audit-dialog.h
 *
 * Dialog showing cryptographic chain verification results.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_AUDIT_DIALOG (snap_audit_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapAuditDialog, snap_audit_dialog, SNAP, AUDIT_DIALOG, AdwDialog)

void snap_audit_dialog_present (const gchar *file_path,
                                GtkWindow   *parent);

G_END_DECLS
