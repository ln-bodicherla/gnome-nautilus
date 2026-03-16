/* snap-browse-dialog.c
 *
 * Interactive timeline browser for snapshot versions.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib/gi18n.h>

#include "snap-browse-dialog.h"
#include "snap-manager.h"

struct _SnapBrowseDialog
{
    AdwDialog parent_instance;

    GtkWidget     *text_view;
    GtkTextBuffer *text_buffer;
    GtkWidget     *version_label;
    GtkWidget     *prev_button;
    GtkWidget     *next_button;
    gchar         *file_path;
    GList         *snapshots;
    GList         *current;
};

G_DEFINE_FINAL_TYPE (SnapBrowseDialog, snap_browse_dialog, ADW_TYPE_DIALOG)

static void
snap_browse_dialog_show_version (SnapBrowseDialog *self)
{
    SnapSnapshot *snapshot;
    g_autoptr (GError) error = NULL;
    g_autofree gchar *content = NULL;
    g_autofree gchar *label_text = NULL;
    g_autofree gchar *time_text = NULL;
    g_autoptr (GDateTime) dt = NULL;
    SnapManager *manager;

    if (self->current == NULL)
    {
        return;
    }

    snapshot = self->current->data;
    manager = snap_manager_get_default ();

    content = snap_manager_show (manager, self->file_path,
                                 snapshot->version, NULL, &error);

    if (error != NULL)
    {
        g_warning ("Failed to load version content: %s", error->message);
        gtk_text_buffer_set_text (self->text_buffer,
                                  _("Error loading content."), -1);
    }
    else if (content != NULL)
    {
        gtk_text_buffer_set_text (self->text_buffer, content, -1);
    }

    dt = g_date_time_new_from_unix_local (snapshot->timestamp);
    time_text = g_date_time_format (dt, "%Y-%m-%d %H:%M:%S");
    label_text = g_strdup_printf (_("Version %ld - %s"),
                                  (long) snapshot->version, time_text);
    gtk_label_set_text (GTK_LABEL (self->version_label), label_text);

    gtk_widget_set_sensitive (self->prev_button, self->current->next != NULL);
    gtk_widget_set_sensitive (self->next_button, self->current->prev != NULL);
}

static void
on_prev_clicked (SnapBrowseDialog *self)
{
    if (self->current != NULL && self->current->next != NULL)
    {
        self->current = self->current->next;
        snap_browse_dialog_show_version (self);
    }
}

static void
on_next_clicked (SnapBrowseDialog *self)
{
    if (self->current != NULL && self->current->prev != NULL)
    {
        self->current = self->current->prev;
        snap_browse_dialog_show_version (self);
    }
}

static gboolean
on_key_pressed (GtkEventControllerKey *controller,
                guint                  keyval,
                guint                  keycode,
                GdkModifierType        state,
                gpointer               user_data)
{
    SnapBrowseDialog *self = SNAP_BROWSE_DIALOG (user_data);

    if (keyval == GDK_KEY_Left)
    {
        on_prev_clicked (self);
        return TRUE;
    }
    else if (keyval == GDK_KEY_Right)
    {
        on_next_clicked (self);
        return TRUE;
    }

    return FALSE;
}

static void
snap_browse_dialog_load (SnapBrowseDialog *self)
{
    g_autoptr (GError) error = NULL;
    SnapManager *manager;

    manager = snap_manager_get_default ();
    self->snapshots = snap_manager_history (manager, self->file_path, -1,
                                            &error);

    if (error != NULL)
    {
        g_warning ("Failed to load history: %s", error->message);
        return;
    }

    if (self->snapshots != NULL)
    {
        self->current = self->snapshots;
        snap_browse_dialog_show_version (self);
    }
}

static void
snap_browse_dialog_dispose (GObject *object)
{
    SnapBrowseDialog *self = SNAP_BROWSE_DIALOG (object);

    g_clear_pointer (&self->file_path, g_free);
    g_list_free_full (g_steal_pointer (&self->snapshots),
                      (GDestroyNotify) snap_snapshot_free);

    G_OBJECT_CLASS (snap_browse_dialog_parent_class)->dispose (object);
}

static void
snap_browse_dialog_finalize (GObject *object)
{
    G_OBJECT_CLASS (snap_browse_dialog_parent_class)->finalize (object);
}

static void
snap_browse_dialog_init (SnapBrowseDialog *self)
{
    GtkWidget *content;
    GtkWidget *header_bar;
    GtkWidget *nav_box;
    GtkWidget *scrolled_window;
    GtkEventController *key_controller;

    adw_dialog_set_title (ADW_DIALOG (self), _("Browse Versions"));
    adw_dialog_set_content_width (ADW_DIALOG (self), 700);
    adw_dialog_set_content_height (ADW_DIALOG (self), 500);

    content = adw_toolbar_view_new ();

    /* Header bar */
    header_bar = adw_header_bar_new ();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (content), header_bar);

    /* Navigation bar */
    nav_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign (nav_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top (nav_box, 8);
    gtk_widget_set_margin_bottom (nav_box, 8);

    self->prev_button = gtk_button_new_from_icon_name ("go-previous-symbolic");
    gtk_widget_set_tooltip_text (self->prev_button,
                                 _("Previous version (Left arrow)"));
    g_signal_connect_swapped (self->prev_button, "clicked",
                              G_CALLBACK (on_prev_clicked), self);
    gtk_box_append (GTK_BOX (nav_box), self->prev_button);

    self->version_label = gtk_label_new ("");
    gtk_widget_add_css_class (self->version_label, "heading");
    gtk_widget_set_hexpand (self->version_label, TRUE);
    gtk_box_append (GTK_BOX (nav_box), self->version_label);

    self->next_button = gtk_button_new_from_icon_name ("go-next-symbolic");
    gtk_widget_set_tooltip_text (self->next_button,
                                 _("Next version (Right arrow)"));
    g_signal_connect_swapped (self->next_button, "clicked",
                              G_CALLBACK (on_next_clicked), self);
    gtk_box_append (GTK_BOX (nav_box), self->next_button);

    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (content), nav_box);

    /* Text view */
    scrolled_window = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);

    self->text_buffer = gtk_text_buffer_new (NULL);
    self->text_view = gtk_text_view_new_with_buffer (self->text_buffer);
    gtk_text_view_set_editable (GTK_TEXT_VIEW (self->text_view), FALSE);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (self->text_view), FALSE);
    gtk_text_view_set_monospace (GTK_TEXT_VIEW (self->text_view), TRUE);
    gtk_text_view_set_left_margin (GTK_TEXT_VIEW (self->text_view), 8);
    gtk_text_view_set_right_margin (GTK_TEXT_VIEW (self->text_view), 8);
    gtk_text_view_set_top_margin (GTK_TEXT_VIEW (self->text_view), 8);
    gtk_text_view_set_bottom_margin (GTK_TEXT_VIEW (self->text_view), 8);

    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window),
                                   self->text_view);
    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (content), scrolled_window);
    adw_dialog_set_child (ADW_DIALOG (self), content);

    /* Keyboard navigation */
    key_controller = gtk_event_controller_key_new ();
    g_signal_connect (key_controller, "key-pressed",
                      G_CALLBACK (on_key_pressed), self);
    gtk_widget_add_controller (GTK_WIDGET (self), key_controller);
}

static void
snap_browse_dialog_class_init (SnapBrowseDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = snap_browse_dialog_dispose;
    object_class->finalize = snap_browse_dialog_finalize;
}

void
snap_browse_dialog_present (const gchar *file_path,
                            GtkWindow   *parent)
{
    SnapBrowseDialog *self;

    g_return_if_fail (file_path != NULL);

    self = g_object_new (SNAP_TYPE_BROWSE_DIALOG, NULL);
    self->file_path = g_strdup (file_path);

    snap_browse_dialog_load (self);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
}
