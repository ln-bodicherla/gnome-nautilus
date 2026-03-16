/* snap-log-dialog.c
 *
 * Global timeline log showing all snapshots across all files.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib/gi18n.h>

#include "snap-log-dialog.h"
#include "snap-manager.h"

struct _SnapLogDialog
{
    AdwDialog parent_instance;

    GtkWidget *list_box;
};

G_DEFINE_FINAL_TYPE (SnapLogDialog, snap_log_dialog, ADW_TYPE_DIALOG)

static GtkWidget *
create_date_header (const gchar *date_str)
{
    GtkWidget *label;

    label = gtk_label_new (date_str);
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_widget_add_css_class (label, "heading");
    gtk_widget_set_margin_top (label, 16);
    gtk_widget_set_margin_bottom (label, 4);
    gtk_widget_set_margin_start (label, 12);

    return label;
}

static GtkWidget *
create_log_row (SnapSnapshot *snapshot)
{
    GtkWidget *row;
    GtkWidget *hbox;
    GtkWidget *vbox;
    GtkWidget *file_label;
    GtkWidget *version_label;
    GtkWidget *time_label;
    GtkWidget *message_label;
    g_autofree gchar *version_text = NULL;
    g_autofree gchar *time_text = NULL;
    g_autofree gchar *basename = NULL;
    g_autoptr (GDateTime) dt = NULL;

    row = gtk_list_box_row_new ();

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top (hbox, 8);
    gtk_widget_set_margin_bottom (hbox, 8);
    gtk_widget_set_margin_start (hbox, 12);
    gtk_widget_set_margin_end (hbox, 12);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand (vbox, TRUE);

    /* File name */
    basename = g_path_get_basename (snapshot->file_path);
    file_label = gtk_label_new (basename);
    gtk_label_set_xalign (GTK_LABEL (file_label), 0.0);
    gtk_widget_add_css_class (file_label, "heading");
    gtk_box_append (GTK_BOX (vbox), file_label);

    /* Full path */
    GtkWidget *path_label = gtk_label_new (snapshot->file_path);
    gtk_label_set_xalign (GTK_LABEL (path_label), 0.0);
    gtk_label_set_ellipsize (GTK_LABEL (path_label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_add_css_class (path_label, "dim-label");
    gtk_widget_add_css_class (path_label, "caption");
    gtk_box_append (GTK_BOX (vbox), path_label);

    /* Message */
    if (snapshot->message != NULL && snapshot->message[0] != '\0')
    {
        message_label = gtk_label_new (snapshot->message);
        gtk_label_set_xalign (GTK_LABEL (message_label), 0.0);
        gtk_label_set_ellipsize (GTK_LABEL (message_label),
                                 PANGO_ELLIPSIZE_END);
        gtk_box_append (GTK_BOX (vbox), message_label);
    }

    gtk_box_append (GTK_BOX (hbox), vbox);

    /* Version badge */
    version_text = g_strdup_printf ("v%ld", (long) snapshot->version);
    version_label = gtk_label_new (version_text);
    gtk_widget_add_css_class (version_label, "dim-label");
    gtk_box_append (GTK_BOX (hbox), version_label);

    /* Timestamp */
    dt = g_date_time_new_from_unix_local (snapshot->timestamp);
    time_text = g_date_time_format (dt, "%H:%M");
    time_label = gtk_label_new (time_text);
    gtk_widget_add_css_class (time_label, "dim-label");
    gtk_box_append (GTK_BOX (hbox), time_label);

    /* Pin indicator */
    if (snapshot->pinned)
    {
        GtkWidget *pin_icon;
        pin_icon = gtk_image_new_from_icon_name ("view-pin-symbolic");
        gtk_widget_set_tooltip_text (pin_icon, _("Pinned"));
        gtk_box_append (GTK_BOX (hbox), pin_icon);
    }

    gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), hbox);

    return row;
}

static void
snap_log_dialog_load (SnapLogDialog *self)
{
    g_autoptr (GError) error = NULL;
    SnapManager *manager;
    GList *log_entries;
    GList *l;
    g_autofree gchar *current_date = NULL;

    manager = snap_manager_get_default ();
    log_entries = snap_manager_global_log (manager, -1, &error);

    if (error != NULL)
    {
        g_warning ("Failed to load global log: %s", error->message);
        return;
    }

    for (l = log_entries; l != NULL; l = l->next)
    {
        SnapSnapshot *snapshot = l->data;
        g_autoptr (GDateTime) dt = NULL;
        g_autofree gchar *date_str = NULL;

        dt = g_date_time_new_from_unix_local (snapshot->timestamp);
        date_str = g_date_time_format (dt, "%A, %B %e, %Y");

        /* Add date header when the date changes */
        if (current_date == NULL || g_strcmp0 (current_date, date_str) != 0)
        {
            GtkWidget *header = create_date_header (date_str);
            gtk_list_box_append (GTK_LIST_BOX (self->list_box), header);
            g_free (current_date);
            current_date = g_strdup (date_str);
        }

        GtkWidget *row = create_log_row (snapshot);
        gtk_list_box_append (GTK_LIST_BOX (self->list_box), row);
    }

    g_list_free_full (log_entries, (GDestroyNotify) snap_snapshot_free);
}

static void
snap_log_dialog_dispose (GObject *object)
{
    G_OBJECT_CLASS (snap_log_dialog_parent_class)->dispose (object);
}

static void
snap_log_dialog_finalize (GObject *object)
{
    G_OBJECT_CLASS (snap_log_dialog_parent_class)->finalize (object);
}

static void
snap_log_dialog_init (SnapLogDialog *self)
{
    GtkWidget *content;
    GtkWidget *header_bar;
    GtkWidget *scrolled_window;

    adw_dialog_set_title (ADW_DIALOG (self), _("Snapshot Log"));
    adw_dialog_set_content_width (ADW_DIALOG (self), 600);
    adw_dialog_set_content_height (ADW_DIALOG (self), 600);

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
snap_log_dialog_class_init (SnapLogDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = snap_log_dialog_dispose;
    object_class->finalize = snap_log_dialog_finalize;
}

void
snap_log_dialog_present (GtkWindow *parent)
{
    SnapLogDialog *self;

    self = g_object_new (SNAP_TYPE_LOG_DIALOG, NULL);

    snap_log_dialog_load (self);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
}
