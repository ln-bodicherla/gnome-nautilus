/* snap-stats-dialog.h
 *
 * Analytics dashboard showing total files, snapshots, store size.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SNAP_TYPE_STATS_DIALOG (snap_stats_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SnapStatsDialog, snap_stats_dialog, SNAP, STATS_DIALOG, AdwDialog)

void snap_stats_dialog_present (GtkWindow *parent);

G_END_DECLS
