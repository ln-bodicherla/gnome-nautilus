/* snap-notify.h
 *
 * Notifications via GNotification.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

void  snap_notify_saved      (const gchar *file_path,
                              gint64       version);
void  snap_notify_restored   (const gchar *file_path,
                              gint64       version);
void  snap_notify_error      (const gchar *title,
                              const gchar *message);
void  snap_notify_info       (const gchar *title,
                              const gchar *message);

G_END_DECLS
