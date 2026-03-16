/* snap-split-view.c
 *
 * Dolphin-style split view (F3) with two directory panes side by side.
 * Shows a secondary file browser pane next to the main view.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-split-view.h"
#include <gio/gio.h>

struct _SnapSplitView
{
    GtkBox parent_instance;

    GtkWidget *revealer;
    GtkWidget *paned;
    GtkWidget *separator;

    /* Secondary pane */
    GtkWidget *secondary_box;
    GtkWidget *breadcrumb_bar;
    GtkWidget *scrolled_window;
    GtkWidget *list_view;
    GtkWidget *toolbar;
    GtkWidget *up_button;
    GtkWidget *path_label;
    GtkWidget *close_button;

    GListStore *file_store;
    gchar *current_path;

    SnapSplitViewNavigateCb navigate_cb;
    gpointer navigate_user_data;
};

G_DEFINE_FINAL_TYPE (SnapSplitView, snap_split_view, GTK_TYPE_BOX)

/* File item for the list */
#define SNAP_TYPE_FILE_ITEM (snap_file_item_get_type ())
G_DECLARE_FINAL_TYPE (SnapFileItem, snap_file_item, SNAP, FILE_ITEM, GObject)

struct _SnapFileItem
{
    GObject parent_instance;
    gchar *name;
    gchar *path;
    gboolean is_directory;
    guint64 size;
    gchar *modified;
};

G_DEFINE_FINAL_TYPE (SnapFileItem, snap_file_item, G_TYPE_OBJECT)

static void
snap_file_item_finalize (GObject *object)
{
    SnapFileItem *self = SNAP_FILE_ITEM (object);
    g_free (self->name);
    g_free (self->path);
    g_free (self->modified);
    G_OBJECT_CLASS (snap_file_item_parent_class)->finalize (object);
}

static void
snap_file_item_class_init (SnapFileItemClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = snap_file_item_finalize;
}

static void
snap_file_item_init (SnapFileItem *self)
{
}

static SnapFileItem *
snap_file_item_new (const gchar *name,
                    const gchar *path,
                    gboolean     is_directory,
                    guint64      size,
                    const gchar *modified)
{
    SnapFileItem *item = g_object_new (SNAP_TYPE_FILE_ITEM, NULL);
    item->name = g_strdup (name);
    item->path = g_strdup (path);
    item->is_directory = is_directory;
    item->size = size;
    item->modified = g_strdup (modified);
    return item;
}

static gchar *
format_size_short (guint64 size)
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
populate_directory (SnapSplitView *self, const gchar *path);

static void
on_item_activated (GtkListView *list_view,
                   guint        position,
                   gpointer     user_data)
{
    SnapSplitView *self = SNAP_SPLIT_VIEW (user_data);
    SnapFileItem *item = g_list_model_get_item (G_LIST_MODEL (self->file_store), position);

    if (item == NULL)
        return;

    if (item->is_directory)
    {
        populate_directory (self, item->path);

        if (self->navigate_cb != NULL)
            self->navigate_cb (item->path, self->navigate_user_data);
    }

    g_object_unref (item);
}

static void
on_up_clicked (GtkButton *button, gpointer user_data)
{
    SnapSplitView *self = SNAP_SPLIT_VIEW (user_data);

    if (self->current_path == NULL)
        return;

    g_autofree gchar *parent = g_path_get_dirname (self->current_path);
    if (parent != NULL && g_strcmp0 (parent, self->current_path) != 0)
        populate_directory (self, parent);
}

static void
on_close_clicked (GtkButton *button, gpointer user_data)
{
    SnapSplitView *self = SNAP_SPLIT_VIEW (user_data);
    snap_split_view_set_active (self, FALSE);
}

static void
setup_item_cb (GtkListItemFactory *factory,
               GtkListItem        *list_item)
{
    GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start (box, 6);
    gtk_widget_set_margin_end (box, 6);
    gtk_widget_set_margin_top (box, 2);
    gtk_widget_set_margin_bottom (box, 2);

    GtkWidget *icon = gtk_image_new ();
    gtk_image_set_pixel_size (GTK_IMAGE (icon), 16);
    gtk_box_append (GTK_BOX (box), icon);

    GtkWidget *name_label = gtk_label_new ("");
    gtk_label_set_xalign (GTK_LABEL (name_label), 0);
    gtk_widget_set_hexpand (name_label, TRUE);
    gtk_label_set_ellipsize (GTK_LABEL (name_label), PANGO_ELLIPSIZE_END);
    gtk_box_append (GTK_BOX (box), name_label);

    GtkWidget *size_label = gtk_label_new ("");
    gtk_widget_add_css_class (size_label, "dim-label");
    gtk_widget_add_css_class (size_label, "caption");
    gtk_box_append (GTK_BOX (box), size_label);

    gtk_list_item_set_child (list_item, box);
}

