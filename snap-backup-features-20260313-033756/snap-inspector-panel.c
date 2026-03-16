/* snap-inspector-panel.c
 *
 * Snap inspector sidebar panel for GNOME Nautilus.
 * Displays file info and snap history in a 280px wide GtkBox.
 */

#include "snap-inspector-panel.h"
#include "snap-manager.h"
#include "snap-store.h"

#include <glib/gi18n.h>

struct _SnapInspectorPanel
{
    GtkBox      parent_instance;

    /* File info section */
    GtkLabel   *file_name_label;
    GtkLabel   *file_size_label;
    GtkLabel   *file_type_label;
    GtkLabel   *file_modified_label;

    /* Snap history section */
    GtkListBox *history_list;

    /* Internal state */
    gchar      *current_file_path;
};

G_DEFINE_FINAL_TYPE (SnapInspectorPanel, snap_inspector_panel, GTK_TYPE_BOX)

/* Forward declarations */
static void snap_inspector_panel_update_file_info (SnapInspectorPanel *self);
static void snap_inspector_panel_update_history   (SnapInspectorPanel *self);

static void
snap_inspector_panel_finalize (GObject *object)
{
    SnapInspectorPanel *self = SNAP_INSPECTOR_PANEL (object);

    g_clear_pointer (&self->current_file_path, g_free);

    G_OBJECT_CLASS (snap_inspector_panel_parent_class)->finalize (object);
}

static GtkWidget *
create_section_header (const gchar *title)
{
    GtkWidget *label;

    label = gtk_label_new (title);
    gtk_widget_add_css_class (label, "heading");
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_widget_set_margin_top (label, 12);
    gtk_widget_set_margin_bottom (label, 6);
    gtk_widget_set_margin_start (label, 8);
    gtk_widget_set_margin_end (label, 8);

    return label;
}

