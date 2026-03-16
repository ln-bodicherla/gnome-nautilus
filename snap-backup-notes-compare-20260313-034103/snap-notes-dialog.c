/* snap-notes-dialog.c
 *
 * Dialog for editing file notes and tags.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-notes-dialog.h"
#include "snap-file-notes.h"

struct _SnapNotesDialog
{
    AdwDialog parent_instance;

    gchar *file_path;
    GtkWidget *note_view;
    GtkWidget *tag_entry;
    GtkWidget *tags_flow;
};

G_DEFINE_FINAL_TYPE (SnapNotesDialog, snap_notes_dialog, ADW_TYPE_DIALOG)

static void
refresh_tags (SnapNotesDialog *self)
{
    /* Clear existing tag widgets */
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child (self->tags_flow)) != NULL)
        gtk_flow_box_remove (GTK_FLOW_BOX (self->tags_flow), child);

    /* Add current tags */
    SnapFileNotes *notes = snap_file_notes_get_default ();
    GList *tags = snap_file_notes_get_tags (notes, self->file_path);

    for (GList *l = tags; l != NULL; l = l->next)
    {
        const gchar *tag = l->data;
        GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
        gtk_widget_add_css_class (box, "card");
        gtk_widget_set_margin_start (box, 2);
        gtk_widget_set_margin_end (box, 2);
        gtk_widget_set_margin_top (box, 2);
        gtk_widget_set_margin_bottom (box, 2);

        GtkWidget *label = gtk_label_new (tag);
        gtk_widget_add_css_class (label, "caption");
        gtk_widget_set_margin_start (label, 6);
        gtk_box_append (GTK_BOX (box), label);

        GtkWidget *remove = gtk_button_new_from_icon_name ("window-close-symbolic");
        gtk_widget_add_css_class (remove, "flat");
        gtk_widget_add_css_class (remove, "circular");
        gtk_widget_set_valign (remove, GTK_ALIGN_CENTER);

        /* Store tag name as widget name for removal */
        gtk_widget_set_name (remove, tag);
        g_signal_connect_swapped (remove, "clicked",
                                  G_CALLBACK (gtk_flow_box_remove),
                                  self->tags_flow);

        gtk_box_append (GTK_BOX (box), remove);
        gtk_flow_box_append (GTK_FLOW_BOX (self->tags_flow), box);
    }

    g_list_free_full (tags, g_free);
}

static void
on_add_tag (GtkEntry *entry, gpointer user_data)
{
    SnapNotesDialog *self = SNAP_NOTES_DIALOG (user_data);
    const gchar *tag = gtk_editable_get_text (GTK_EDITABLE (entry));

    if (tag == NULL || *tag == '\0')
        return;

    snap_file_notes_add_tag (snap_file_notes_get_default (), self->file_path, tag);
    gtk_editable_set_text (GTK_EDITABLE (entry), "");
    refresh_tags (self);
}

static void
on_save_note (SnapNotesDialog *self)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->note_view));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds (buffer, &start, &end);
    g_autofree gchar *text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

    if (text != NULL && *text != '\0')
        snap_file_notes_set_note (snap_file_notes_get_default (), self->file_path, text);
    else
        snap_file_notes_remove_note (snap_file_notes_get_default (), self->file_path);
}

static void
snap_notes_dialog_closed (AdwDialog *dialog)
{
    SnapNotesDialog *self = SNAP_NOTES_DIALOG (dialog);
    on_save_note (self);

    if (ADW_DIALOG_CLASS (snap_notes_dialog_parent_class)->closed)
        ADW_DIALOG_CLASS (snap_notes_dialog_parent_class)->closed (dialog);
}

static void
snap_notes_dialog_finalize (GObject *object)
{
    SnapNotesDialog *self = SNAP_NOTES_DIALOG (object);
    g_free (self->file_path);
    G_OBJECT_CLASS (snap_notes_dialog_parent_class)->finalize (object);
}

static void
snap_notes_dialog_class_init (SnapNotesDialogClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = snap_notes_dialog_finalize;
    ADW_DIALOG_CLASS (klass)->closed = snap_notes_dialog_closed;
}

