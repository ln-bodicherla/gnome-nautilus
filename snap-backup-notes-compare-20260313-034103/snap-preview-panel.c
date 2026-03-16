/* snap-preview-panel.c
 *
 * Quick file preview panel. Shows:
 * - Text files: syntax-highlighted content preview
 * - Images: scaled thumbnail preview
 * - Other files: metadata (size, type, modified date)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-preview-panel.h"
#include <gio/gio.h>

#define PREVIEW_MAX_TEXT_SIZE (256 * 1024) /* 256KB max for text preview */
#define PREVIEW_WIDTH 320

struct _SnapPreviewPanel
{
    GtkBox parent_instance;

    GtkWidget *header_label;
    GtkWidget *stack;

    /* Text preview */
    GtkWidget *text_scrolled;
    GtkWidget *text_view;

    /* Image preview */
    GtkWidget *image_scrolled;
    GtkWidget *image_picture;

    /* Metadata view */
    GtkWidget *meta_box;
    GtkWidget *meta_icon;
    GtkWidget *meta_name;
    GtkWidget *meta_type;
    GtkWidget *meta_size;
    GtkWidget *meta_modified;
    GtkWidget *meta_permissions;

    gchar *current_path;
};

G_DEFINE_FINAL_TYPE (SnapPreviewPanel, snap_preview_panel, GTK_TYPE_BOX)

static gchar *
format_size (guint64 size)
{
    if (size < 1024)
        return g_strdup_printf ("%" G_GUINT64_FORMAT " bytes", size);
    else if (size < 1024 * 1024)
        return g_strdup_printf ("%.1f KB", (gdouble) size / 1024.0);
    else if (size < 1024 * 1024 * 1024)
        return g_strdup_printf ("%.1f MB", (gdouble) size / (1024.0 * 1024.0));
    else
        return g_strdup_printf ("%.2f GB", (gdouble) size / (1024.0 * 1024.0 * 1024.0));
}

static gboolean
is_text_file (const gchar *content_type)
{
    if (content_type == NULL)
        return FALSE;
    return g_str_has_prefix (content_type, "text/") ||
           g_strcmp0 (content_type, "application/json") == 0 ||
           g_strcmp0 (content_type, "application/xml") == 0 ||
           g_strcmp0 (content_type, "application/javascript") == 0 ||
           g_strcmp0 (content_type, "application/x-shellscript") == 0 ||
           g_strcmp0 (content_type, "application/x-python") == 0;
}

static gboolean
is_image_file (const gchar *content_type)
{
    if (content_type == NULL)
        return FALSE;
    return g_str_has_prefix (content_type, "image/");
}

static void
show_text_preview (SnapPreviewPanel *self, const gchar *path)
{
    g_autoptr(GFile) file = g_file_new_for_path (path);
    g_autofree gchar *contents = NULL;
    gsize length;

    if (!g_file_load_contents (file, NULL, &contents, &length, NULL, NULL))
    {
        gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "meta");
        return;
    }

    /* Truncate if too large */
    if (length > PREVIEW_MAX_TEXT_SIZE)
    {
        contents[PREVIEW_MAX_TEXT_SIZE] = '\0';
        length = PREVIEW_MAX_TEXT_SIZE;
    }

    /* Validate UTF-8 */
    if (!g_utf8_validate (contents, length, NULL))
    {
        gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "meta");
        return;
    }

    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->text_view));
    gtk_text_buffer_set_text (buffer, contents, length);
    gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "text");
}

static void
show_image_preview (SnapPreviewPanel *self, const gchar *path)
{
    g_autoptr(GFile) file = g_file_new_for_path (path);
    GdkTexture *texture = gdk_texture_new_from_file (file, NULL);

    if (texture == NULL)
    {
        gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "meta");
        return;
    }

    gtk_picture_set_paintable (GTK_PICTURE (self->image_picture),
                                GDK_PAINTABLE (texture));
    gtk_picture_set_can_shrink (GTK_PICTURE (self->image_picture), TRUE);
    gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "image");

    g_object_unref (texture);
}

