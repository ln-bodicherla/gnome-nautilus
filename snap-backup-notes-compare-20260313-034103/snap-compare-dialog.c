/* snap-compare-dialog.c
 *
 * Dialog for comparing two selected files side by side.
 * Shows files in two text views with diff highlighting.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-compare-dialog.h"
#include <gio/gio.h>

struct _SnapCompareDialog
{
    AdwDialog parent_instance;

    GtkWidget *left_label;
    GtkWidget *right_label;
    GtkWidget *left_view;
    GtkWidget *right_view;
    GtkWidget *status_label;
};

G_DEFINE_FINAL_TYPE (SnapCompareDialog, snap_compare_dialog, ADW_TYPE_DIALOG)

static void
snap_compare_dialog_class_init (SnapCompareDialogClass *klass)
{
}

static void
snap_compare_dialog_init (SnapCompareDialog *self)
{
    GtkWidget *toolbar_view, *header_bar, *paned;
    GtkWidget *left_box, *right_box;
    GtkWidget *left_scroll, *right_scroll;

    adw_dialog_set_title (ADW_DIALOG (self), "Compare Files");
    adw_dialog_set_content_width (ADW_DIALOG (self), 900);
    adw_dialog_set_content_height (ADW_DIALOG (self), 600);

    toolbar_view = adw_toolbar_view_new ();
    header_bar = adw_header_bar_new ();

    self->status_label = gtk_label_new ("");
    gtk_widget_add_css_class (self->status_label, "dim-label");
    gtk_widget_add_css_class (self->status_label, "caption");
    adw_header_bar_set_title_widget (ADW_HEADER_BAR (header_bar), self->status_label);

    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), header_bar);

    /* Paned view */
    paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);

    /* Left side */
    left_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

    self->left_label = gtk_label_new ("");
    gtk_widget_add_css_class (self->left_label, "heading");
    gtk_widget_set_margin_start (self->left_label, 8);
    gtk_widget_set_margin_top (self->left_label, 4);
    gtk_widget_set_margin_bottom (self->left_label, 4);
    gtk_label_set_xalign (GTK_LABEL (self->left_label), 0);
    gtk_label_set_ellipsize (GTK_LABEL (self->left_label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_box_append (GTK_BOX (left_box), self->left_label);

    GtkWidget *sep1 = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append (GTK_BOX (left_box), sep1);

    left_scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (left_scroll),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand (left_scroll, TRUE);

    self->left_view = gtk_text_view_new ();
    gtk_text_view_set_editable (GTK_TEXT_VIEW (self->left_view), FALSE);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (self->left_view), FALSE);
    gtk_text_view_set_monospace (GTK_TEXT_VIEW (self->left_view), TRUE);
    gtk_text_view_set_left_margin (GTK_TEXT_VIEW (self->left_view), 8);
    gtk_text_view_set_top_margin (GTK_TEXT_VIEW (self->left_view), 4);

    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (left_scroll), self->left_view);
    gtk_box_append (GTK_BOX (left_box), left_scroll);

    /* Right side */
    right_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

    self->right_label = gtk_label_new ("");
    gtk_widget_add_css_class (self->right_label, "heading");
    gtk_widget_set_margin_start (self->right_label, 8);
    gtk_widget_set_margin_top (self->right_label, 4);
    gtk_widget_set_margin_bottom (self->right_label, 4);
    gtk_label_set_xalign (GTK_LABEL (self->right_label), 0);
    gtk_label_set_ellipsize (GTK_LABEL (self->right_label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_box_append (GTK_BOX (right_box), self->right_label);

    GtkWidget *sep2 = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append (GTK_BOX (right_box), sep2);

    right_scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (right_scroll),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand (right_scroll, TRUE);

    self->right_view = gtk_text_view_new ();
    gtk_text_view_set_editable (GTK_TEXT_VIEW (self->right_view), FALSE);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (self->right_view), FALSE);
    gtk_text_view_set_monospace (GTK_TEXT_VIEW (self->right_view), TRUE);
    gtk_text_view_set_left_margin (GTK_TEXT_VIEW (self->right_view), 8);
    gtk_text_view_set_top_margin (GTK_TEXT_VIEW (self->right_view), 4);

    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (right_scroll), self->right_view);
    gtk_box_append (GTK_BOX (right_box), right_scroll);

    gtk_paned_set_start_child (GTK_PANED (paned), left_box);
    gtk_paned_set_end_child (GTK_PANED (paned), right_box);
    gtk_paned_set_resize_start_child (GTK_PANED (paned), TRUE);
    gtk_paned_set_resize_end_child (GTK_PANED (paned), TRUE);

    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view), paned);
    adw_dialog_set_child (ADW_DIALOG (self), toolbar_view);
}

