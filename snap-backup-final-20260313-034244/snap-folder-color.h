/* snap-folder-color.h
 *
 * Custom folder colors/emblems system.
 * Stores color associations in ~/.snapstore/folder_colors.json
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

#define SNAP_TYPE_FOLDER_COLOR (snap_folder_color_get_type ())
G_DECLARE_FINAL_TYPE (SnapFolderColor, snap_folder_color, SNAP, FOLDER_COLOR, GObject)

SnapFolderColor *snap_folder_color_get_default   (void);
void             snap_folder_color_set           (SnapFolderColor *self,
                                                    const gchar     *path,
                                                    const gchar     *color_name);
const gchar     *snap_folder_color_get           (SnapFolderColor *self,
                                                    const gchar     *path);
void             snap_folder_color_remove        (SnapFolderColor *self,
                                                    const gchar     *path);

/* Available color names: red, orange, yellow, green, blue, purple, grey, brown */

G_END_DECLS
