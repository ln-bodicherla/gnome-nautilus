/* snap-notify.c
 *
 * Notifications via GNotification.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-notify.h"
#include <gio/gio.h>

void
snap_notify_saved (const gchar *file_path, gint64 version)
{
    GApplication *app = g_application_get_default ();
    if (app == NULL)
        return;

    g_autofree gchar *basename = g_path_get_basename (file_path);
    g_autofree gchar *body = g_strdup_printf ("Saved %s as version %ld", basename, (long) version);

    g_autoptr (GNotification) notification = g_notification_new ("Snap: Saved");
    g_notification_set_body (notification, body);
    g_notification_set_priority (notification, G_NOTIFICATION_PRIORITY_LOW);

    g_application_send_notification (app, "snap-saved", notification);
}

void
snap_notify_restored (const gchar *file_path, gint64 version)
{
    GApplication *app = g_application_get_default ();
    if (app == NULL)
        return;

    g_autofree gchar *basename = g_path_get_basename (file_path);
    g_autofree gchar *body = g_strdup_printf ("Restored %s to version %ld", basename, (long) version);

    g_autoptr (GNotification) notification = g_notification_new ("Snap: Restored");
    g_notification_set_body (notification, body);

    g_application_send_notification (app, "snap-restored", notification);
}

void
snap_notify_error (const gchar *title, const gchar *message)
{
    GApplication *app = g_application_get_default ();
    if (app == NULL)
        return;

    g_autoptr (GNotification) notification = g_notification_new (title);
    g_notification_set_body (notification, message);
    g_notification_set_priority (notification, G_NOTIFICATION_PRIORITY_URGENT);

    g_application_send_notification (app, "snap-error", notification);
}

void
snap_notify_info (const gchar *title, const gchar *message)
{
    GApplication *app = g_application_get_default ();
    if (app == NULL)
        return;

    g_autoptr (GNotification) notification = g_notification_new (title);
    g_notification_set_body (notification, message);

    g_application_send_notification (app, "snap-info", notification);
}
