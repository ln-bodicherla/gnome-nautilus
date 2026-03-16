/* snap-disk-usage.c
 *
 * Disk usage analysis for current directory.
 * Scans directory tree and shows sorted list of largest items
 * with size bars.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-disk-usage.h"
#include <gio/gio.h>

#define MAX_ENTRIES 100

struct _SnapDiskUsage
{
    GtkBox parent_instance;

    GtkWidget *header_label;
    GtkWidget *total_label;
    GtkWidget *list_box;
    GtkWidget *spinner;
    GtkWidget *scrolled;

    gchar *current_path;
    guint64 total_size;
};

G_DEFINE_FINAL_TYPE (SnapDiskUsage, snap_disk_usage, GTK_TYPE_BOX)

typedef struct
{
    gchar *name;
    gchar *path;
    guint64 size;
    gboolean is_directory;
} DiskEntry;

static void
disk_entry_free (DiskEntry *entry)
{
    g_free (entry->name);
    g_free (entry->path);
    g_free (entry);
}

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

static guint64
get_directory_size (const gchar *path, int depth)
{
    if (depth > 10)
        return 0;

    guint64 total = 0;
    GFile *dir = g_file_new_for_path (path);
    GFileEnumerator *enumerator = g_file_enumerate_children (
        dir,
        G_FILE_ATTRIBUTE_STANDARD_NAME ","
        G_FILE_ATTRIBUTE_STANDARD_TYPE ","
        G_FILE_ATTRIBUTE_STANDARD_SIZE,
        G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);

    if (enumerator != NULL)
    {
        GFileInfo *info;
        while ((info = g_file_enumerator_next_file (enumerator, NULL, NULL)) != NULL)
        {
            const gchar *name = g_file_info_get_name (info);
            if (name[0] == '.')
            {
                g_object_unref (info);
                continue;
            }

            if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
            {
                g_autofree gchar *child_path = g_build_filename (path, name, NULL);
                total += get_directory_size (child_path, depth + 1);
            }
            else
            {
                total += g_file_info_get_size (info);
            }
            g_object_unref (info);
        }
        g_object_unref (enumerator);
    }

    g_object_unref (dir);
    return total;
}

static gint
compare_entries (gconstpointer a, gconstpointer b)
{
    const DiskEntry *ea = *(const DiskEntry **) a;
    const DiskEntry *eb = *(const DiskEntry **) b;

    if (ea->size > eb->size) return -1;
    if (ea->size < eb->size) return 1;
    return 0;
}

static void
scan_thread_func (GTask        *task,
                  gpointer      source_object,
                  gpointer      task_data,
                  GCancellable *cancellable)
{
    const gchar *path = task_data;
    GPtrArray *entries = g_ptr_array_new_with_free_func ((GDestroyNotify) disk_entry_free);

    GFile *dir = g_file_new_for_path (path);
    GFileEnumerator *enumerator = g_file_enumerate_children (
        dir,
        G_FILE_ATTRIBUTE_STANDARD_NAME ","
        G_FILE_ATTRIBUTE_STANDARD_TYPE ","
        G_FILE_ATTRIBUTE_STANDARD_SIZE,
        G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);

    if (enumerator != NULL)
    {
        GFileInfo *info;
        while ((info = g_file_enumerator_next_file (enumerator, NULL, NULL)) != NULL)
        {
            if (g_cancellable_is_cancelled (cancellable))
            {
                g_object_unref (info);
                break;
            }

            const gchar *name = g_file_info_get_name (info);
            if (name[0] == '.')
            {
                g_object_unref (info);
                continue;
            }

            DiskEntry *entry = g_new0 (DiskEntry, 1);
            entry->name = g_strdup (name);
            entry->path = g_build_filename (path, name, NULL);
            entry->is_directory = g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY;

            if (entry->is_directory)
                entry->size = get_directory_size (entry->path, 0);
            else
                entry->size = g_file_info_get_size (info);

            g_ptr_array_add (entries, entry);
            g_object_unref (info);
        }
        g_object_unref (enumerator);
    }

    g_object_unref (dir);

    /* Sort by size descending */
    g_ptr_array_sort (entries, compare_entries);

    g_task_return_pointer (task, entries, (GDestroyNotify) g_ptr_array_unref);
}

