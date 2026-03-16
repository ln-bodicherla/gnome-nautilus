/* snap-blame-dialog.c
 *
 * Dialog for line-level version attribution view using GtkTextView.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-blame-dialog.h"
#include "snap-manager.h"

struct _SnapBlameDialog
{
    AdwDialog parent_instance;

    gchar *file_path;

    /* Widgets */
    GtkWidget *text_view;
    GtkWidget *status_label;
};

G_DEFINE_FINAL_TYPE (SnapBlameDialog, snap_blame_dialog, ADW_TYPE_DIALOG)

static void
snap_blame_dialog_load (SnapBlameDialog *self)
{
    SnapManager *manager = snap_manager_get_default ();
    g_autoptr (GError) error = NULL;
    GList *blame_lines;
    GtkTextBuffer *buffer;
    GtkTextIter iter;

    blame_lines = snap_manager_blame (manager, self->file_path, &error);
    if (error != NULL)
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), error->message);
        return;
    }

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->text_view));
    gtk_text_buffer_set_text (buffer, "", 0);
    gtk_text_buffer_get_start_iter (buffer, &iter);

    /* Create tags for version attribution */
    gtk_text_buffer_create_tag (buffer, "version-tag",
                                "foreground", "#888888",
                                "family", "monospace",
                                "scale", 0.85,
                                NULL);
    gtk_text_buffer_create_tag (buffer, "content-tag",
                                "family", "monospace",
                                NULL);

    for (GList *l = blame_lines; l != NULL; l = l->next)
    {
        SnapSnapshot *snap = l->data;

        /* Format: "v<version> | <line content>" */
        g_autofree gchar *prefix = g_strdup_printf ("v%-6" G_GINT64_FORMAT " | ", snap->version);
        gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, prefix, -1,
                                                   "version-tag", NULL);

        if (snap->message != NULL)
        {
            gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                                       snap->message, -1,
                                                       "content-tag", NULL);
        }

        gtk_text_buffer_insert (buffer, &iter, "\n", 1);
    }

    g_autofree gchar *status = g_strdup_printf ("%u lines attributed",
                                                  g_list_length (blame_lines));
    gtk_label_set_text (GTK_LABEL (self->status_label), status);

    g_list_free_full (blame_lines, (GDestroyNotify) snap_snapshot_free);
}

static void
snap_blame_dialog_finalize (GObject *object)
{
    SnapBlameDialog *self = SNAP_BLAME_DIALOG (object);

    g_free (self->file_path);

    G_OBJECT_CLASS (snap_blame_dialog_parent_class)->finalize (object);
}

static void
snap_blame_dialog_class_init (SnapBlameDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = snap_blame_dialog_finalize;
}

static void
snap_blame_dialog_init (SnapBlameDialog *self)
{
    GtkWidget *toolbar_view;
    GtkWidget *header_bar;
    GtkWidget *content_box;
    GtkWidget *scrolled;

    adw_dialog_set_title (ADW_DIALOG (self), "Blame");
    adw_dialog_set_content_width (ADW_DIALOG (self), 600);
    adw_dialog_set_content_height (ADW_DIALOG (self), 500);

    toolbar_view = adw_toolbar_view_new ();
    header_bar = adw_header_bar_new ();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), header_bar);

    content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

    /* Text view in scrolled window */
    scrolled = gtk_scrolled_window_new ();
    gtk_widget_set_vexpand (scrolled, TRUE);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    self->text_view = gtk_text_view_new ();
    gtk_text_view_set_editable (GTK_TEXT_VIEW (self->text_view), FALSE);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (self->text_view), FALSE);
    gtk_text_view_set_monospace (GTK_TEXT_VIEW (self->text_view), TRUE);
    gtk_text_view_set_left_margin (GTK_TEXT_VIEW (self->text_view), 6);
    gtk_text_view_set_right_margin (GTK_TEXT_VIEW (self->text_view), 6);
    gtk_text_view_set_top_margin (GTK_TEXT_VIEW (self->text_view), 6);
    gtk_text_view_set_bottom_margin (GTK_TEXT_VIEW (self->text_view), 6);

    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), self->text_view);
    gtk_box_append (GTK_BOX (content_box), scrolled);

    /* Status bar */
    self->status_label = gtk_label_new ("");
    gtk_widget_add_css_class (self->status_label, "dim-label");
    gtk_widget_set_margin_start (self->status_label, 12);
    gtk_widget_set_margin_end (self->status_label, 12);
    gtk_widget_set_margin_top (self->status_label, 6);
    gtk_widget_set_margin_bottom (self->status_label, 6);
    gtk_label_set_xalign (GTK_LABEL (self->status_label), 0);
    gtk_box_append (GTK_BOX (content_box), self->status_label);

    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view), content_box);
    adw_dialog_set_child (ADW_DIALOG (self), toolbar_view);
}

void
snap_blame_dialog_present (const gchar *file_path,
                           GtkWindow   *parent)
{
    SnapBlameDialog *self;

    g_return_if_fail (file_path != NULL);

    self = g_object_new (SNAP_TYPE_BLAME_DIALOG, NULL);
    self->file_path = g_strdup (file_path);

    snap_blame_dialog_load (self);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
}