static void
highlight_differences (GtkTextView *view_a, GtkTextView *view_b,
                       const gchar *text_a, const gchar *text_b)
{
    GtkTextBuffer *buf_a = gtk_text_view_get_buffer (view_a);
    GtkTextBuffer *buf_b = gtk_text_view_get_buffer (view_b);

    gtk_text_buffer_set_text (buf_a, text_a, -1);
    gtk_text_buffer_set_text (buf_b, text_b, -1);

    /* Create tags for highlighting */
    GtkTextTag *add_tag = gtk_text_buffer_create_tag (buf_b, "added",
        "background", "#d4edda", NULL);
    GtkTextTag *del_tag = gtk_text_buffer_create_tag (buf_a, "deleted",
        "background", "#f8d7da", NULL);

    /* Simple line-by-line comparison */
    g_auto(GStrv) lines_a = g_strsplit (text_a, "\n", -1);
    g_auto(GStrv) lines_b = g_strsplit (text_b, "\n", -1);

    guint len_a = g_strv_length (lines_a);
    guint len_b = g_strv_length (lines_b);
    guint max_len = MAX (len_a, len_b);

    for (guint i = 0; i < max_len; i++)
    {
        const gchar *la = (i < len_a) ? lines_a[i] : NULL;
        const gchar *lb = (i < len_b) ? lines_b[i] : NULL;

        gboolean different = (la == NULL || lb == NULL || g_strcmp0 (la, lb) != 0);

        if (different)
        {
            if (la != NULL && i < len_a)
            {
                GtkTextIter start, end;
                gtk_text_buffer_get_iter_at_line (buf_a, &start, i);
                gtk_text_buffer_get_iter_at_line (buf_a, &end, i);
                if (!gtk_text_iter_ends_line (&end))
                    gtk_text_iter_forward_to_line_end (&end);
                gtk_text_buffer_apply_tag (buf_a, del_tag, &start, &end);
            }

            if (lb != NULL && i < len_b)
            {
                GtkTextIter start, end;
                gtk_text_buffer_get_iter_at_line (buf_b, &start, i);
                gtk_text_buffer_get_iter_at_line (buf_b, &end, i);
                if (!gtk_text_iter_ends_line (&end))
                    gtk_text_iter_forward_to_line_end (&end);
                gtk_text_buffer_apply_tag (buf_b, add_tag, &start, &end);
            }
        }
    }
}

void
snap_compare_dialog_present (const gchar *path_a,
                              const gchar *path_b,
                              GtkWindow   *parent)
{
    SnapCompareDialog *self = g_object_new (SNAP_TYPE_COMPARE_DIALOG, NULL);

    g_autofree gchar *name_a = g_path_get_basename (path_a);
    g_autofree gchar *name_b = g_path_get_basename (path_b);

    gtk_label_set_text (GTK_LABEL (self->left_label), name_a);
    gtk_label_set_text (GTK_LABEL (self->right_label), name_b);
    gtk_widget_set_tooltip_text (self->left_label, path_a);
    gtk_widget_set_tooltip_text (self->right_label, path_b);

    /* Load files */
    g_autoptr(GFile) file_a = g_file_new_for_path (path_a);
    g_autoptr(GFile) file_b = g_file_new_for_path (path_b);
    g_autofree gchar *content_a = NULL;
    g_autofree gchar *content_b = NULL;
    gsize len_a, len_b;

    gboolean ok_a = g_file_load_contents (file_a, NULL, &content_a, &len_a, NULL, NULL);
    gboolean ok_b = g_file_load_contents (file_b, NULL, &content_b, &len_b, NULL, NULL);

    if (!ok_a || !ok_b)
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), "Error loading files");
        adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
        return;
    }

    if (!g_utf8_validate (content_a, len_a, NULL) ||
        !g_utf8_validate (content_b, len_b, NULL))
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), "Binary files cannot be compared");
        adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
        return;
    }

    /* Count differences */
    g_auto(GStrv) la = g_strsplit (content_a, "\n", -1);
    g_auto(GStrv) lb = g_strsplit (content_b, "\n", -1);
    guint diffs = 0;
    guint max = MAX (g_strv_length (la), g_strv_length (lb));
    for (guint i = 0; i < max; i++)
    {
        const gchar *line_a = (i < g_strv_length (la)) ? la[i] : "";
        const gchar *line_b = (i < g_strv_length (lb)) ? lb[i] : "";
        if (g_strcmp0 (line_a, line_b) != 0)
            diffs++;
    }

    g_autofree gchar *status = NULL;
    if (diffs == 0)
        status = g_strdup ("Files are identical");
    else
        status = g_strdup_printf ("%u lines differ", diffs);
    gtk_label_set_text (GTK_LABEL (self->status_label), status);

    highlight_differences (GTK_TEXT_VIEW (self->left_view),
                           GTK_TEXT_VIEW (self->right_view),
                           content_a, content_b);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
}
