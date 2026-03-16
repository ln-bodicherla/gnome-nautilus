/* snap-filter-bar.c
 *
 * Quick filter bar (like Dolphin's Ctrl+I).
 * Shows a text entry at the bottom that filters files by name.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-filter-bar.h"

enum
{
    FILTER_CHANGED,
    FILTER_CLOSED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _SnapFilterBar
{
    GtkBox parent_instance;

    GtkWidget *revealer;
    GtkWidget *entry;
    GtkWidget *close_button;
    GtkWidget *match_label;
};

G_DEFINE_FINAL_TYPE (SnapFilterBar, snap_filter_bar, GTK_TYPE_BOX)

static void
on_entry_changed (GtkEditable   *editable,
                  SnapFilterBar *self)
{
    const gchar *text = gtk_editable_get_text (editable);
    g_signal_emit (self, signals[FILTER_CHANGED], 0, text);
}

static void
on_close_clicked (GtkButton     *button,
                  SnapFilterBar *self)
{
    snap_filter_bar_hide (self);
    g_signal_emit (self, signals[FILTER_CLOSED], 0);
}

static gboolean
on_entry_key_pressed (GtkEventControllerKey *controller,
                      guint                  keyval,
                      guint                  keycode,
                      GdkModifierType        state,
                      SnapFilterBar         *self)
{
    if (keyval == GDK_KEY_Escape)
    {
        snap_filter_bar_hide (self);
        g_signal_emit (self, signals[FILTER_CLOSED], 0);
        return TRUE;
    }
    return FALSE;
}

static void
snap_filter_bar_class_init (SnapFilterBarClass *klass)
{
    signals[FILTER_CHANGED] = g_signal_new (
        "filter-changed",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1, G_TYPE_STRING);

    signals[FILTER_CLOSED] = g_signal_new (
        "filter-closed",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);
}

static void
snap_filter_bar_init (SnapFilterBar *self)
{
    GtkWidget *inner_box;
    GtkWidget *icon;
    GtkEventController *key_controller;

    gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);

    self->revealer = gtk_revealer_new ();
    gtk_revealer_set_transition_type (GTK_REVEALER (self->revealer),
                                      GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
    gtk_revealer_set_transition_duration (GTK_REVEALER (self->revealer), 150);
    gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), FALSE);

    inner_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_margin_start (inner_box, 8);
    gtk_widget_set_margin_end (inner_box, 8);
    gtk_widget_set_margin_top (inner_box, 2);
    gtk_widget_set_margin_bottom (inner_box, 2);
    gtk_widget_add_css_class (inner_box, "snap-filter-bar");

    /* Filter icon */
    icon = gtk_image_new_from_icon_name ("edit-find-symbolic");
    gtk_box_append (GTK_BOX (inner_box), icon);

    /* Filter entry */
    self->entry = gtk_entry_new ();
    gtk_widget_set_hexpand (self->entry, TRUE);
    gtk_entry_set_placeholder_text (GTK_ENTRY (self->entry), "Filter...");
    gtk_widget_add_css_class (self->entry, "snap-filter-entry");
    g_signal_connect (self->entry, "changed", G_CALLBACK (on_entry_changed), self);
    gtk_box_append (GTK_BOX (inner_box), self->entry);

    /* Escape key handler */
    key_controller = gtk_event_controller_key_new ();
    g_signal_connect (key_controller, "key-pressed", G_CALLBACK (on_entry_key_pressed), self);
    gtk_widget_add_controller (self->entry, key_controller);

    /* Match count label */
    self->match_label = gtk_label_new ("");
    gtk_widget_add_css_class (self->match_label, "dim-label");
    gtk_box_append (GTK_BOX (inner_box), self->match_label);

    /* Close button */
    self->close_button = gtk_button_new_from_icon_name ("window-close-symbolic");
    gtk_widget_add_css_class (self->close_button, "flat");
    gtk_widget_add_css_class (self->close_button, "circular");
    g_signal_connect (self->close_button, "clicked", G_CALLBACK (on_close_clicked), self);
    gtk_box_append (GTK_BOX (inner_box), self->close_button);

    gtk_revealer_set_child (GTK_REVEALER (self->revealer), inner_box);
    gtk_box_append (GTK_BOX (self), self->revealer);
}

GtkWidget *
snap_filter_bar_new (void)
{
    return g_object_new (SNAP_TYPE_FILTER_BAR, NULL);
}

const gchar *
snap_filter_bar_get_text (SnapFilterBar *self)
{
    g_return_val_if_fail (SNAP_IS_FILTER_BAR (self), NULL);
    return gtk_editable_get_text (GTK_EDITABLE (self->entry));
}

void
snap_filter_bar_show (SnapFilterBar *self)
{
    g_return_if_fail (SNAP_IS_FILTER_BAR (self));
    gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), TRUE);
    gtk_widget_grab_focus (self->entry);
}

void
snap_filter_bar_hide (SnapFilterBar *self)
{
    g_return_if_fail (SNAP_IS_FILTER_BAR (self));
    gtk_editable_set_text (GTK_EDITABLE (self->entry), "");
    gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), FALSE);
}

gboolean
snap_filter_bar_is_active (SnapFilterBar *self)
{
    g_return_val_if_fail (SNAP_IS_FILTER_BAR (self), FALSE);
    return gtk_revealer_get_reveal_child (GTK_REVEALER (self->revealer));
}