static GtkWidget *
create_info_row (const gchar *label_text,
                 GtkLabel   **value_label_out)
{
    GtkWidget *row;
    GtkWidget *name_label;
    GtkWidget *value_label;

    row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start (row, 8);
    gtk_widget_set_margin_end (row, 8);
    gtk_widget_set_margin_top (row, 2);
    gtk_widget_set_margin_bottom (row, 2);

    name_label = gtk_label_new (label_text);
    gtk_widget_add_css_class (name_label, "dim-label");
    gtk_label_set_xalign (GTK_LABEL (name_label), 0.0);
    gtk_widget_set_hexpand (name_label, FALSE);
    gtk_box_append (GTK_BOX (row), name_label);

    value_label = gtk_label_new ("--");
    gtk_label_set_xalign (GTK_LABEL (value_label), 1.0);
    gtk_label_set_ellipsize (GTK_LABEL (value_label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand (value_label, TRUE);
    gtk_box_append (GTK_BOX (row), value_label);

    if (value_label_out)
        *value_label_out = GTK_LABEL (value_label);

    return row;
}

static void
snap_inspector_panel_init (SnapInspectorPanel *self)
{
    GtkWidget *separator;

    gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_size_request (GTK_WIDGET (self), 280, -1);
    gtk_widget_add_css_class (GTK_WIDGET (self), "snap-inspector-panel");

    /* File info section header */
    gtk_box_append (GTK_BOX (self), create_section_header (_("File Info")));

    /* File info rows */
    gtk_box_append (GTK_BOX (self), create_info_row (_("Name:"), &self->file_name_label));
    gtk_box_append (GTK_BOX (self), create_info_row (_("Size:"), &self->file_size_label));
    gtk_box_append (GTK_BOX (self), create_info_row (_("Type:"), &self->file_type_label));
    gtk_box_append (GTK_BOX (self), create_info_row (_("Modified:"), &self->file_modified_label));

    /* Separator */
    separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top (separator, 8);
    gtk_widget_set_margin_bottom (separator, 4);
    gtk_box_append (GTK_BOX (self), separator);

    /* Snap history section header */
    gtk_box_append (GTK_BOX (self), create_section_header (_("Snap History")));

    /* History list */
    self->history_list = GTK_LIST_BOX (gtk_list_box_new ());
    gtk_list_box_set_selection_mode (self->history_list, GTK_SELECTION_NONE);
    gtk_list_box_set_placeholder (self->history_list,
                                 gtk_label_new (_("No snapshots")));
    gtk_widget_add_css_class (GTK_WIDGET (self->history_list), "boxed-list");
    gtk_widget_set_margin_start (GTK_WIDGET (self->history_list), 8);
    gtk_widget_set_margin_end (GTK_WIDGET (self->history_list), 8);
    gtk_widget_set_vexpand (GTK_WIDGET (self->history_list), TRUE);
    gtk_box_append (GTK_BOX (self), GTK_WIDGET (self->history_list));

    self->current_file_path = NULL;
}

static void
snap_inspector_panel_class_init (SnapInspectorPanelClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = snap_inspector_panel_finalize;
}

SnapInspectorPanel *
snap_inspector_panel_new (void)
{
    return g_object_new (SNAP_TYPE_INSPECTOR_PANEL, NULL);
}

static gchar *
format_file_size (gint64 size)
{
    if (size < 1024)
        return g_strdup_printf ("%"G_GINT64_FORMAT" B", size);
    else if (size < 1024 * 1024)
        return g_strdup_printf ("%.1f KB", (gdouble) size / 1024.0);
    else if (size < 1024 * 1024 * 1024)
        return g_strdup_printf ("%.1f MB", (gdouble) size / (1024.0 * 1024.0));
    else
        return g_strdup_printf ("%.1f GB", (gdouble) size / (1024.0 * 1024.0 * 1024.0));
}

static void
snap_inspector_panel_update_file_info (SnapInspectorPanel *self)
{
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFileInfo) info = NULL;
    g_autoptr (GError) error = NULL;

    if (self->current_file_path == NULL)
    {
        gtk_label_set_text (self->file_name_label, "--");
        gtk_label_set_text (self->file_size_label, "--");
        gtk_label_set_text (self->file_type_label, "--");
        gtk_label_set_text (self->file_modified_label, "--");
        return;
    }

    file = g_file_new_for_path (self->current_file_path);
    info = g_file_query_info (file,
                              G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
                              G_FILE_ATTRIBUTE_STANDARD_SIZE ","
                              G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
                              G_FILE_ATTRIBUTE_TIME_MODIFIED,
                              G_FILE_QUERY_INFO_NONE,
                              NULL,
                              &error);

    if (error != NULL)
    {
        g_warning ("snap-inspector-panel: failed to query file info: %s", error->message);
        return;
    }

    /* Name */
    gtk_label_set_text (self->file_name_label,
                        g_file_info_get_display_name (info));

    /* Size */
    {
        g_autofree gchar *size_str = format_file_size (
            g_file_info_get_size (info));
        gtk_label_set_text (self->file_size_label, size_str);
    }

    /* Type */
    {
        const gchar *content_type = g_file_info_get_content_type (info);
        g_autofree gchar *type_desc = g_content_type_get_description (content_type);
        gtk_label_set_text (self->file_type_label,
                            type_desc ? type_desc : content_type);
    }

    /* Modified date */
    {
        g_autoptr (GDateTime) mtime = g_file_info_get_modification_date_time (info);
        if (mtime != NULL)
        {
            g_autofree gchar *date_str = g_date_time_format (mtime, "%Y-%m-%d %H:%M");
            gtk_label_set_text (self->file_modified_label, date_str);
        }
        else
        {
            gtk_label_set_text (self->file_modified_label, "--");
        }
    }
}

static GtkWidget *
create_history_row (SnapSnapshot *snapshot)
{
    GtkWidget *row;
    GtkWidget *vbox;
    GtkWidget *message_label;
    GtkWidget *meta_label;
    g_autoptr (GDateTime) dt = NULL;
    g_autofree gchar *time_str = NULL;
    g_autofree gchar *meta_str = NULL;

    row = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_top (row, 6);
    gtk_widget_set_margin_bottom (row, 6);
    gtk_widget_set_margin_start (row, 6);
    gtk_widget_set_margin_end (row, 6);

    vbox = row;

    /* Snapshot message or version */
    if (snapshot->message != NULL && snapshot->message[0] != '\0')
    {
        message_label = gtk_label_new (snapshot->message);
    }
    else
    {
        g_autofree gchar *version_text = g_strdup_printf (
            _("Snapshot v%"G_GINT64_FORMAT), snapshot->version);
        message_label = gtk_label_new (version_text);
    }
    gtk_label_set_xalign (GTK_LABEL (message_label), 0.0);
    gtk_label_set_ellipsize (GTK_LABEL (message_label), PANGO_ELLIPSIZE_END);
    gtk_box_append (GTK_BOX (vbox), message_label);

    /* Timestamp and size metadata */
    dt = g_date_time_new_from_unix_local (snapshot->timestamp);
    if (dt != NULL)
        time_str = g_date_time_format (dt, "%Y-%m-%d %H:%M");
    else
        time_str = g_strdup ("--");

    meta_str = g_strdup_printf ("%s  |  %s",
                                time_str,
                                snapshot->pinned ? _("Pinned") : "");
    meta_label = gtk_label_new (meta_str);
    gtk_widget_add_css_class (meta_label, "dim-label");
    gtk_widget_add_css_class (meta_label, "caption");
    gtk_label_set_xalign (GTK_LABEL (meta_label), 0.0);
    gtk_box_append (GTK_BOX (vbox), meta_label);

    return row;
}

static void
snap_inspector_panel_update_history (SnapInspectorPanel *self)
{
    GtkWidget *child;
    SnapStore *store;
    g_autoptr (GError) error = NULL;
    GList *history = NULL;
    GList *l;

    /* Clear existing rows */
    while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->history_list))) != NULL)
        gtk_list_box_remove (self->history_list, child);

    if (self->current_file_path == NULL)
        return;

    store = snap_store_get_default ();
    if (store == NULL)
        return;

    history = snap_store_get_history (store,
                                     self->current_file_path,
                                     NULL, /* default branch */
                                     20,   /* limit */
                                     &error);

    if (error != NULL)
    {
        g_warning ("snap-inspector-panel: failed to get history: %s", error->message);
        return;
    }

    for (l = history; l != NULL; l = l->next)
    {
        SnapSnapshot *snapshot = l->data;
        GtkWidget *row_content = create_history_row (snapshot);
        gtk_list_box_append (self->history_list, row_content);
    }

    g_list_free_full (history, (GDestroyNotify) snap_snapshot_free);
}

