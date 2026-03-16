/* snap-audit-dialog.c
 *
 * Dialog showing cryptographic chain verification results.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-audit-dialog.h"
#include "snap-manager.h"

struct _SnapAuditDialog
{
    AdwDialog parent_instance;

    gchar *file_path;

    /* Widgets */
    GtkWidget *status_icon;
    GtkWidget *status_label;
    GtkWidget *details_list;
    GtkWidget *verify_button;
};

G_DEFINE_FINAL_TYPE (SnapAuditDialog, snap_audit_dialog, ADW_TYPE_DIALOG)

static void
snap_audit_dialog_run_verify (SnapAuditDialog *self)
{
    SnapManager *manager = snap_manager_get_default ();
    g_autoptr (GError) error = NULL;
    gboolean valid;

    gtk_widget_set_sensitive (self->verify_button, FALSE);

    valid = snap_manager_verify (manager, self->file_path, &error);

    if (error != NULL)
    {
        gtk_image_set_from_icon_name (GTK_IMAGE (self->status_icon),
                                      "dialog-error-symbolic");
        gtk_label_set_text (GTK_LABEL (self->status_label), error->message);
    }
    else if (valid)
    {
        gtk_image_set_from_icon_name (GTK_IMAGE (self->status_icon),
                                      "emblem-ok-symbolic");
        gtk_label_set_text (GTK_LABEL (self->status_label),
                            "Chain verification passed. All hashes are valid.");

        /* Show audit log entries */
        GList *audit_log = snap_manager_history (manager, self->file_path, 0, NULL);
        GtkWidget *child;

        while ((child = gtk_widget_get_first_child (self->details_list)))
            gtk_list_box_remove (GTK_LIST_BOX (self->details_list), child);

        for (GList *l = audit_log; l != NULL; l = l->next)
        {
            SnapSnapshot *snap = l->data;
            GtkWidget *row = adw_action_row_new ();

            g_autofree gchar *title = g_strdup_printf ("Version %" G_GINT64_FORMAT, snap->version);
            adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);

            g_autofree gchar *subtitle = g_strdup_printf ("Hash: %s", snap->hash);
            adw_action_row_set_subtitle (ADW_ACTION_ROW (row), subtitle);

            GtkWidget *icon = gtk_image_new_from_icon_name ("emblem-ok-symbolic");
            adw_action_row_add_prefix (ADW_ACTION_ROW (row), icon);

            gtk_list_box_append (GTK_LIST_BOX (self->details_list), row);
        }

        g_list_free_full (audit_log, (GDestroyNotify) snap_snapshot_free);
    }
    else
    {
        gtk_image_set_from_icon_name (GTK_IMAGE (self->status_icon),
                                      "dialog-warning-symbolic");
        gtk_label_set_text (GTK_LABEL (self->status_label),
                            "Chain verification FAILED. Integrity compromised.");
    }

    gtk_widget_set_sensitive (self->verify_button, TRUE);
}

static void
on_verify_clicked (GtkButton       *button,
                   SnapAuditDialog *self)
{
    snap_audit_dialog_run_verify (self);
}

static void
snap_audit_dialog_finalize (GObject *object)
{
    SnapAuditDialog *self = SNAP_AUDIT_DIALOG (object);

    g_free (self->file_path);

    G_OBJECT_CLASS (snap_audit_dialog_parent_class)->finalize (object);
}

static void
snap_audit_dialog_class_init (SnapAuditDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = snap_audit_dialog_finalize;
}

static void
snap_audit_dialog_init (SnapAuditDialog *self)
{
    GtkWidget *toolbar_view;
    GtkWidget *header_bar;
    GtkWidget *content_box;
    GtkWidget *clamp;
    GtkWidget *status_box;

    adw_dialog_set_title (ADW_DIALOG (self), "Audit");
    adw_dialog_set_content_width (ADW_DIALOG (self), 450);
    adw_dialog_set_content_height (ADW_DIALOG (self), 500);

    toolbar_view = adw_toolbar_view_new ();
    header_bar = adw_header_bar_new ();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), header_bar);

    content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top (content_box, 24);
    gtk_widget_set_margin_bottom (content_box, 24);
    gtk_widget_set_margin_start (content_box, 12);
    gtk_widget_set_margin_end (content_box, 12);

    /* Status display */
    status_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign (status_box, GTK_ALIGN_CENTER);

    self->status_icon = gtk_image_new_from_icon_name ("dialog-question-symbolic");
    gtk_image_set_pixel_size (GTK_IMAGE (self->status_icon), 48);
    gtk_box_append (GTK_BOX (status_box), self->status_icon);

    self->status_label = gtk_label_new ("Click Verify to check chain integrity");
    gtk_label_set_wrap (GTK_LABEL (self->status_label), TRUE);
    gtk_box_append (GTK_BOX (status_box), self->status_label);

    gtk_box_append (GTK_BOX (content_box), status_box);

    /* Verify button */
    self->verify_button = gtk_button_new_with_label ("Verify Chain");
    gtk_widget_add_css_class (self->verify_button, "suggested-action");
    gtk_widget_add_css_class (self->verify_button, "pill");
    gtk_widget_set_halign (self->verify_button, GTK_ALIGN_CENTER);
    g_signal_connect (self->verify_button, "clicked", G_CALLBACK (on_verify_clicked), self);
    gtk_box_append (GTK_BOX (content_box), self->verify_button);

    /* Details list */
    self->details_list = gtk_list_box_new ();
    gtk_widget_add_css_class (self->details_list, "boxed-list");
    gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->details_list), GTK_SELECTION_NONE);
    gtk_box_append (GTK_BOX (content_box), self->details_list);

    clamp = adw_clamp_new ();
    adw_clamp_set_child (ADW_CLAMP (clamp), content_box);

    GtkWidget *scrolled = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), clamp);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view), scrolled);
    adw_dialog_set_child (ADW_DIALOG (self), toolbar_view);
}

void
snap_audit_dialog_present (const gchar *file_path,
                           GtkWindow   *parent)
{
    SnapAuditDialog *self;

    g_return_if_fail (file_path != NULL);

    self = g_object_new (SNAP_TYPE_AUDIT_DIALOG, NULL);
    self->file_path = g_strdup (file_path);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
}