static void
show_metadata (SnapPreviewPanel *self, const gchar *path, GFileInfo *info)
{
    const gchar *content_type = g_file_info_get_content_type (info);
    g_autofree gchar *type_desc = content_type ? g_content_type_get_description (content_type) : g_strdup ("Unknown");

    guint64 size = g_file_info_get_size (info);
    g_autofree gchar *size_str = format_size (size);

    GDateTime *mtime = g_file_info_get_modification_date_time (info);
    g_autofree gchar *mtime_str = mtime ? g_date_time_format (mtime, "%Y-%m-%d %H:%M") : g_strdup ("Unknown");

    /* Icon */
    GIcon *icon = g_file_info_get_icon (info);
    if (icon != NULL)
        gtk_image_set_from_gicon (GTK_IMAGE (self->meta_icon), icon);

    g_autofree gchar *basename = g_path_get_basename (path);
    gtk_label_set_text (GTK_LABEL (self->meta_name), basename);
    gtk_label_set_text (GTK_LABEL (self->meta_type), type_desc);
    gtk_label_set_text (GTK_LABEL (self->meta_size), size_str);
    gtk_label_set_text (GTK_LABEL (self->meta_modified), mtime_str);

    if (mtime)
        g_date_time_unref (mtime);

    gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "meta");
}

static void
snap_preview_panel_finalize (GObject *object)
{
    SnapPreviewPanel *self = SNAP_PREVIEW_PANEL (object);
    g_free (self->current_path);
    G_OBJECT_CLASS (snap_preview_panel_parent_class)->finalize (object);
}

static void
snap_preview_panel_class_init (SnapPreviewPanelClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = snap_preview_panel_finalize;
}

static GtkWidget *
make_meta_row (const gchar *label_text, GtkWidget **value_label)
{
    GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start (row, 12);
    gtk_widget_set_margin_end (row, 12);
    gtk_widget_set_margin_top (row, 2);
    gtk_widget_set_margin_bottom (row, 2);

    GtkWidget *key = gtk_label_new (label_text);
    gtk_widget_add_css_class (key, "dim-label");
    gtk_widget_add_css_class (key, "caption");
    gtk_label_set_xalign (GTK_LABEL (key), 0);
    gtk_widget_set_size_request (key, 80, -1);
    gtk_box_append (GTK_BOX (row), key);

    *value_label = gtk_label_new ("");
    gtk_widget_add_css_class (*value_label, "caption");
    gtk_label_set_xalign (GTK_LABEL (*value_label), 0);
    gtk_label_set_wrap (GTK_LABEL (*value_label), TRUE);
    gtk_label_set_ellipsize (GTK_LABEL (*value_label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand (*value_label, TRUE);
    gtk_box_append (GTK_BOX (row), *value_label);

    return row;
}

static void
snap_preview_panel_init (SnapPreviewPanel *self)
{
    GtkWidget *meta_content;

    gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_size_request (GTK_WIDGET (self), PREVIEW_WIDTH, -1);

    /* Header */
    self->header_label = gtk_label_new ("Preview");
    gtk_widget_add_css_class (self->header_label, "heading");
    gtk_widget_set_margin_top (self->header_label, 8);
    gtk_widget_set_margin_bottom (self->header_label, 4);
    gtk_box_append (GTK_BOX (self), self->header_label);

    GtkWidget *sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append (GTK_BOX (self), sep);

    /* Stack for different preview types */
    self->stack = gtk_stack_new ();
    gtk_stack_set_transition_type (GTK_STACK (self->stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_widget_set_vexpand (self->stack, TRUE);

    /* Text preview page */
    self->text_scrolled = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->text_scrolled),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    self->text_view = gtk_text_view_new ();
    gtk_text_view_set_editable (GTK_TEXT_VIEW (self->text_view), FALSE);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (self->text_view), FALSE);
    gtk_text_view_set_monospace (GTK_TEXT_VIEW (self->text_view), TRUE);
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (self->text_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin (GTK_TEXT_VIEW (self->text_view), 8);
    gtk_text_view_set_right_margin (GTK_TEXT_VIEW (self->text_view), 8);
    gtk_text_view_set_top_margin (GTK_TEXT_VIEW (self->text_view), 4);

    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (self->text_scrolled), self->text_view);
    gtk_stack_add_named (GTK_STACK (self->stack), self->text_scrolled, "text");

    /* Image preview page */
    self->image_scrolled = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->image_scrolled),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    self->image_picture = gtk_picture_new ();
    gtk_picture_set_content_fit (GTK_PICTURE (self->image_picture), GTK_CONTENT_FIT_CONTAIN);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (self->image_scrolled), self->image_picture);
    gtk_stack_add_named (GTK_STACK (self->stack), self->image_scrolled, "image");

    /* Metadata page */
    meta_content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

    self->meta_icon = gtk_image_new ();
    gtk_image_set_pixel_size (GTK_IMAGE (self->meta_icon), 64);
    gtk_widget_set_margin_top (self->meta_icon, 24);
    gtk_widget_set_margin_bottom (self->meta_icon, 12);
    gtk_widget_set_halign (self->meta_icon, GTK_ALIGN_CENTER);
    gtk_box_append (GTK_BOX (meta_content), self->meta_icon);

    self->meta_name = gtk_label_new ("");
    gtk_widget_add_css_class (self->meta_name, "title-3");
    gtk_label_set_wrap (GTK_LABEL (self->meta_name), TRUE);
    gtk_label_set_justify (GTK_LABEL (self->meta_name), GTK_JUSTIFY_CENTER);
    gtk_widget_set_margin_bottom (self->meta_name, 16);
    gtk_box_append (GTK_BOX (meta_content), self->meta_name);

    GtkWidget *detail_sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_start (detail_sep, 12);
    gtk_widget_set_margin_end (detail_sep, 12);
    gtk_box_append (GTK_BOX (meta_content), detail_sep);

    gtk_box_append (GTK_BOX (meta_content), make_meta_row ("Type:", &self->meta_type));
    gtk_box_append (GTK_BOX (meta_content), make_meta_row ("Size:", &self->meta_size));
    gtk_box_append (GTK_BOX (meta_content), make_meta_row ("Modified:", &self->meta_modified));

    self->meta_box = meta_content;
    gtk_stack_add_named (GTK_STACK (self->stack), meta_content, "meta");

    /* Empty state */
    GtkWidget *empty_label = gtk_label_new ("Select a file to preview");
    gtk_widget_add_css_class (empty_label, "dim-label");
    gtk_stack_add_named (GTK_STACK (self->stack), empty_label, "empty");

    gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "empty");
    gtk_box_append (GTK_BOX (self), self->stack);
}

