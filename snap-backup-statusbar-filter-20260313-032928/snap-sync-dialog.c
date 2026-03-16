/* snap-sync-dialog.c
 *
 * Dialog for sync target path entry and progress tracking.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-sync-dialog.h"
#include "snap-sync.h"

struct _SnapSyncDialog
{
    AdwDialog parent_instance;

    SnapSync *sync;
    GCancellable *cancellable;

    /* Widgets */
    GtkWidget *target_entry;
    GtkWidget *sync_button;
    GtkWidget *progress_bar;
    GtkWidget *status_label;
};

G_DEFINE_FINAL_TYPE (SnapSyncDialog, snap_sync_dialog, ADW_TYPE_DIALOG)

static gboolean
update_progress_cb (gpointer user_data)
{
    SnapSyncDialog *self = SNAP_SYNC_DIALOG (user_data);
    gdouble progress;

    if (self->sync == NULL)
        return G_SOURCE_REMOVE;

    progress = snap_sync_get_progress (self->sync);
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->progress_bar), progress);

    if (progress >= 1.0)
        return G_SOURCE_REMOVE;

    return G_SOURCE_CONTINUE;
}

static void
on_sync_finished (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
    SnapSyncDialog *self = SNAP_SYNC_DIALOG (user_data);
    g_autoptr (GError) error = NULL;

    snap_sync_to_path_finish (SNAP_SYNC (source), result, &error);

    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->progress_bar), 1.0);
    gtk_widget_set_sensitive (self->sync_button, TRUE);

    if (error != NULL)
    {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            gtk_label_set_text (GTK_LABEL (self->status_label), error->message);
        else
            gtk_label_set_text (GTK_LABEL (self->status_label), "Sync cancelled");
        return;
    }

    gtk_label_set_text (GTK_LABEL (self->status_label), "Sync completed successfully");
}

static void
on_sync_clicked (GtkButton      *button,
                 SnapSyncDialog *self)
{
    const gchar *target;

    target = gtk_editable_get_text (GTK_EDITABLE (self->target_entry));
    if (target == NULL || *target == '\0')
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), "Target path is required");
        return;
    }

    gtk_widget_set_sensitive (self->sync_button, FALSE);
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->progress_bar), 0.0);
    gtk_label_set_text (GTK_LABEL (self->status_label), "Syncing...");

    g_cancellable_reset (self->cancellable);

    snap_sync_to_path_async (self->sync, target, self->cancellable,
                             on_sync_finished, self);

    g_timeout_add (200, update_progress_cb, self);
}

static void
snap_sync_dialog_finalize (GObject *object)
{
    SnapSyncDialog *self = SNAP_SYNC_DIALOG (object);

    g_cancellable_cancel (self->cancellable);
    g_clear_object (&self->cancellable);
    g_clear_object (&self->sync);

    G_OBJECT_CLASS (snap_sync_dialog_parent_class)->finalize (object);
}

static void
snap_sync_dialog_class_init (SnapSyncDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = snap_sync_dialog_finalize;
}

static void
snap_sync_dialog_init (SnapSyncDialog *self)
{
    GtkWidget *toolbar_view;
    GtkWidget *header_bar;
    GtkWidget *content_box;
    GtkWidget *clamp;
    GtkWidget *target_group;

    self->sync = snap_sync_new ();
    self->cancellable = g_cancellable_new ();

    adw_dialog_set_title (ADW_DIALOG (self), "Sync");
    adw_dialog_set_content_width (ADW_DIALOG (self), 400);
    adw_dialog_set_content_height (ADW_DIALOG (self), 300);

    toolbar_view = adw_toolbar_view_new ();
    header_bar = adw_header_bar_new ();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), header_bar);

    content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top (content_box, 12);
    gtk_widget_set_margin_bottom (content_box, 12);
    gtk_widget_set_margin_start (content_box, 12);
    gtk_widget_set_margin_end (content_box, 12);

    /* Target path entry */
    target_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (target_group), "Sync Target");

    self->target_entry = adw_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->target_entry), "Target Path");
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (target_group), self->target_entry);

    gtk_box_append (GTK_BOX (content_box), target_group);

    /* Progress bar */
    self->progress_bar = gtk_progress_bar_new ();
    gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (self->progress_bar), TRUE);
    gtk_box_append (GTK_BOX (content_box), self->progress_bar);

    /* Sync button */
    self->sync_button = gtk_button_new_with_label ("Start Sync");
    gtk_widget_add_css_class (self->sync_button, "suggested-action");
    gtk_widget_add_css_class (self->sync_button, "pill");
    gtk_widget_set_halign (self->sync_button, GTK_ALIGN_CENTER);
    g_signal_connect (self->sync_button, "clicked", G_CALLBACK (on_sync_clicked), self);
    gtk_box_append (GTK_BOX (content_box), self->sync_button);

    /* Status label */
    self->status_label = gtk_label_new ("");
    gtk_widget_add_css_class (self->status_label, "dim-label");
    gtk_box_append (GTK_BOX (content_box), self->status_label);

    clamp = adw_clamp_new ();
    adw_clamp_set_child (ADW_CLAMP (clamp), content_box);

    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view), clamp);
    adw_dialog_set_child (ADW_DIALOG (self), toolbar_view);
}

void
snap_sync_dialog_present (GtkWindow *parent)
{
    SnapSyncDialog *self;

    self = g_object_new (SNAP_TYPE_SYNC_DIALOG, NULL);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
}
