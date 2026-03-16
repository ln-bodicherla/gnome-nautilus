/* snap-history-dialog.c
 *
 * Dialog showing version history for a file.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib/gi18n.h>

#include "snap-history-dialog.h"
#include "snap-manager.h"

struct _SnapHistoryDialog
{
    AdwDialog parent_instance;

    GtkWidget *list_box;
    gchar     *file_path;
};

G_DEFINE_FINAL_TYPE (SnapHistoryDialog, snap_history_dialog, ADW_TYPE_DIALOG)

static void
on_diff_clicked (GtkButton *button,
                 gpointer   user_data)
{
    SnapHistoryDialog *self = SNAP_HISTORY_DIALOG (user_data);
    gint64 version;
    g_autoptr (GError) error = NULL;
    SnapManager *manager;
    SnapDiffResult *result;

    version = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "snap-version"));
    manager = snap_manager_get_default ();

    result = snap_manager_diff (manager, self->file_path, version - 1, version, &error);

    if (error != NULL)
    {
        g_warning ("Failed to get diff: %s", error->message);
    }

    g_clear_pointer (&result, snap_diff_result_free);
}

static void
on_restore_clicked (GtkButton *button,
                    gpointer   user_data)
{
    SnapHistoryDialog *self = SNAP_HISTORY_DIALOG (user_data);
    gint64 version;
    g_autoptr (GError) error = NULL;
    SnapManager *manager;

    version = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "snap-version"));
    manager = snap_manager_get_default ();

    snap_manager_restore (manager, self->file_path, version, NULL, &error);

    if (error != NULL)
    {
        g_warning ("Failed to restore snapshot: %s", error->message);
    }

    adw_dialog_close (ADW_DIALOG (self));
}

static GtkWidget *
create_history_row (SnapHistoryDialog *self,
                    SnapSnapshot      *snapshot)
{
    GtkWidget *row;
    GtkWidget *hbox;
    GtkWidget *vbox;
    GtkWidget *version_label;
    GtkWidget *time_label;
    GtkWidget *message_label;
    GtkWidget *pin_icon;
    GtkWidget *button_box;
    GtkWidget *diff_button;
    GtkWidget *restore_button;
    g_autofree gchar *version_text = NULL;
    g_autofree gchar *time_text = NULL;
    g_autoptr (GDateTime) dt = NULL;

    row = gtk_list_box_row_new ();

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top (hbox, 8);
    gtk_widget_set_margin_bottom (hbox, 8);
    gtk_widget_set_margin_start (hbox, 12);
    gtk_widget_set_margin_end (hbox, 12);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand (vbox, TRUE);

    version_text = g_strdup_printf (_("Version %ld"), (long) snapshot->version);
    version_label = gtk_label_new (version_text);
    gtk_label_set_xalign (GTK_LABEL (version_label), 0.0);
    gtk_widget_add_css_class (version_label, "heading");
    gtk_box_append (GTK_BOX (vbox), version_label);

    dt = g_date_time_new_from_unix_local (snapshot->timestamp);
    time_text = g_date_time_format (dt, "%Y-%m-%d %H:%M:%S");
    time_label = gtk_label_new (time_text);
    gtk_label_set_xalign (GTK_LABEL (time_label), 0.0);
    gtk_widget_add_css_class (time_label, "dim-label");
    gtk_box_append (GTK_BOX (vbox), time_label);

    if (snapshot->message != NULL && snapshot->message[0] != '\0')
    {
        message_label = gtk_label_new (snapshot->message);
        gtk_label_set_xalign (GTK_LABEL (message_label), 0.0);
        gtk_label_set_ellipsize (GTK_LABEL (message_label), PANGO_ELLIPSIZE_END);
        gtk_box_append (GTK_BOX (vbox), message_label);
    }

    gtk_box_append (GTK_BOX (hbox), vbox);

    if (snapshot->pinned)
    {
        pin_icon = gtk_image_new_from_icon_name ("view-pin-symbolic");
        gtk_widget_set_tooltip_text (pin_icon, _("Pinned"));
        gtk_box_append (GTK_BOX (hbox), pin_icon);
    }

    button_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

    diff_button = gtk_button_new_with_label (_("Diff"));
    gtk_widget_add_css_class (diff_button, "flat");
    g_object_set_data (G_OBJECT (diff_button), "snap-version",
                       GINT_TO_POINTER ((gint) snapshot->version));
    g_signal_connect (diff_button, "clicked",
                      G_CALLBACK (on_diff_clicked), self);
    gtk_box_append (GTK_BOX (button_box), diff_button);

    restore_button = gtk_button_new_with_label (_("Restore"));
    gtk_widget_add_css_class (restore_button, "flat");
    g_object_set_data (G_OBJECT (restore_button), "snap-version",
                       GINT_TO_POINTER ((gint) snapshot->version));
    g_signal_connect (restore_button, "clicked",
                      G_CALLBACK (on_restore_clicked), self);
    gtk_box_append (GTK_BOX (button_box), restore_button);

    gtk_box_append (GTK_BOX (hbox), button_box);

    gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), hbox);

    return row;
}

static void
snap_history_dialog_load (SnapHistoryDialog *self)
{
    g_autoptr (GError) error = NULL;
    SnapManager *manager;
    GList *history;
    GList *l;

    manager = snap_manager_get_default ();
    history = snap_manager_history (manager, self->file_path, -1, &error);

    if (error != NULL)
    {
        g_warning ("Failed to load history: %s", error->message);
        return;
    }

    for (l = history; l != NULL; l = l->next)
    {
        SnapSnapshot *snapshot = l->data;
        GtkWidget *row = create_history_row (self, snapshot);
        gtk_list_box_append (GTK_LIST_BOX (self->list_box), row);
    }

    g_list_free_full (history, (GDestroyNotify) snap_snapshot_free);
}

static void
snap_history_dialog_dispose (GObject *object)
{
    SnapHistoryDialog *self = SNAP_HISTORY_DIALOG (object);

    g_clear_pointer (&self->file_path, g_free);

    G_OBJECT_CLASS (snap_history_dialog_parent_class)->dispose (object);
}

static void
snap_history_dialog_finalize (GObject *object)
{
    G_OBJECT_CLASS (snap_history_dialog_parent_class)->finalize (object);
}

static void
snap_history_dialog_init (SnapHistoryDialog *self)
{
    GtkWidget *content;
    GtkWidget *header_bar;
    GtkWidget *scrolled_window;

    adw_dialog_set_title (ADW_DIALOG (self), _("Version History"));
    adw_dialog_set_content_width (ADW_DIALOG (self), 500);
    adw_dialog_set_content_height (ADW_DIALOG (self), 500);

    content = adw_toolbar_view_new ();

    header_bar = adw_header_bar_new ();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (content), header_bar);

    scrolled_window = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    self->list_box = gtk_list_box_new ();
    gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->list_box),
                                     GTK_SELECTION_NONE);
    gtk_widget_add_css_class (self->list_box, "boxed-list");
    gtk_widget_set_margin_top (self->list_box, 12);
    gtk_widget_set_margin_bottom (self->list_box, 12);
    gtk_widget_set_margin_start (self->list_box, 12);
    gtk_widget_set_margin_end (self->list_box, 12);

    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window),
                                   self->list_box);
    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (content), scrolled_window);
    adw_dialog_set_child (ADW_DIALOG (self), content);
}

static void
snap_history_dialog_class_init (SnapHistoryDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = snap_history_dialog_dispose;
    object_class->finalize = snap_history_dialog_finalize;
}

void
snap_history_dialog_present (const gchar *file_path,
                             GtkWindow   *parent)
{
    SnapHistoryDialog *self;

    g_return_if_fail (file_path != NULL);

    self = g_object_new (SNAP_TYPE_HISTORY_DIALOG, NULL);
    self->file_path = g_strdup (file_path);

    snap_history_dialog_load (self);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
}
