/* snap-recent-locations.h
 *
 * Tracks recently visited directories for quick navigation.
 * Persists to ~/.snapstore/recent_locations.json
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define SNAP_TYPE_RECENT_LOCATIONS (snap_recent_locations_get_type ())
G_DECLARE_FINAL_TYPE (SnapRecentLocations, snap_recent_locations, SNAP, RECENT_LOCATIONS, GObject)

SnapRecentLocations *snap_recent_locations_get_default (void);

void   snap_recent_locations_add        (SnapRecentLocations *self,
                                          const gchar         *path);
GList *snap_recent_locations_get_list   (SnapRecentLocations *self,
                                          guint                max_entries);
void   snap_recent_locations_clear      (SnapRecentLocations *self);

G_END_DECLS