static void
scan_finished (GObject      *source,
               GAsyncResult *result,
               gpointer      user_data)
{
    SnapDiskUsage *self = SNAP_DISK_USAGE (user_data);
    GPtrArray *entries = g_task_propagate_pointer (G_TASK (result), NULL);

    gtk_widget_set_visible (self->spinner, FALSE);

    if (entries == NULL)
        return;

    /* Clear existing rows */
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child (self->list_box)) != NULL)
        gtk_list_box_remove (GTK_LIST_BOX (self->list_box), child);

    /* Calculate total */
    self->total_size = 0;
    for (guint i = 0; i < entries->len; i++)
    {
        DiskEntry *entry = g_ptr_array_index (entries, i);
        self->total_size += entry->size;
    }

    g_autofree gchar *total_str = format_size (self->total_size);
    g_autofree gchar *total_text = g_strdup_printf ("Total: %s", total_str);
    gtk_label_set_text (GTK_LABEL (self->total_label), total_text);

    /* Add rows */
    guint limit = MIN (entries->len, MAX_ENTRIES);
    for (guint i = 0; i < limit; i++)
    {
        DiskEntry *entry = g_ptr_array_index (entries, i);

        GtkWidget *row = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_margin_start (row, 8);
        gtk_widget_set_margin_end (row, 8);
        gtk_widget_set_margin_top (row, 3);
        gtk_widget_set_margin_bottom (row, 3);

        /* Name + size row */
        GtkWidget *info_row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);

        GtkWidget *icon = gtk_image_new_from_icon_name (
            entry->is_directory ? "folder-symbolic" : "text-x-generic-symbolic");
        gtk_image_set_pixel_size (GTK_IMAGE (icon), 14);
        gtk_box_append (GTK_BOX (info_row), icon);

        GtkWidget *name_label = gtk_label_new (entry->name);
        gtk_label_set_xalign (GTK_LABEL (name_label), 0);
        gtk_label_set_ellipsize (GTK_LABEL (name_label), PANGO_ELLIPSIZE_END);
        gtk_widget_set_hexpand (name_label, TRUE);
        gtk_widget_add_css_class (name_label, "caption");
        gtk_box_append (GTK_BOX (info_row), name_label);

        g_autofree gchar *size_str = format_size (entry->size);
        GtkWidget *size_label = gtk_label_new (size_str);
        gtk_widget_add_css_class (size_label, "dim-label");
        gtk_widget_add_css_class (size_label, "caption");
        gtk_box_append (GTK_BOX (info_row), size_label);

        gtk_box_append (GTK_BOX (row), info_row);

        /* Size bar */
        GtkWidget *progress = gtk_progress_bar_new ();
        gdouble fraction = (self->total_size > 0) ?
            (gdouble) entry->size / (gdouble) self->total_size : 0.0;
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress), fraction);
        gtk_widget_set_size_request (progress, -1, 4);
        gtk_box_append (GTK_BOX (row), progress);

        gtk_list_box_append (GTK_LIST_BOX (self->list_box), row);
    }

    g_ptr_array_unref (entries);
}

static void
snap_disk_usage_finalize (GObject *object)
{
    SnapDiskUsage *self = SNAP_DISK_USAGE (object);
    g_free (self->current_path);
    G_OBJECT_CLASS (snap_disk_usage_parent_class)->finalize (object);
}

static void
snap_disk_usage_class_init (SnapDiskUsageClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = snap_disk_usage_finalize;
}

static void
snap_disk_usage_init (SnapDiskUsage *self)
{
    GtkWidget *header_box;
    GtkWidget *sep;

    gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);

    /* Header */
    header_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_margin_start (header_box, 8);
    gtk_widget_set_margin_end (header_box, 8);
    gtk_widget_set_margin_top (header_box, 6);
    gtk_widget_set_margin_bottom (header_box, 4);

    self->header_label = gtk_label_new ("Disk Usage");
    gtk_widget_add_css_class (self->header_label, "heading");
    gtk_label_set_xalign (GTK_LABEL (self->header_label), 0);
    gtk_widget_set_hexpand (self->header_label, TRUE);
    gtk_box_append (GTK_BOX (header_box), self->header_label);

    self->spinner = gtk_spinner_new ();
    gtk_widget_set_visible (self->spinner, FALSE);
    gtk_box_append (GTK_BOX (header_box), self->spinner);

    self->total_label = gtk_label_new ("");
    gtk_widget_add_css_class (self->total_label, "dim-label");
    gtk_widget_add_css_class (self->total_label, "caption");
    gtk_box_append (GTK_BOX (header_box), self->total_label);

    gtk_box_append (GTK_BOX (self), header_box);

    sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append (GTK_BOX (self), sep);

    /* List */
    self->scrolled = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolled),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand (self->scrolled, TRUE);

    self->list_box = gtk_list_box_new ();
    gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->list_box), GTK_SELECTION_NONE);

    GtkWidget *placeholder = gtk_label_new ("Select a directory to analyze");
    gtk_widget_add_css_class (placeholder, "dim-label");
    gtk_widget_set_margin_top (placeholder, 24);
    gtk_list_box_set_placeholder (GTK_LIST_BOX (self->list_box), placeholder);

    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (self->scrolled), self->list_box);
    gtk_box_append (GTK_BOX (self), self->scrolled);
}

GtkWidget *
snap_disk_usage_new (void)
{
    return g_object_new (SNAP_TYPE_DISK_USAGE, NULL);
}

void
snap_disk_usage_scan_directory (SnapDiskUsage *self,
                                 const gchar   *path)
{
    g_return_if_fail (SNAP_IS_DISK_USAGE (self));
    g_return_if_fail (path != NULL);

    g_free (self->current_path);
    self->current_path = g_strdup (path);

    g_autofree gchar *basename = g_path_get_basename (path);
    g_autofree gchar *title = g_strdup_printf ("Disk Usage: %s", basename);
    gtk_label_set_text (GTK_LABEL (self->header_label), title);
    gtk_label_set_text (GTK_LABEL (self->total_label), "Scanning...");

    gtk_widget_set_visible (self->spinner, TRUE);
    gtk_spinner_start (GTK_SPINNER (self->spinner));

    GTask *task = g_task_new (self, NULL, scan_finished, self);
    g_task_set_task_data (task, g_strdup (path), g_free);
    g_task_run_in_thread (task, scan_thread_func);
    g_object_unref (task);
}
