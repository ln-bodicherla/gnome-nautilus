/* snap-disk-usage.h
 *
 * Disk usage analysis for current directory.
 * Shows file/folder sizes in a treemap or sorted list view.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SNAP_TYPE_DISK_USAGE (snap_disk_usage_get_type ())
G_DECLARE_FINAL_TYPE (SnapDiskUsage, snap_disk_usage, SNAP, DISK_USAGE, GtkBox)

GtkWidget *snap_disk_usage_new            (void);
void       snap_disk_usage_scan_directory (SnapDiskUsage *self,
                                            const gchar   *path);

G_END_DECLS
