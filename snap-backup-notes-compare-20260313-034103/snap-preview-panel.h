/* snap-preview-panel.h
 *
 * Quick file preview panel (similar to Dolphin F11 / macOS QuickLook).
 * Shows text content, image preview, or file metadata.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SNAP_TYPE_PREVIEW_PANEL (snap_preview_panel_get_type ())
G_DECLARE_FINAL_TYPE (SnapPreviewPanel, snap_preview_panel, SNAP, PREVIEW_PANEL, GtkBox)

GtkWidget *snap_preview_panel_new       (void);
void       snap_preview_panel_set_file (SnapPreviewPanel *self,
                                         const gchar      *path);
void       snap_preview_panel_clear    (SnapPreviewPanel *self);

G_END_DECLS
