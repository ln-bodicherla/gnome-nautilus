/* snap-stash-dialog.c
 *
 * Dialog for stash save/pop/list operations.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-stash-dialog.h"
#include "snap-manager.h"

struct _SnapStashDialog
{
    AdwDialog parent_instance;

    gchar *file_path;

    /* Widgets */
    GtkWidget *message_entry;
    GtkWidget *stash_list;
    GtkWidget *save_button;
    GtkWidget *pop_button;
    GtkWidget *status_label;
};

G_DEFINE_FINAL_TYPE (SnapStashDialog, snap_stash_dialog, ADW_TYPE_DIALOG)

static void
snap_stash_dialog_refresh_list (SnapStashDialog *self)
{
    SnapManager *manager = snap_manager_get_default ();
    g_autoptr (GError) error = NULL;
    GList *stashes;
    GtkWidget *child;

    while ((child = gtk_widget_get_first_child (self->stash_list)))
        gtk_list_box_remove (GTK_LIST_BOX (self->stash_list), child);

    stashes = snap_manager_stash_list (manager, self->file_path, &error);
    if (error != NULL)
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), error->message);
        return;
    }

    gboolean has_stashes = (stashes != NULL);
    gtk_widget_set_sensitive (self->pop_button, has_stashes);

    for (GList *l = stashes; l != NULL; l = l->next)
    {
        SnapStash *stash = l->data;
        GtkWidget *row = adw_action_row_new ();

        g_autofree gchar *title = g_strdup_printf ("Stash #%" G_GINT64_FORMAT, stash->id);
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);

        if (stash->message != NULL && *stash->message != '\0')
            adw_action_row_set_subtitle (ADW_ACTION_ROW (row), stash->message);

        GtkWidget *icon = gtk_image_new_from_icon_name ("document-save-symbolic");
        adw_action_row_add_prefix (ADW_ACTION_ROW (row), icon);

        gtk_list_box_append (GTK_LIST_BOX (self->stash_list), row);
    }

    g_list_free_full (stashes, (GDestroyNotify) snap_stash_free);
}

static void
on_save_clicked (GtkButton       *button,
                 SnapStashDialog *self)
{
    SnapManager *manager = snap_manager_get_default ();
    g_autoptr (GError) error = NULL;
    const gchar *message;

    message = gtk_editable_get_text (GTK_EDITABLE (self->message_entry));

    snap_manager_stash (manager, self->file_path, message, &error);
    if (error != NULL)
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), error->message);
        return;
    }

    gtk_editable_set_text (GTK_EDITABLE (self->message_entry), "");
    gtk_label_set_text (GTK_LABEL (self->status_label), "Changes stashed");
    snap_stash_dialog_refresh_list (self);
}

static void
on_pop_clicked (GtkButton       *button,
                SnapStashDialog *self)
{
    SnapManager *manager = snap_manager_get_default ();
    g_autoptr (GError) error = NULL;

    snap_manager_stash_pop (manager, self->file_path, &error);
    if (error != NULL)
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), error->message);
        return;
    }

    gtk_label_set_text (GTK_LABEL (self->status_label), "Stash popped and applied");
    snap_stash_dialog_refresh_list (self);
}

static void
snap_stash_dialog_finalize (GObject *object)
{
    SnapStashDialog *self = SNAP_STASH_DIALOG (object);

    g_free (self->file_path);

    G_OBJECT_CLASS (snap_stash_dialog_parent_class)->finalize (object);
}

static void
snap_stash_dialog_class_init (SnapStashDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = snap_stash_dialog_finalize;
}

static void
snap_stash_dialog_init (SnapStashDialog *self)
{
    GtkWidget *toolbar_view;
    GtkWidget *header_bar;
    GtkWidget *content_box;
    GtkWidget *clamp;
    GtkWidget *save_group;
    GtkWidget *button_box;
    GtkWidget *list_group;

    adw_dialog_set_title (ADW_DIALOG (self), "Stash");
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

    /* Save section */
    save_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (save_group), "Stash Changes");

    self->message_entry = adw_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->message_entry), "Message (optional)");
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (save_group), self->message_entry);

    gtk_box_append (GTK_BOX (content_box), save_group);

    /* Buttons */
    button_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign (button_box, GTK_ALIGN_CENTER);

    self->save_button = gtk_button_new_with_label ("Stash Save");
    gtk_widget_add_css_class (self->save_button, "suggested-action");
    gtk_widget_add_css_class (self->save_button, "pill");
    g_signal_connect (self->save_button, "clicked", G_CALLBACK (on_save_clicked), self);
    gtk_box_append (GTK_BOX (button_box), self->save_button);

    self->pop_button = gtk_button_new_with_label ("Stash Pop");
    gtk_widget_add_css_class (self->pop_button, "pill");
    gtk_widget_set_sensitive (self->pop_button, FALSE);
    g_signal_connect (self->pop_button, "clicked", G_CALLBACK (on_pop_clicked), self);
    gtk_box_append (GTK_BOX (button_box), self->pop_button);

    gtk_box_append (GTK_BOX (content_box), button_box);

    /* Stash list */
    list_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (list_group), "Stashed Changes");

    self->stash_list = gtk_list_box_new ();
    gtk_widget_add_css_class (self->stash_list, "boxed-list");
    gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->stash_list), GTK_SELECTION_NONE);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (list_group), self->stash_list);
    gtk_box_append (GTK_BOX (content_box), list_group);

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
snap_stash_dialog_present (const gchar *file_path,
                           GtkWindow   *parent)
{
    SnapStashDialog *self;

    g_return_if_fail (file_path != NULL);

    self = g_object_new (SNAP_TYPE_STASH_DIALOG, NULL);
    self->file_path = g_strdup (file_path);

    snap_stash_dialog_refresh_list (self);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
}
