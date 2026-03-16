/* snap-diff-dialog.c
 *
 * Dialog showing diff between current file and latest snapshot.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib/gi18n.h>

#include "snap-diff-dialog.h"
#include "snap-manager.h"
#include "snap-diff.h"

struct _SnapDiffDialog
{
    AdwDialog parent_instance;

    GtkWidget       *text_view;
    GtkTextBuffer   *text_buffer;
    GtkWidget       *unified_toggle;
    GtkWidget       *side_by_side_toggle;
    gchar           *file_path;
    SnapDiffResult  *diff_result;
};

G_DEFINE_FINAL_TYPE (SnapDiffDialog, snap_diff_dialog, ADW_TYPE_DIALOG)

static void
snap_diff_dialog_render_unified (SnapDiffDialog *self)
{
    GtkTextIter iter;
    GList *h;

    gtk_text_buffer_set_text (self->text_buffer, "", -1);

    if (self->diff_result == NULL)
    {
        return;
    }

    gtk_text_buffer_get_start_iter (self->text_buffer, &iter);

    for (h = self->diff_result->hunks; h != NULL; h = h->next)
    {
        SnapDiffHunk *hunk = h->data;
        GList *l;

        for (l = hunk->lines; l != NULL; l = l->next)
        {
            SnapDiffLine *line = l->data;
            GtkTextIter line_start;
            const gchar *tag_name = NULL;
            g_autofree gchar *display = NULL;

            gtk_text_buffer_get_end_iter (self->text_buffer, &line_start);
            gint offset = gtk_text_iter_get_offset (&line_start);

            switch (line->type)
            {
                case SNAP_DIFF_LINE_ADDITION:
                    display = g_strdup_printf ("+%s\n", line->text);
                    tag_name = "snap-addition";
                    break;
                case SNAP_DIFF_LINE_DELETION:
                    display = g_strdup_printf ("-%s\n", line->text);
                    tag_name = "snap-deletion";
                    break;
                case SNAP_DIFF_LINE_CONTEXT:
                    display = g_strdup_printf (" %s\n", line->text);
                    break;
            }

            gtk_text_buffer_insert (self->text_buffer, &line_start, display, -1);

            if (tag_name != NULL)
            {
                GtkTextIter tag_start;
                gtk_text_buffer_get_iter_at_offset (self->text_buffer,
                                                    &tag_start, offset);
                gtk_text_buffer_get_end_iter (self->text_buffer, &line_start);
                gtk_text_buffer_apply_tag_by_name (self->text_buffer,
                                                   tag_name,
                                                   &tag_start,
                                                   &line_start);
            }
        }
    }
}

static void
snap_diff_dialog_render_side_by_side (SnapDiffDialog *self)
{
    GtkTextIter iter;
    GList *pairs;
    GList *l;

    gtk_text_buffer_set_text (self->text_buffer, "", -1);

    if (self->diff_result == NULL)
    {
        return;
    }

    pairs = snap_diff_side_by_side (self->diff_result);

    gtk_text_buffer_get_start_iter (self->text_buffer, &iter);

    for (l = pairs; l != NULL; l = l->next)
    {
        SnapDiffSidePair *pair = l->data;
        GtkTextIter line_start;
        g_autofree gchar *display = NULL;
        const gchar *tag_name = NULL;
        gint offset;

        gtk_text_buffer_get_end_iter (self->text_buffer, &line_start);
        offset = gtk_text_iter_get_offset (&line_start);

        if (pair->left != NULL && pair->right != NULL)
        {
            display = g_strdup_printf ("%-40s | %s\n",
                                       pair->left->text,
                                       pair->right->text);
        }
        else if (pair->left != NULL)
        {
            display = g_strdup_printf ("%-40s | \n", pair->left->text);
            tag_name = "snap-deletion";
        }
        else if (pair->right != NULL)
        {
            display = g_strdup_printf ("%-40s | %s\n", "", pair->right->text);
            tag_name = "snap-addition";
        }

        if (display != NULL)
        {
            gtk_text_buffer_insert (self->text_buffer, &line_start,
                                    display, -1);

            if (tag_name != NULL)
            {
                GtkTextIter tag_start;
                gtk_text_buffer_get_iter_at_offset (self->text_buffer,
                                                    &tag_start, offset);
                gtk_text_buffer_get_end_iter (self->text_buffer, &line_start);
                gtk_text_buffer_apply_tag_by_name (self->text_buffer,
                                                   tag_name,
                                                   &tag_start,
                                                   &line_start);
            }
        }
    }

    g_list_free_full (pairs, (GDestroyNotify) snap_diff_side_pair_free);
}

static void
on_mode_toggled (GtkToggleButton *button,
                 gpointer         user_data)
{
    SnapDiffDialog *self = SNAP_DIFF_DIALOG (user_data);

    if (!gtk_toggle_button_get_active (button))
    {
        return;
    }

    if (GTK_WIDGET (button) == self->unified_toggle)
    {
        snap_diff_dialog_render_unified (self);
    }
    else
    {
        snap_diff_dialog_render_side_by_side (self);
    }
}

static void
snap_diff_dialog_load (SnapDiffDialog *self)
{
    g_autoptr (GError) error = NULL;
    SnapManager *manager;

    manager = snap_manager_get_default ();
    self->diff_result = snap_manager_diff_current (manager, self->file_path,
                                                    &error);

    if (error != NULL)
    {
        g_warning ("Failed to get diff: %s", error->message);
        return;
    }

    snap_diff_dialog_render_unified (self);
}

static void
snap_diff_dialog_dispose (GObject *object)
{
    SnapDiffDialog *self = SNAP_DIFF_DIALOG (object);

    g_clear_pointer (&self->file_path, g_free);
    g_clear_pointer (&self->diff_result, snap_diff_result_free);

    G_OBJECT_CLASS (snap_diff_dialog_parent_class)->dispose (object);
}

static void
snap_diff_dialog_finalize (GObject *object)
{
    G_OBJECT_CLASS (snap_diff_dialog_parent_class)->finalize (object);
}

static void
snap_diff_dialog_init (SnapDiffDialog *self)
{
    GtkWidget *content;
    GtkWidget *header_bar;
    GtkWidget *toggle_box;
    GtkWidget *scrolled_window;
    GtkTextTag *addition_tag;
    GtkTextTag *deletion_tag;
    GtkTextTagTable *tag_table;
    PangoFontDescription *font_desc;

    adw_dialog_set_title (ADW_DIALOG (self), _("Diff View"));
    adw_dialog_set_content_width (ADW_DIALOG (self), 700);
    adw_dialog_set_content_height (ADW_DIALOG (self), 500);

    content = adw_toolbar_view_new ();

    header_bar = adw_header_bar_new ();

    toggle_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class (toggle_box, "linked");

    self->unified_toggle = gtk_toggle_button_new_with_label (_("Unified"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->unified_toggle),
                                  TRUE);
    g_signal_connect (self->unified_toggle, "toggled",
                      G_CALLBACK (on_mode_toggled), self);
    gtk_box_append (GTK_BOX (toggle_box), self->unified_toggle);

    self->side_by_side_toggle = gtk_toggle_button_new_with_label (
        _("Side by Side"));
    gtk_toggle_button_set_group (
        GTK_TOGGLE_BUTTON (self->side_by_side_toggle),
        GTK_TOGGLE_BUTTON (self->unified_toggle));
    g_signal_connect (self->side_by_side_toggle, "toggled",
                      G_CALLBACK (on_mode_toggled), self);
    gtk_box_append (GTK_BOX (toggle_box), self->side_by_side_toggle);

    adw_header_bar_set_title_widget (ADW_HEADER_BAR (header_bar), toggle_box);
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (content), header_bar);

    scrolled_window = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);

    self->text_buffer = gtk_text_buffer_new (NULL);
    tag_table = gtk_text_buffer_get_tag_table (self->text_buffer);

    addition_tag = gtk_text_tag_new ("snap-addition");
    g_object_set (addition_tag,
                  "background", "#d4edda",
                  "foreground", "#155724",
                  NULL);
    gtk_text_tag_table_add (tag_table, addition_tag);
    g_object_unref (addition_tag);

    deletion_tag = gtk_text_tag_new ("snap-deletion");
    g_object_set (deletion_tag,
                  "background", "#f8d7da",
                  "foreground", "#721c24",
                  NULL);
    gtk_text_tag_table_add (tag_table, deletion_tag);
    g_object_unref (deletion_tag);

    self->text_view = gtk_text_view_new_with_buffer (self->text_buffer);
    gtk_text_view_set_editable (GTK_TEXT_VIEW (self->text_view), FALSE);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (self->text_view), FALSE);
    gtk_text_view_set_monospace (GTK_TEXT_VIEW (self->text_view), TRUE);
    gtk_text_view_set_left_margin (GTK_TEXT_VIEW (self->text_view), 8);
    gtk_text_view_set_right_margin (GTK_TEXT_VIEW (self->text_view), 8);
    gtk_text_view_set_top_margin (GTK_TEXT_VIEW (self->text_view), 8);
    gtk_text_view_set_bottom_margin (GTK_TEXT_VIEW (self->text_view), 8);

    font_desc = pango_font_description_from_string ("Monospace");
    gtk_widget_add_css_class (self->text_view, "monospace");
    pango_font_description_free (font_desc);

    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window),
                                   self->text_view);
    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (content), scrolled_window);
    adw_dialog_set_child (ADW_DIALOG (self), content);
}

static void
snap_diff_dialog_class_init (SnapDiffDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = snap_diff_dialog_dispose;
    object_class->finalize = snap_diff_dialog_finalize;
}

void
snap_diff_dialog_present (const gchar *file_path,
                          GtkWindow   *parent)
{
    SnapDiffDialog *self;

    g_return_if_fail (file_path != NULL);

    self = g_object_new (SNAP_TYPE_DIFF_DIALOG, NULL);
    self->file_path = g_strdup (file_path);

    snap_diff_dialog_load (self);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
}
