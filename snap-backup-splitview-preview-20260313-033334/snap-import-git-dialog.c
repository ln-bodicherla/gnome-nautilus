/* snap-import-git-dialog.c
 *
 * Dialog to import git history with repo path entry and progress.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-import-git-dialog.h"
#include "snap-manager.h"

struct _SnapImportGitDialog
{
    AdwDialog parent_instance;

    GCancellable *cancellable;

    /* Widgets */
    GtkWidget *repo_entry;
    GtkWidget *file_entry;
    GtkWidget *import_button;
    GtkWidget *progress_bar;
    GtkWidget *status_label;
};

G_DEFINE_FINAL_TYPE (SnapImportGitDialog, snap_import_git_dialog, ADW_TYPE_DIALOG)

static void
on_import_finished (GObject      *source,
                    GAsyncResult *result,
                    gpointer      user_data)
{
    SnapImportGitDialog *self = SNAP_IMPORT_GIT_DIALOG (user_data);
    g_autoptr (GError) error = NULL;

    snap_manager_import_git_finish (SNAP_MANAGER (source), result, &error);

    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->progress_bar), 1.0);
    gtk_widget_set_sensitive (self->import_button, TRUE);

    if (error != NULL)
    {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            gtk_label_set_text (GTK_LABEL (self->status_label), error->message);
        else
            gtk_label_set_text (GTK_LABEL (self->status_label), "Import cancelled");
        return;
    }

    gtk_label_set_text (GTK_LABEL (self->status_label), "Git history imported successfully");
}

static gboolean
pulse_progress_cb (gpointer user_data)
{
    SnapImportGitDialog *self = SNAP_IMPORT_GIT_DIALOG (user_data);

    if (!gtk_widget_get_sensitive (self->import_button))
    {
        gtk_progress_bar_pulse (GTK_PROGRESS_BAR (self->progress_bar));
        return G_SOURCE_CONTINUE;
    }

    return G_SOURCE_REMOVE;
}

static void
on_import_clicked (GtkButton           *button,
                   SnapImportGitDialog *self)
{
    SnapManager *manager = snap_manager_get_default ();
    const gchar *repo_path;
    const gchar *file_path;

    repo_path = gtk_editable_get_text (GTK_EDITABLE (self->repo_entry));
    file_path = gtk_editable_get_text (GTK_EDITABLE (self->file_entry));

    if (repo_path == NULL || *repo_path == '\0')
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), "Repository path is required");
        return;
    }

    gtk_widget_set_sensitive (self->import_button, FALSE);
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->progress_bar), 0.0);
    gtk_label_set_text (GTK_LABEL (self->status_label), "Importing...");

    g_cancellable_reset (self->cancellable);

    snap_manager_import_git_async (manager, repo_path,
                                   (file_path != NULL && *file_path != '\0') ? file_path : NULL,
                                   self->cancellable,
                                   on_import_finished, self);

    g_timeout_add (300, pulse_progress_cb, self);
}

static void
snap_import_git_dialog_finalize (GObject *object)
{
    SnapImportGitDialog *self = SNAP_IMPORT_GIT_DIALOG (object);

    g_cancellable_cancel (self->cancellable);
    g_clear_object (&self->cancellable);

    G_OBJECT_CLASS (snap_import_git_dialog_parent_class)->finalize (object);
}

static void
snap_import_git_dialog_class_init (SnapImportGitDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = snap_import_git_dialog_finalize;
}

static void
snap_import_git_dialog_init (SnapImportGitDialog *self)
{
    GtkWidget *toolbar_view;
    GtkWidget *header_bar;
    GtkWidget *content_box;
    GtkWidget *clamp;
    GtkWidget *entries_group;

    self->cancellable = g_cancellable_new ();

    adw_dialog_set_title (ADW_DIALOG (self), "Import Git History");
    adw_dialog_set_content_width (ADW_DIALOG (self), 450);
    adw_dialog_set_content_height (ADW_DIALOG (self), 350);

    toolbar_view = adw_toolbar_view_new ();
    header_bar = adw_header_bar_new ();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), header_bar);

    content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top (content_box, 12);
    gtk_widget_set_margin_bottom (content_box, 12);
    gtk_widget_set_margin_start (content_box, 12);
    gtk_widget_set_margin_end (content_box, 12);

    /* Entries */
    entries_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (entries_group), "Git Repository");

    self->repo_entry = adw_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->repo_entry), "Repository Path");
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (entries_group), self->repo_entry);

    self->file_entry = adw_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->file_entry), "File Path (optional)");
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (entries_group), self->file_entry);

    gtk_box_append (GTK_BOX (content_box), entries_group);

    /* Progress bar */
    self->progress_bar = gtk_progress_bar_new ();
    gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (self->progress_bar), TRUE);
    gtk_box_append (GTK_BOX (content_box), self->progress_bar);

    /* Import button */
    self->import_button = gtk_button_new_with_label ("Import");
    gtk_widget_add_css_class (self->import_button, "suggested-action");
    gtk_widget_add_css_class (self->import_button, "pill");
    gtk_widget_set_halign (self->import_button, GTK_ALIGN_CENTER);
    g_signal_connect (self->import_button, "clicked", G_CALLBACK (on_import_clicked), self);
    gtk_box_append (GTK_BOX (content_box), self->import_button);

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
snap_import_git_dialog_present (GtkWindow *parent)
{
    SnapImportGitDialog *self;

    self = g_object_new (SNAP_TYPE_IMPORT_GIT_DIALOG, NULL);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
}
