#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

GIcon    *snap_status_indicator_get_icon    (const gchar *file_path);
gboolean  snap_status_indicator_is_tracked  (const gchar *file_path);

G_END_DECLS