GtkWidget *
snap_preview_panel_new (void)
{
    return g_object_new (SNAP_TYPE_PREVIEW_PANEL, NULL);
}

void
snap_preview_panel_set_file (SnapPreviewPanel *self,
                              const gchar      *path)
{
    g_return_if_fail (SNAP_IS_PREVIEW_PANEL (self));
    g_return_if_fail (path != NULL);

    g_free (self->current_path);
    self->current_path = g_strdup (path);

    /* Update header */
    g_autofree gchar *basename = g_path_get_basename (path);
    gtk_label_set_text (GTK_LABEL (self->header_label), basename);

    /* Query file info */
    g_autoptr(GFile) file = g_file_new_for_path (path);
    g_autoptr(GFileInfo) info = g_file_query_info (
        file,
        G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
        G_FILE_ATTRIBUTE_STANDARD_SIZE ","
        G_FILE_ATTRIBUTE_STANDARD_ICON ","
        G_FILE_ATTRIBUTE_TIME_MODIFIED,
        G_FILE_QUERY_INFO_NONE, NULL, NULL);

    if (info == NULL)
    {
        gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "empty");
        return;
    }

    const gchar *content_type = g_file_info_get_content_type (info);

    if (is_image_file (content_type))
        show_image_preview (self, path);
    else if (is_text_file (content_type))
        show_text_preview (self, path);
    else
        show_metadata (self, path, info);
}

void
snap_preview_panel_clear (SnapPreviewPanel *self)
{
    g_return_if_fail (SNAP_IS_PREVIEW_PANEL (self));

    g_clear_pointer (&self->current_path, g_free);
    gtk_label_set_text (GTK_LABEL (self->header_label), "Preview");
    gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "empty");
}
