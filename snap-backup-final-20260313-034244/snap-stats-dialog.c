/* snap-stats-dialog.c
 *
 * Analytics dashboard showing total files, snapshots, store size.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-stats-dialog.h"
#include "snap-store.h"

struct _SnapStatsDialog
{
    AdwDialog parent_instance;

    /* Widgets */
    GtkWidget *total_files_row;
    GtkWidget *total_snapshots_row;
    GtkWidget *store_size_row;
};

G_DEFINE_FINAL_TYPE (SnapStatsDialog, snap_stats_dialog, ADW_TYPE_DIALOG)

static gchar *
format_size (gint64 size)
{
    if (size < 1024)
        return g_strdup_printf ("%" G_GINT64_FORMAT " B", size);
    else if (size < 1024 * 1024)
        return g_strdup_printf ("%.1f KB", (gdouble) size / 1024.0);
    else if (size < 1024 * 1024 * 1024)
        return g_strdup_printf ("%.1f MB", (gdouble) size / (1024.0 * 1024.0));
    else
        return g_strdup_printf ("%.2f GB", (gdouble) size / (1024.0 * 1024.0 * 1024.0));
}

static void
snap_stats_dialog_load (SnapStatsDialog *self)
{
    SnapStore *store = snap_store_get_default ();

    gint64 total_files = snap_store_get_total_files (store);
    gint64 total_snapshots = snap_store_get_total_snapshots (store);
    gint64 store_size = snap_store_get_store_size (store);

    g_autofree gchar *files_str = g_strdup_printf ("%" G_GINT64_FORMAT, total_files);
    g_autofree gchar *snaps_str = g_strdup_printf ("%" G_GINT64_FORMAT, total_snapshots);
    g_autofree gchar *size_str = format_size (store_size);

    adw_action_row_set_subtitle (ADW_ACTION_ROW (self->total_files_row), files_str);
    adw_action_row_set_subtitle (ADW_ACTION_ROW (self->total_snapshots_row), snaps_str);
    adw_action_row_set_subtitle (ADW_ACTION_ROW (self->store_size_row), size_str);
}

static void
snap_stats_dialog_class_init (SnapStatsDialogClass *klass)
{
}

static void
snap_stats_dialog_init (SnapStatsDialog *self)
{
    GtkWidget *toolbar_view;
    GtkWidget *header_bar;
    GtkWidget *content_box;
    GtkWidget *clamp;
    GtkWidget *stats_group;

    adw_dialog_set_title (ADW_DIALOG (self), "Statistics");
    adw_dialog_set_content_width (ADW_DIALOG (self), 400);
    adw_dialog_set_content_height (ADW_DIALOG (self), 300);

    toolbar_view = adw_toolbar_view_new ();
    header_bar = adw_header_bar_new ();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), header_bar);

    content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top (content_box, 24);
    gtk_widget_set_margin_bottom (content_box, 24);
    gtk_widget_set_margin_start (content_box, 12);
    gtk_widget_set_margin_end (content_box, 12);

    /* Stats group */
    stats_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (stats_group), "Store Overview");

    self->total_files_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->total_files_row), "Tracked Files");
    adw_action_row_add_prefix (ADW_ACTION_ROW (self->total_files_row),
                                gtk_image_new_from_icon_name ("document-open-symbolic"));
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (stats_group), self->total_files_row);

    self->total_snapshots_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->total_snapshots_row), "Total Snapshots");
    adw_action_row_add_prefix (ADW_ACTION_ROW (self->total_snapshots_row),
                                gtk_image_new_from_icon_name ("view-list-symbolic"));
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (stats_group), self->total_snapshots_row);

    self->store_size_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->store_size_row), "Store Size");
    adw_action_row_add_prefix (ADW_ACTION_ROW (self->store_size_row),
                                gtk_image_new_from_icon_name ("drive-harddisk-symbolic"));
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (stats_group), self->store_size_row);

    gtk_box_append (GTK_BOX (content_box), stats_group);

    clamp = adw_clamp_new ();
    adw_clamp_set_child (ADW_CLAMP (clamp), content_box);

    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view), clamp);
    adw_dialog_set_child (ADW_DIALOG (self), toolbar_view);
}

void
snap_stats_dialog_present (GtkWindow *parent)
{
    SnapStatsDialog *self;

    self = g_object_new (SNAP_TYPE_STATS_DIALOG, NULL);

    snap_stats_dialog_load (self);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
}