static void
bind_item_cb (GtkListItemFactory *factory,
              GtkListItem        *list_item)
{
    GtkWidget *box = gtk_list_item_get_child (list_item);
    SnapFileItem *item = gtk_list_item_get_item (list_item);

    GtkWidget *icon = gtk_widget_get_first_child (box);
    GtkWidget *name_label = gtk_widget_get_next_sibling (icon);
    GtkWidget *size_label = gtk_widget_get_next_sibling (name_label);

    if (item->is_directory)
        gtk_image_set_from_icon_name (GTK_IMAGE (icon), "folder-symbolic");
    else
        gtk_image_set_from_icon_name (GTK_IMAGE (icon), "text-x-generic-symbolic");

    gtk_label_set_text (GTK_LABEL (name_label), item->name);

    if (item->is_directory)
        gtk_label_set_text (GTK_LABEL (size_label), "");
    else
    {
        g_autofree gchar *size_str = format_size_short (item->size);
        gtk_label_set_text (GTK_LABEL (size_label), size_str);
    }
}

static void
populate_directory (SnapSplitView *self, const gchar *path)
{
    g_free (self->current_path);
    self->current_path = g_strdup (path);

    /* Update path label */
    g_autofree gchar *basename = g_path_get_basename (path);
    gtk_label_set_text (GTK_LABEL (self->path_label), path);
    gtk_widget_set_tooltip_text (self->path_label, path);

    /* Clear existing items */
    g_list_store_remove_all (self->file_store);

    /* Enumerate directory */
    GFile *dir = g_file_new_for_path (path);
    GFileEnumerator *enumerator = g_file_enumerate_children (
        dir,
        G_FILE_ATTRIBUTE_STANDARD_NAME ","
        G_FILE_ATTRIBUTE_STANDARD_TYPE ","
        G_FILE_ATTRIBUTE_STANDARD_SIZE ","
        G_FILE_ATTRIBUTE_TIME_MODIFIED,
        G_FILE_QUERY_INFO_NONE, NULL, NULL);

    if (enumerator == NULL)
    {
        g_object_unref (dir);
        return;
    }

    GFileInfo *info;
    while ((info = g_file_enumerator_next_file (enumerator, NULL, NULL)) != NULL)
    {
        const gchar *name = g_file_info_get_name (info);

        /* Skip hidden files */
        if (name[0] == '.')
        {
            g_object_unref (info);
            continue;
        }

        gboolean is_dir = g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY;
        guint64 size = g_file_info_get_size (info);
        g_autofree gchar *child_path = g_build_filename (path, name, NULL);

        SnapFileItem *item = snap_file_item_new (name, child_path, is_dir, size, "");
        g_list_store_append (self->file_store, item);
        g_object_unref (item);
        g_object_unref (info);
    }

    g_object_unref (enumerator);
    g_object_unref (dir);
}

static void
snap_split_view_finalize (GObject *object)
{
    SnapSplitView *self = SNAP_SPLIT_VIEW (object);
    g_free (self->current_path);
    G_OBJECT_CLASS (snap_split_view_parent_class)->finalize (object);
}

static void
snap_split_view_class_init (SnapSplitViewClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = snap_split_view_finalize;
}

