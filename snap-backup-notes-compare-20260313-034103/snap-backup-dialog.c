/* snap-backup-dialog.c
 *
 * Dialog for backup create and import with file picker.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-backup-dialog.h"
#include "snap-sync.h"

struct _SnapBackupDialog
{
    AdwDialog parent_instance;

    SnapSync *sync;
    GCancellable *cancellable;

    /* Widgets */
    GtkWidget *progress_bar;
    GtkWidget *status_label;
    GtkWidget *backup_button;
    GtkWidget *import_button;
};

G_DEFINE_FINAL_TYPE (SnapBackupDialog, snap_backup_dialog, ADW_TYPE_DIALOG)

static void
on_backup_finished (GObject      *source,
                    GAsyncResult *result,
                    gpointer      user_data)
{
    SnapBackupDialog *self = SNAP_BACKUP_DIALOG (user_data);
    g_autoptr (GError) error = NULL;

    snap_sync_backup_finish (SNAP_SYNC (source), result, &error);

    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->progress_bar), 1.0);
    gtk_widget_set_sensitive (self->backup_button, TRUE);
    gtk_widget_set_sensitive (self->import_button, TRUE);

    if (error != NULL)
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), error->message);
        return;
    }

    gtk_label_set_text (GTK_LABEL (self->status_label), "Backup created successfully");
}

static void
on_backup_save_response (GObject      *source,
                         GAsyncResult *result,
                         gpointer      user_data)
{
    SnapBackupDialog *self = SNAP_BACKUP_DIALOG (user_data);
    GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
    g_autoptr (GError) error = NULL;
    g_autoptr (GFile) file = NULL;

    file = gtk_file_dialog_save_finish (dialog, result, &error);
    if (file == NULL)
        return;

    g_autofree gchar *path = g_file_get_path (file);

    gtk_widget_set_sensitive (self->backup_button, FALSE);
    gtk_widget_set_sensitive (self->import_button, FALSE);
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->progress_bar), 0.0);
    gtk_label_set_text (GTK_LABEL (self->status_label), "Creating backup...");

    g_cancellable_reset (self->cancellable);
    snap_sync_backup_async (self->sync, path, self->cancellable,
                            on_backup_finished, self);
}

static void
on_backup_clicked (GtkButton        *button,
                   SnapBackupDialog *self)
{
    GtkFileDialog *dialog;
    GtkWindow *parent;

    dialog = gtk_file_dialog_new ();
    gtk_file_dialog_set_title (dialog, "Save Backup");
    gtk_file_dialog_set_initial_name (dialog, "snap-backup.tar.gz");

    parent = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));
    gtk_file_dialog_save (dialog, parent, NULL,
                          on_backup_save_response, self);
}

static void
on_import_finished (GObject      *source,
                    GAsyncResult *result,
                    gpointer      user_data)
{
    SnapBackupDialog *self = SNAP_BACKUP_DIALOG (user_data);
    g_autoptr (GError) error = NULL;

    snap_sync_import_finish (SNAP_SYNC (source), result, &error);

    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->progress_bar), 1.0);
    gtk_widget_set_sensitive (self->backup_button, TRUE);
    gtk_widget_set_sensitive (self->import_button, TRUE);

    if (error != NULL)
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), error->message);
        return;
    }

    gtk_label_set_text (GTK_LABEL (self->status_label), "Backup imported successfully");
}

static void
on_import_open_response (GObject      *source,
                         GAsyncResult *result,
                         gpointer      user_data)
{
    SnapBackupDialog *self = SNAP_BACKUP_DIALOG (user_data);
    GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
    g_autoptr (GError) error = NULL;
    g_autoptr (GFile) file = NULL;

    file = gtk_file_dialog_open_finish (dialog, result, &error);
    if (file == NULL)
        return;

    g_autofree gchar *path = g_file_get_path (file);

    gtk_widget_set_sensitive (self->backup_button, FALSE);
    gtk_widget_set_sensitive (self->import_button, FALSE);
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->progress_bar), 0.0);
    gtk_label_set_text (GTK_LABEL (self->status_label), "Importing backup...");

    g_cancellable_reset (self->cancellable);
    snap_sync_import_async (self->sync, path, self->cancellable,
                            on_import_finished, self);
}

static void
on_import_clicked (GtkButton        *button,
                   SnapBackupDialog *self)
{
    GtkFileDialog *dialog;
    GtkWindow *parent;

    dialog = gtk_file_dialog_new ();
    gtk_file_dialog_set_title (dialog, "Import Backup");

    parent = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));
    gtk_file_dialog_open (dialog, parent, NULL,
                          on_import_open_response, self);
}

static void
snap_backup_dialog_finalize (GObject *object)
{
    SnapBackupDialog *self = SNAP_BACKUP_DIALOG (object);

    g_cancellable_cancel (self->cancellable);
    g_clear_object (&self->cancellable);
    g_clear_object (&self->sync);

    G_OBJECT_CLASS (snap_backup_dialog_parent_class)->finalize (object);
}

static void
snap_backup_dialog_class_init (SnapBackupDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = snap_backup_dialog_finalize;
}

static void
snap_backup_dialog_init (SnapBackupDialog *self)
{
    GtkWidget *toolbar_view;
    GtkWidget *header_bar;
    GtkWidget *content_box;
    GtkWidget *clamp;
    GtkWidget *button_box;

    self->sync = snap_sync_new ();
    self->cancellable = g_cancellable_new ();

    adw_dialog_set_title (ADW_DIALOG (self), "Backup");
    adw_dialog_set_content_width (ADW_DIALOG (self), 400);
    adw_dialog_set_content_height (ADW_DIALOG (self), 300);

    toolbar_view = adw_toolbar_view_new ();
    header_bar = adw_header_bar_new ();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), header_bar);

    content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top (content_box, 24);
    gtk_widget_set_margin_bottom (content_box, 24);
    gtk_widget_set_margin_start (content_box, 24);
    gtk_widget_set_margin_end (content_box, 24);

    /* Buttons */
    button_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign (button_box, GTK_ALIGN_CENTER);

    self->backup_button = gtk_button_new_with_label ("Create Backup");
    gtk_widget_add_css_class (self->backup_button, "suggested-action");
    gtk_widget_add_css_class (self->backup_button, "pill");
    g_signal_connect (self->backup_button, "clicked", G_CALLBACK (on_backup_clicked), self);
    gtk_box_append (GTK_BOX (button_box), self->backup_button);

    self->import_button = gtk_button_new_with_label ("Import Backup");
    gtk_widget_add_css_class (self->import_button, "pill");
    g_signal_connect (self->import_button, "clicked", G_CALLBACK (on_import_clicked), self);
    gtk_box_append (GTK_BOX (button_box), self->import_button);

    gtk_box_append (GTK_BOX (content_box), button_box);

    /* Progress bar */
    self->progress_bar = gtk_progress_bar_new ();
    gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (self->progress_bar), TRUE);
    gtk_box_append (GTK_BOX (content_box), self->progress_bar);

    /* Status label */
    self->status_label = gtk_label_new ("Create or import a snap backup archive");
    gtk_widget_add_css_class (self->status_label, "dim-label");
    gtk_box_append (GTK_BOX (content_box), self->status_label);

    clamp = adw_clamp_new ();
    adw_clamp_set_child (ADW_CLAMP (clamp), content_box);

    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view), clamp);
    adw_dialog_set_child (ADW_DIALOG (self), toolbar_view);
}

void
snap_backup_dialog_present (GtkWindow *parent)
{
    SnapBackupDialog *self;

    self = g_object_new (SNAP_TYPE_BACKUP_DIALOG, NULL);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
}
