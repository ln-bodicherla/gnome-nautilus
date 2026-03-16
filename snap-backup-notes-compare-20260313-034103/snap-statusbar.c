/* snap-statusbar.c
 *
 * Status bar showing file count, selection info, and free space.
 * Similar to Dolphin's bottom status bar.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-statusbar.h"

struct _SnapStatusbar
{
    GtkBox parent_instance;

    GtkWidget *file_count_label;
    GtkWidget *selection_label;
    GtkWidget *free_space_label;
    GtkWidget *filter_label;
};

G_DEFINE_FINAL_TYPE (SnapStatusbar, snap_statusbar, GTK_TYPE_BOX)

static gchar *
format_size (guint64 size)
{
    if (size < 1024)
        return g_strdup_printf ("%" G_GUINT64_FORMAT " B", size);
    else if (size < 1024 * 1024)
        return g_strdup_printf ("%.1f KB", (gdouble) size / 1024.0);
    else if (size < 1024 * 1024 * 1024)
        return g_strdup_printf ("%.1f MB", (gdouble) size / (1024.0 * 1024.0));
    else
        return g_strdup_printf ("%.2f GB", (gdouble) size / (1024.0 * 1024.0 * 1024.0));
}

static void
snap_statusbar_class_init (SnapStatusbarClass *klass)
{
}

static void
snap_statusbar_init (SnapStatusbar *self)
{
    GtkWidget *separator1;
    GtkWidget *separator2;

    gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_start (GTK_WIDGET (self), 8);
    gtk_widget_set_margin_end (GTK_WIDGET (self), 8);
    gtk_widget_set_margin_top (GTK_WIDGET (self), 2);
    gtk_widget_set_margin_bottom (GTK_WIDGET (self), 2);

    /* File count */
    self->file_count_label = gtk_label_new ("");
    gtk_widget_add_css_class (self->file_count_label, "snap-statusbar-label");
    gtk_label_set_xalign (GTK_LABEL (self->file_count_label), 0);
    gtk_box_append (GTK_BOX (self), self->file_count_label);

    separator1 = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_start (separator1, 8);
    gtk_widget_set_margin_end (separator1, 8);
    gtk_box_append (GTK_BOX (self), separator1);

    /* Selection info */
    self->selection_label = gtk_label_new ("");
    gtk_widget_add_css_class (self->selection_label, "snap-statusbar-label");
    gtk_widget_set_hexpand (self->selection_label, TRUE);
    gtk_label_set_xalign (GTK_LABEL (self->selection_label), 0);
    gtk_box_append (GTK_BOX (self), self->selection_label);

    /* Filter indicator */
    self->filter_label = gtk_label_new ("");
    gtk_widget_add_css_class (self->filter_label, "snap-statusbar-filter");
    gtk_widget_set_visible (self->filter_label, FALSE);
    gtk_box_append (GTK_BOX (self), self->filter_label);

    separator2 = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_start (separator2, 8);
    gtk_widget_set_margin_end (separator2, 8);
    gtk_box_append (GTK_BOX (self), separator2);

    /* Free space */
    self->free_space_label = gtk_label_new ("");
    gtk_widget_add_css_class (self->free_space_label, "snap-statusbar-label");
    gtk_label_set_xalign (GTK_LABEL (self->free_space_label), 1);
    gtk_box_append (GTK_BOX (self), self->free_space_label);
}

GtkWidget *
snap_statusbar_new (void)
{
    return g_object_new (SNAP_TYPE_STATUSBAR, NULL);
}

void
snap_statusbar_set_file_count (SnapStatusbar *self,
                                guint          total,
                                guint          hidden)
{
    g_return_if_fail (SNAP_IS_STATUSBAR (self));

    g_autofree gchar *text = NULL;
    if (hidden > 0)
        text = g_strdup_printf ("%u items (%u hidden)", total, hidden);
    else
        text = g_strdup_printf ("%u items", total);

    gtk_label_set_text (GTK_LABEL (self->file_count_label), text);
}

void
snap_statusbar_set_selection (SnapStatusbar *self,
                               guint          count,
                               guint64        total_size)
{
    g_return_if_fail (SNAP_IS_STATUSBAR (self));

    if (count == 0)
    {
        gtk_label_set_text (GTK_LABEL (self->selection_label), "");
        return;
    }

    g_autofree gchar *size_str = format_size (total_size);
    g_autofree gchar *text = g_strdup_printf ("%u selected (%s)", count, size_str);
    gtk_label_set_text (GTK_LABEL (self->selection_label), text);
}

void
snap_statusbar_set_free_space (SnapStatusbar *self,
                                guint64        free_bytes)
{
    g_return_if_fail (SNAP_IS_STATUSBAR (self));

    g_autofree gchar *size_str = format_size (free_bytes);
    g_autofree gchar *text = g_strdup_printf ("%s free", size_str);
    gtk_label_set_text (GTK_LABEL (self->free_space_label), text);
}

void
snap_statusbar_set_filter_text (SnapStatusbar *self,
                                 const gchar   *text)
{
    g_return_if_fail (SNAP_IS_STATUSBAR (self));

    if (text != NULL && *text != '\0')
    {
        g_autofree gchar *label = g_strdup_printf ("Filter: %s", text);
        gtk_label_set_text (GTK_LABEL (self->filter_label), label);
        gtk_widget_set_visible (self->filter_label, TRUE);
    }
    else
    {
        gtk_widget_set_visible (self->filter_label, FALSE);
    }
}