static void
snap_split_view_init (SnapSplitView *self)
{
    GtkWidget *inner_box;
    GtkWidget *header_box;
    GtkListItemFactory *factory;
    GtkSingleSelection *selection;

    gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);

    /* Separator line */
    self->separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);

    /* Revealer wrapping the secondary pane */
    self->revealer = gtk_revealer_new ();
    gtk_revealer_set_transition_type (GTK_REVEALER (self->revealer),
                                      GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT);
    gtk_revealer_set_transition_duration (GTK_REVEALER (self->revealer), 200);
    gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), FALSE);

    inner_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request (inner_box, 300, -1);
    gtk_widget_set_hexpand (inner_box, TRUE);

    /* Header bar with path and buttons */
    header_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_margin_start (header_box, 6);
    gtk_widget_set_margin_end (header_box, 6);
    gtk_widget_set_margin_top (header_box, 4);
    gtk_widget_set_margin_bottom (header_box, 4);
    gtk_widget_add_css_class (header_box, "toolbar");

    self->up_button = gtk_button_new_from_icon_name ("go-up-symbolic");
    gtk_widget_add_css_class (self->up_button, "flat");
    gtk_widget_set_tooltip_text (self->up_button, "Go to parent directory");
    g_signal_connect (self->up_button, "clicked", G_CALLBACK (on_up_clicked), self);
    gtk_box_append (GTK_BOX (header_box), self->up_button);

    self->path_label = gtk_label_new ("");
    gtk_label_set_xalign (GTK_LABEL (self->path_label), 0);
    gtk_label_set_ellipsize (GTK_LABEL (self->path_label), PANGO_ELLIPSIZE_START);
    gtk_widget_set_hexpand (self->path_label, TRUE);
    gtk_widget_add_css_class (self->path_label, "caption");
    gtk_box_append (GTK_BOX (header_box), self->path_label);

    self->close_button = gtk_button_new_from_icon_name ("window-close-symbolic");
    gtk_widget_add_css_class (self->close_button, "flat");
    gtk_widget_add_css_class (self->close_button, "circular");
    gtk_widget_set_tooltip_text (self->close_button, "Close split view");
    g_signal_connect (self->close_button, "clicked", G_CALLBACK (on_close_clicked), self);
    gtk_box_append (GTK_BOX (header_box), self->close_button);

    gtk_box_append (GTK_BOX (inner_box), header_box);

    /* File list */
    self->file_store = g_list_store_new (SNAP_TYPE_FILE_ITEM);
    selection = gtk_single_selection_new (G_LIST_MODEL (self->file_store));

    factory = gtk_signal_list_item_factory_new ();
    g_signal_connect (factory, "setup", G_CALLBACK (setup_item_cb), NULL);
    g_signal_connect (factory, "bind", G_CALLBACK (bind_item_cb), NULL);

    self->list_view = gtk_list_view_new (GTK_SELECTION_MODEL (selection), factory);
    g_signal_connect (self->list_view, "activate", G_CALLBACK (on_item_activated), self);

    self->scrolled_window = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (self->scrolled_window),
                                    self->list_view);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolled_window),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand (self->scrolled_window, TRUE);

    gtk_box_append (GTK_BOX (inner_box), self->scrolled_window);

    gtk_revealer_set_child (GTK_REVEALER (self->revealer), inner_box);

    gtk_box_append (GTK_BOX (self), self->separator);
    gtk_box_append (GTK_BOX (self), self->revealer);

    gtk_widget_set_visible (self->separator, FALSE);
}

GtkWidget *
snap_split_view_new (void)
{
    return g_object_new (SNAP_TYPE_SPLIT_VIEW, NULL);
}

void
snap_split_view_set_active (SnapSplitView *self,
                             gboolean       split_active)
{
    g_return_if_fail (SNAP_IS_SPLIT_VIEW (self));

    gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), split_active);
    gtk_widget_set_visible (self->separator, split_active);

    if (split_active && self->current_path == NULL)
    {
        /* Default to home directory */
        const gchar *home = g_get_home_dir ();
        populate_directory (self, home);
    }
}

gboolean
snap_split_view_get_active (SnapSplitView *self)
{
    g_return_val_if_fail (SNAP_IS_SPLIT_VIEW (self), FALSE);
    return gtk_revealer_get_reveal_child (GTK_REVEALER (self->revealer));
}

void
snap_split_view_toggle (SnapSplitView *self)
{
    g_return_if_fail (SNAP_IS_SPLIT_VIEW (self));
    snap_split_view_set_active (self, !snap_split_view_get_active (self));
}

void
snap_split_view_set_navigate_callback (SnapSplitView            *self,
                                        SnapSplitViewNavigateCb   cb,
                                        gpointer                  user_data)
{
    g_return_if_fail (SNAP_IS_SPLIT_VIEW (self));
    self->navigate_cb = cb;
    self->navigate_user_data = user_data;
}

void
snap_split_view_set_directory (SnapSplitView *self,
                                const gchar   *path)
{
    g_return_if_fail (SNAP_IS_SPLIT_VIEW (self));
    g_return_if_fail (path != NULL);

    populate_directory (self, path);
}

const gchar *
snap_split_view_get_directory (SnapSplitView *self)
{
    g_return_val_if_fail (SNAP_IS_SPLIT_VIEW (self), NULL);
    return self->current_path;
}
