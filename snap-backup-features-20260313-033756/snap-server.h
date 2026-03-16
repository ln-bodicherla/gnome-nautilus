/* snap-server.h
 *
 * HTTP JSON API on localhost (port 7483).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define SNAP_TYPE_SERVER (snap_server_get_type ())
G_DECLARE_FINAL_TYPE (SnapServer, snap_server, SNAP, SERVER, GObject)

SnapServer    *snap_server_new               (void);

gboolean       snap_server_start             (SnapServer   *self,
                                              gint          port,
                                              GError      **error);
void           snap_server_stop              (SnapServer   *self);
gboolean       snap_server_is_running        (SnapServer   *self);
gint           snap_server_get_port          (SnapServer   *self);

G_END_DECLS
