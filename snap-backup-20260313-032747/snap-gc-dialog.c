/* snap-gc-dialog.c
 *
 * GC/Clean/Repair confirmation dialog.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-gc-dialog.h"
#include "snap-manager.h"

struct _SnapGcDialog
{
    AdwAlertDialog parent_instance;
};

G_DEFINE_FINAL_TYPE (SnapGcDialog, snap_gc_dialog, ADW_TYPE_ALERT_DIALOG)

static void
snap_gc_dialog_class_init (SnapGcDialogClass *klass)
{
}

static void
snap_gc_dialog_init (SnapGcDialog *self)
{
}

static void
on_gc_response (AdwAlertDialog *dialog, const gchar *response, gpointer user_data)
{
    if (g_strcmp0 (response, "gc") == 0)
    {
        SnapManager *mgr = snap_manager_get_default ();
        GError *error = NULL;
        gint deleted = snap_manager_gc (mgr, &error);

        if (error != NULL)
        {
            g_warning ("GC failed: %s", error->message);
            g_error_free (error);
        }
        else
        {
            g_message ("GC completed: %d snapshots removed", deleted);
        }
    }
    else if (g_strcmp0 (response, "repair") == 0)
    {
        SnapManager *mgr = snap_manager_get_default ();
        GError *error = NULL;
        gboolean ok = snap_manager_repair (mgr, &error);

        if (!ok)
        {
            g_warning ("Repair failed: %s", error ? error->message : "unknown");
            g_clear_error (&error);
        }
    }
}

void
snap_gc_dialog_present (GtkWindow *parent)
{
    SnapGcDialog *self = g_object_new (SNAP_TYPE_GC_DIALOG, NULL);

    adw_alert_dialog_set_heading (ADW_ALERT_DIALOG (self), "Snap Maintenance");
    adw_alert_dialog_set_body (ADW_ALERT_DIALOG (self),
        "Garbage collection removes old unpinned snapshots. "
        "Repair checks database integrity and cleans up orphaned data.");

    adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (self),
                                     "cancel", "Cancel",
                                     "gc", "Run GC",
                                     "repair", "Repair",
                                     NULL);

    adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (self),
                                               "gc", ADW_RESPONSE_SUGGESTED);
    adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (self), "cancel");
    adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (self), "cancel");

    g_signal_connect (self, "response", G_CALLBACK (on_gc_response), NULL);

    adw_alert_dialog_choose (ADW_ALERT_DIALOG (self), GTK_WIDGET (parent), NULL, NULL, NULL);
}