static void
snap_notes_dialog_init (SnapNotesDialog *self)
{
    GtkWidget *toolbar_view, *header_bar, *scrolled, *content_box;
    GtkWidget *note_group, *tag_group, *tag_input_box;

    adw_dialog_set_title (ADW_DIALOG (self), "File Notes");
    adw_dialog_set_content_width (ADW_DIALOG (self), 450);
    adw_dialog_set_content_height (ADW_DIALOG (self), 400);

    toolbar_view = adw_toolbar_view_new ();
    header_bar = adw_header_bar_new ();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), header_bar);

    content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 18);
    gtk_widget_set_margin_top (content_box, 12);
    gtk_widget_set_margin_bottom (content_box, 24);
    gtk_widget_set_margin_start (content_box, 12);
    gtk_widget_set_margin_end (content_box, 12);

    /* Note section */
    note_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (note_group), "Note");

    self->note_view = gtk_text_view_new ();
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (self->note_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin (GTK_TEXT_VIEW (self->note_view), 8);
    gtk_text_view_set_right_margin (GTK_TEXT_VIEW (self->note_view), 8);
    gtk_text_view_set_top_margin (GTK_TEXT_VIEW (self->note_view), 8);
    gtk_text_view_set_bottom_margin (GTK_TEXT_VIEW (self->note_view), 8);
    gtk_widget_set_size_request (self->note_view, -1, 120);
    gtk_widget_add_css_class (self->note_view, "card");

    adw_preferences_group_add (ADW_PREFERENCES_GROUP (note_group), self->note_view);
    gtk_box_append (GTK_BOX (content_box), note_group);

    /* Tags section */
    tag_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (tag_group), "Tags");

    tag_input_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);

    self->tag_entry = gtk_entry_new ();
    gtk_entry_set_placeholder_text (GTK_ENTRY (self->tag_entry), "Add tag...");
    gtk_widget_set_hexpand (self->tag_entry, TRUE);
    g_signal_connect (self->tag_entry, "activate", G_CALLBACK (on_add_tag), self);
    gtk_box_append (GTK_BOX (tag_input_box), self->tag_entry);

    GtkWidget *add_btn = gtk_button_new_from_icon_name ("list-add-symbolic");
    gtk_widget_add_css_class (add_btn, "suggested-action");
    g_signal_connect_swapped (add_btn, "clicked",
                              G_CALLBACK (on_add_tag), self->tag_entry);
    gtk_box_append (GTK_BOX (tag_input_box), add_btn);

    adw_preferences_group_add (ADW_PREFERENCES_GROUP (tag_group), tag_input_box);

    self->tags_flow = gtk_flow_box_new ();
    gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (self->tags_flow), GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (self->tags_flow), 10);
    gtk_flow_box_set_row_spacing (GTK_FLOW_BOX (self->tags_flow), 4);
    gtk_flow_box_set_column_spacing (GTK_FLOW_BOX (self->tags_flow), 4);

    adw_preferences_group_add (ADW_PREFERENCES_GROUP (tag_group), self->tags_flow);
    gtk_box_append (GTK_BOX (content_box), tag_group);

    scrolled = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), content_box);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view), scrolled);
    adw_dialog_set_child (ADW_DIALOG (self), toolbar_view);
}

void
snap_notes_dialog_present (const gchar *path, GtkWindow *parent)
{
    SnapNotesDialog *self = g_object_new (SNAP_TYPE_NOTES_DIALOG, NULL);
    self->file_path = g_strdup (path);

    g_autofree gchar *basename = g_path_get_basename (path);
    g_autofree gchar *title = g_strdup_printf ("Notes: %s", basename);
    adw_dialog_set_title (ADW_DIALOG (self), title);

    /* Load existing note */
    const gchar *note = snap_file_notes_get_note (snap_file_notes_get_default (), path);
    if (note != NULL)
    {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->note_view));
        gtk_text_buffer_set_text (buffer, note, -1);
    }

    /* Load tags */
    refresh_tags (self);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
}