void
snap_inspector_panel_set_file (SnapInspectorPanel *self,
                               const gchar        *file_path)
{
    g_return_if_fail (SNAP_IS_INSPECTOR_PANEL (self));

    if (g_strcmp0 (self->current_file_path, file_path) == 0)
        return;

    g_free (self->current_file_path);
    self->current_file_path = g_strdup (file_path);

    snap_inspector_panel_update_file_info (self);
    snap_inspector_panel_update_history (self);
}

void
snap_inspector_panel_toggle (gpointer window)
{
    GtkWidget *revealer;

    g_return_if_fail (GTK_IS_WINDOW (window));

    /* Look for a GtkRevealer child named "snap-inspector-revealer" */
    revealer = GTK_WIDGET (gtk_widget_get_template_child (GTK_WIDGET (window),
                                                          G_OBJECT_TYPE (window),
                                                          "snap_inspector_revealer"));

    if (revealer == NULL)
    {
        /* Fallback: search by name in the widget tree */
        for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (window));
             child != NULL;
             child = gtk_widget_get_next_sibling (child))
        {
            if (GTK_IS_REVEALER (child) &&
                g_strcmp0 (gtk_widget_get_name (child), "snap-inspector-revealer") == 0)
            {
                revealer = child;
                break;
            }
        }
    }

    if (revealer != NULL && GTK_IS_REVEALER (revealer))
    {
        gboolean visible = gtk_revealer_get_reveal_child (GTK_REVEALER (revealer));
        gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), !visible);
    }
    else
    {
        g_warning ("snap-inspector-panel: could not find snap-inspector-revealer in window");
    }
}
