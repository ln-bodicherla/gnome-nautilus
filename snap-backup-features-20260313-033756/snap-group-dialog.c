/* snap-group-dialog.c
 *
 * Dialog for multi-file group save with name and message.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-group-dialog.h"
#include "snap-manager.h"

struct _SnapGroupDialog
{
    AdwDialog parent_instance;

    GList *file_paths;

    /* Widgets */
    GtkWidget *name_entry;
    GtkWidget *message_entry;
    GtkWidget *file_list;
    GtkWidget *status_label;
    GtkWidget *save_button;
};

G_DEFINE_FINAL_TYPE (SnapGroupDialog, snap_group_dialog, ADW_TYPE_DIALOG)

static void
on_save_clicked (GtkButton       *button,
                 SnapGroupDialog *self)
{
    SnapManager *manager = snap_manager_get_default ();
    g_autoptr (GError) error = NULL;
    const gchar *name;
    const gchar *message;

    name = gtk_editable_get_text (GTK_EDITABLE (self->name_entry));
    message = gtk_editable_get_text (GTK_EDITABLE (self->message_entry));

    if (name == NULL || *name == '\0')
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), "Group name is required");
        return;
    }

    gtk_widget_set_sensitive (self->save_button, FALSE);

    snap_manager_group_save (manager, name, message, self->file_paths, &error);
    if (error != NULL)
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), error->message);
        gtk_widget_set_sensitive (self->save_button, TRUE);
        return;
    }

    gtk_label_set_text (GTK_LABEL (self->status_label), "Group saved successfully");
    gtk_widget_set_sensitive (self->save_button, TRUE);
}

static void
snap_group_dialog_finalize (GObject *object)
{
    SnapGroupDialog *self = SNAP_GROUP_DIALOG (object);

    g_list_free_full (self->file_paths, g_free);

    G_OBJECT_CLASS (snap_group_dialog_parent_class)->finalize (object);
}

static void
snap_group_dialog_class_init (SnapGroupDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = snap_group_dialog_finalize;
}

static void
snap_group_dialog_init (SnapGroupDialog *self)
{
    GtkWidget *toolbar_view;
    GtkWidget *header_bar;
    GtkWidget *content_box;
    GtkWidget *clamp;
    GtkWidget *entries_group;

    adw_dialog_set_title (ADW_DIALOG (self), "Group Save");
    adw_dialog_set_content_width (ADW_DIALOG (self), 400);
    adw_dialog_set_content_height (ADW_DIALOG (self), 450);

    toolbar_view = adw_toolbar_view_new ();
    header_bar = adw_header_bar_new ();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), header_bar);

    content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top (content_box, 12);
    gtk_widget_set_margin_bottom (content_box, 12);
    gtk_widget_set_margin_start (content_box, 12);
    gtk_widget_set_margin_end (content_box, 12);

    /* Name and message entries */
    entries_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (entries_group), "Group Details");

    self->name_entry = adw_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->name_entry), "Name");
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (entries_group), self->name_entry);

    self->message_entry = adw_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->message_entry), "Message");
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (entries_group), self->message_entry);

    gtk_box_append (GTK_BOX (content_box), entries_group);

    /* File list */
    GtkWidget *files_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (files_group), "Files");

    self->file_list = gtk_list_box_new ();
    gtk_widget_add_css_class (self->file_list, "boxed-list");
    gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->file_list), GTK_SELECTION_NONE);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (files_group), self->file_list);
    gtk_box_append (GTK_BOX (content_box), files_group);

    /* Save button */
    self->save_button = gtk_button_new_with_label ("Save Group");
    gtk_widget_add_css_class (self->save_button, "suggested-action");
    gtk_widget_add_css_class (self->save_button, "pill");
    gtk_widget_set_halign (self->save_button, GTK_ALIGN_CENTER);
    g_signal_connect (self->save_button, "clicked", G_CALLBACK (on_save_clicked), self);
    gtk_box_append (GTK_BOX (content_box), self->save_button);

    /* Status label */
    self->status_label = gtk_label_new ("");
    gtk_widget_add_css_class (self->status_label, "dim-label");
    gtk_box_append (GTK_BOX (content_box), self->status_label);

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
snap_group_dialog_present (GList     *file_paths,
                           GtkWindow *parent)
{
    SnapGroupDialog *self;
    GList *l;

    g_return_if_fail (file_paths != NULL);

    self = g_object_new (SNAP_TYPE_GROUP_DIALOG, NULL);

    /* Copy the file paths list */
    self->file_paths = NULL;
    for (l = file_paths; l != NULL; l = l->next)
    {
        self->file_paths = g_list_append (self->file_paths, g_strdup (l->data));

        /* Add each file to the visible list */
        GtkWidget *row = adw_action_row_new ();
        g_autofree gchar *basename = g_path_get_basename (l->data);
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), basename);
        adw_action_row_set_subtitle (ADW_ACTION_ROW (row), l->data);
        gtk_list_box_append (GTK_LIST_BOX (self->file_list), row);
    }

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
}
