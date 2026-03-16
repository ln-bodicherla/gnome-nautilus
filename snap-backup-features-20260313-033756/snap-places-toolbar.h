/* snap-places-toolbar.h
 *
 * Dolphin-style places toolbar showing bookmarked locations
 * as clickable buttons below the main toolbar.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SNAP_TYPE_PLACES_TOOLBAR (snap_places_toolbar_get_type ())
G_DECLARE_FINAL_TYPE (SnapPlacesToolbar, snap_places_toolbar, SNAP, PLACES_TOOLBAR, GtkBox)

GtkWidget *snap_places_toolbar_new           (void);
void       snap_places_toolbar_set_visible   (SnapPlacesToolbar *self,
                                                gboolean           visible);
void       snap_places_toolbar_add_place     (SnapPlacesToolbar *self,
                                                const gchar       *name,
                                                const gchar       *path,
                                                const gchar       *icon_name);

/* Signal: "place-activated" with path string */

G_END_DECLS
