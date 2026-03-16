/* snap-restore-dialog.c
 *
 * Confirmation dialog for restoring a snapshot.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib/gi18n.h>

#include "snap-restore-dialog.h"
#include "snap-manager.h"

struct _SnapRestoreDialog
{
    AdwAlertDialog parent_instance;

    GtkWidget *version_spin;
    gchar     *file_path;
};

G_DEFINE_FINAL_TYPE (SnapRestoreDialog, snap_restore_dialog, ADW_TYPE_ALERT_DIALOG)

static void
on_response (SnapRestoreDialog *self,
             const gchar       *response)
{
    if (g_strcmp0 (response, "restore") == 0)
    {
        g_autoptr (GError) error = NULL;
        SnapManager *manager;
        gint64 version;

        version = (gint64) gtk_spin_button_get_value (
            GTK_SPIN_BUTTON (self->version_spin));
        manager = snap_manager_get_default ();

        snap_manager_restore (manager, self->file_path, version, NULL, &error);

        if (error != NULL)
        {
            g_warning ("Failed to restore snapshot: %s", error->message);
        }
    }
}

static void
snap_restore_dialog_dispose (GObject *object)
{
    SnapRestoreDialog *self = SNAP_RESTORE_DIALOG (object);

    g_clear_pointer (&self->file_path, g_free);

    G_OBJECT_CLASS (snap_restore_dialog_parent_class)->dispose (object);
}

static void
snap_restore_dialog_finalize (GObject *object)
{
    G_OBJECT_CLASS (snap_restore_dialog_parent_class)->finalize (object);
}

static void
snap_restore_dialog_init (SnapRestoreDialog *self)
{
}

static void
snap_restore_dialog_class_init (SnapRestoreDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = snap_restore_dialog_dispose;
    object_class->finalize = snap_restore_dialog_finalize;
}

void
snap_restore_dialog_present (const gchar *file_path,
                             GtkWindow   *parent)
{
    SnapRestoreDialog *self;
    GtkWidget *version_box;
    GtkWidget *version_label;
    SnapManager *manager;
    gint64 latest_version;
    g_autoptr (GError) error = NULL;
    GList *history;

    g_return_if_fail (file_path != NULL);

    self = g_object_new (SNAP_TYPE_RESTORE_DIALOG, NULL);
    self->file_path = g_strdup (file_path);

    adw_alert_dialog_set_heading (ADW_ALERT_DIALOG (self),
                                  _("Restore Snapshot"));
    adw_alert_dialog_set_body (ADW_ALERT_DIALOG (self),
                               _("This will replace the current file contents "
                                 "with the selected snapshot version. This "
                                 "action cannot be undone."));

    /* Determine latest version for the spin button range */
    manager = snap_manager_get_default ();
    history = snap_manager_history (manager, file_path, 1, &error);
    latest_version = 1;

    if (history != NULL)
    {
        SnapSnapshot *snapshot = history->data;
        latest_version = snapshot->version;
        g_list_free_full (history, (GDestroyNotify) snap_snapshot_free);
    }

    /* Version picker */
    version_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign (version_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top (version_box, 12);

    version_label = gtk_label_new (_("Version:"));
    gtk_box_append (GTK_BOX (version_box), version_label);

    self->version_spin = gtk_spin_button_new_with_range (1, latest_version, 1);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->version_spin),
                               latest_version);
    gtk_box_append (GTK_BOX (version_box), self->version_spin);

    adw_alert_dialog_set_extra_child (ADW_ALERT_DIALOG (self), version_box);

    adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (self),
                                    "cancel", _("Cancel"),
                                    "restore", _("Restore"),
                                    NULL);
    adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (self),
                                              "restore",
                                              ADW_RESPONSE_DESTRUCTIVE);
    adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (self), "cancel");
    adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (self), "cancel");

    g_signal_connect (self, "response",
                      G_CALLBACK (on_response), NULL);

    adw_alert_dialog_choose (ADW_ALERT_DIALOG (self), GTK_WIDGET (parent),
                             NULL, NULL, NULL);
}
