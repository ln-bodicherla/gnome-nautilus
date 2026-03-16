/* snap-config-dialog.h
 *
 * Preferences dialog exposing all snap-config options.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_CONFIG_DIALOG (snap_config_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapConfigDialog, snap_config_dialog, SNAP, CONFIG_DIALOG, AdwPreferencesDialog)

void snap_config_dialog_present (GtkWindow *parent);

G_END_DECLS
