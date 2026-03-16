/* snap-undo-history.c
 *
 * Tracks file operations for visual undo/redo history.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-undo-history.h"

#define MAX_HISTORY 50

struct _SnapUndoHistory
{
    GtkBox parent_instance;

    GtkWidget *list_box;
    GtkWidget *clear_button;
    GtkWidget *empty_label;
    guint count;
};

G_DEFINE_FINAL_TYPE (SnapUndoHistory, snap_undo_history, GTK_TYPE_BOX)

static void
on_clear_clicked (GtkButton *button, gpointer user_data)
{
    snap_undo_history_clear (SNAP_UNDO_HISTORY (user_data));
}

static void
snap_undo_history_class_init (SnapUndoHistoryClass *klass)
{
}

static void
snap_undo_history_init (SnapUndoHistory *self)
{
    GtkWidget *header;
    GtkWidget *title;
    GtkWidget *scrolled;

    gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);

    /* Header */
    header = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_margin_start (header, 8);
    gtk_widget_set_margin_end (header, 8);
    gtk_widget_set_margin_top (header, 6);
    gtk_widget_set_margin_bottom (header, 4);

    title = gtk_label_new ("Operation History");
    gtk_widget_add_css_class (title, "heading");
    gtk_label_set_xalign (GTK_LABEL (title), 0);
    gtk_widget_set_hexpand (title, TRUE);
    gtk_box_append (GTK_BOX (header), title);

    self->clear_button = gtk_button_new_from_icon_name ("edit-clear-symbolic");
    gtk_widget_add_css_class (self->clear_button, "flat");
    gtk_widget_add_css_class (self->clear_button, "circular");
    gtk_widget_set_tooltip_text (self->clear_button, "Clear history");
    g_signal_connect (self->clear_button, "clicked", G_CALLBACK (on_clear_clicked), self);
    gtk_box_append (GTK_BOX (header), self->clear_button);

    gtk_box_append (GTK_BOX (self), header);

    GtkWidget *sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append (GTK_BOX (self), sep);

    /* List */
    scrolled = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand (scrolled, TRUE);

    self->list_box = gtk_list_box_new ();
    gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->list_box), GTK_SELECTION_NONE);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), self->list_box);

    gtk_box_append (GTK_BOX (self), scrolled);

    /* Empty state */
    self->empty_label = gtk_label_new ("No recent operations");
    gtk_widget_add_css_class (self->empty_label, "dim-label");
    gtk_widget_set_margin_top (self->empty_label, 24);
    gtk_list_box_set_placeholder (GTK_LIST_BOX (self->list_box), self->empty_label);

    self->count = 0;
}

GtkWidget *
snap_undo_history_new (void)
{
    return g_object_new (SNAP_TYPE_UNDO_HISTORY, NULL);
}

void
snap_undo_history_add (SnapUndoHistory *self,
                        const gchar     *operation,
                        const gchar     *detail)
{
    g_return_if_fail (SNAP_IS_UNDO_HISTORY (self));

    GtkWidget *row = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_start (row, 8);
    gtk_widget_set_margin_end (row, 8);
    gtk_widget_set_margin_top (row, 4);
    gtk_widget_set_margin_bottom (row, 4);

    GtkWidget *op_label = gtk_label_new (operation);
    gtk_widget_add_css_class (op_label, "caption");
    gtk_label_set_xalign (GTK_LABEL (op_label), 0);
    gtk_box_append (GTK_BOX (row), op_label);

    if (detail != NULL && *detail != '\0')
    {
        GtkWidget *detail_label = gtk_label_new (detail);
        gtk_widget_add_css_class (detail_label, "dim-label");
        gtk_widget_add_css_class (detail_label, "caption");
        gtk_label_set_xalign (GTK_LABEL (detail_label), 0);
        gtk_label_set_ellipsize (GTK_LABEL (detail_label), PANGO_ELLIPSIZE_MIDDLE);
        gtk_box_append (GTK_BOX (row), detail_label);
    }

    /* Timestamp */
    g_autoptr(GDateTime) now = g_date_time_new_now_local ();
    g_autofree gchar *time_str = g_date_time_format (now, "%H:%M:%S");
    GtkWidget *time_label = gtk_label_new (time_str);
    gtk_widget_add_css_class (time_label, "dim-label");
    gtk_widget_add_css_class (time_label, "caption");
    gtk_label_set_xalign (GTK_LABEL (time_label), 1);
    gtk_box_append (GTK_BOX (row), time_label);

    gtk_list_box_prepend (GTK_LIST_BOX (self->list_box), row);
    self->count++;

    /* Trim old entries */
    while (self->count > MAX_HISTORY)
    {
        GtkWidget *last = gtk_widget_get_last_child (self->list_box);
        if (last != NULL)
        {
            gtk_list_box_remove (GTK_LIST_BOX (self->list_box), last);
            self->count--;
        }
        else
            break;
    }
}

void
snap_undo_history_clear (SnapUndoHistory *self)
{
    g_return_if_fail (SNAP_IS_UNDO_HISTORY (self));

    GtkWidget *child;
    while ((child = gtk_widget_get_first_child (self->list_box)) != NULL)
        gtk_list_box_remove (GTK_LIST_BOX (self->list_box), child);

    self->count = 0;
}
