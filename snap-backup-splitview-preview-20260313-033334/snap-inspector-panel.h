#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SNAP_TYPE_INSPECTOR_PANEL (snap_inspector_panel_get_type ())

G_DECLARE_FINAL_TYPE (SnapInspectorPanel, snap_inspector_panel, SNAP, INSPECTOR_PANEL, GtkBox)

SnapInspectorPanel *snap_inspector_panel_new      (void);
void                snap_inspector_panel_set_file  (SnapInspectorPanel *self,
                                                    const gchar        *file_path);
void                snap_inspector_panel_toggle    (gpointer            window);

G_END_DECLS
