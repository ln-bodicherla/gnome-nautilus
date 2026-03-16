/* snap-places-toolbar.c
 *
 * Dolphin-style places toolbar showing bookmarked locations
 * as clickable buttons in a horizontal bar.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-places-toolbar.h"

enum
{
    PLACE_ACTIVATED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _SnapPlacesToolbar
{
    GtkBox parent_instance;

    GtkWidget *revealer;
    GtkWidget *button_box;
};

G_DEFINE_FINAL_TYPE (SnapPlacesToolbar, snap_places_toolbar, GTK_TYPE_BOX)

typedef struct
{
    gchar *path;
    SnapPlacesToolbar *toolbar;
} PlaceData;

static void
place_data_free (gpointer data)
{
    PlaceData *pd = data;
    g_free (pd->path);
    g_free (pd);
}

static void
on_place_clicked (GtkButton *button, gpointer user_data)
{
    PlaceData *pd = user_data;
    g_signal_emit (pd->toolbar, signals[PLACE_ACTIVATED], 0, pd->path);
}

static void
snap_places_toolbar_class_init (SnapPlacesToolbarClass *klass)
{
    signals[PLACE_ACTIVATED] = g_signal_new (
        "place-activated",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
add_default_place (SnapPlacesToolbar *self,
                   const gchar       *label,
                   const gchar       *path,
                   const gchar       *icon_name)
{
    snap_places_toolbar_add_place (self, label, path, icon_name);
}

static void
snap_places_toolbar_init (SnapPlacesToolbar *self)
{
    gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);

    self->revealer = gtk_revealer_new ();
    gtk_revealer_set_transition_type (GTK_REVEALER (self->revealer),
                                      GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration (GTK_REVEALER (self->revealer), 150);
    gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), FALSE);

    self->button_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_set_margin_start (self->button_box, 4);
    gtk_widget_set_margin_end (self->button_box, 4);
    gtk_widget_set_margin_top (self->button_box, 2);
    gtk_widget_set_margin_bottom (self->button_box, 2);
    gtk_widget_add_css_class (self->button_box, "linked");

    gtk_revealer_set_child (GTK_REVEALER (self->revealer), self->button_box);
    gtk_box_append (GTK_BOX (self), self->revealer);

    /* Default places */
    add_default_place (self, "Home", g_get_home_dir (), "user-home-symbolic");
    add_default_place (self, "Desktop",
                       g_build_filename (g_get_home_dir (), "Desktop", NULL),
                       "user-desktop-symbolic");
    add_default_place (self, "Documents",
                       g_build_filename (g_get_home_dir (), "Documents", NULL),
                       "folder-documents-symbolic");
    add_default_place (self, "Downloads",
                       g_build_filename (g_get_home_dir (), "Downloads", NULL),
                       "folder-download-symbolic");
    add_default_place (self, "Music",
                       g_build_filename (g_get_home_dir (), "Music", NULL),
                       "folder-music-symbolic");
    add_default_place (self, "Pictures",
                       g_build_filename (g_get_home_dir (), "Pictures", NULL),
                       "folder-pictures-symbolic");
    add_default_place (self, "Videos",
                       g_build_filename (g_get_home_dir (), "Videos", NULL),
                       "folder-videos-symbolic");
    add_default_place (self, "/",  "/", "drive-harddisk-symbolic");
}

GtkWidget *
snap_places_toolbar_new (void)
{
    return g_object_new (SNAP_TYPE_PLACES_TOOLBAR, NULL);
}

void
snap_places_toolbar_set_visible (SnapPlacesToolbar *self,
                                   gboolean           visible)
{
    g_return_if_fail (SNAP_IS_PLACES_TOOLBAR (self));
    gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), visible);
}

void
snap_places_toolbar_add_place (SnapPlacesToolbar *self,
                                 const gchar       *name,
                                 const gchar       *path,
                                 const gchar       *icon_name)
{
    g_return_if_fail (SNAP_IS_PLACES_TOOLBAR (self));

    PlaceData *pd = g_new0 (PlaceData, 1);
    pd->path = g_strdup (path);
    pd->toolbar = self;

    GtkWidget *button = gtk_button_new ();
    gtk_widget_add_css_class (button, "flat");
    gtk_widget_add_css_class (button, "compact");

    GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);

    if (icon_name != NULL)
    {
        GtkWidget *icon = gtk_image_new_from_icon_name (icon_name);
        gtk_image_set_pixel_size (GTK_IMAGE (icon), 14);
        gtk_box_append (GTK_BOX (box), icon);
    }

    GtkWidget *label = gtk_label_new (name);
    gtk_widget_add_css_class (label, "caption");
    gtk_box_append (GTK_BOX (box), label);

    gtk_button_set_child (GTK_BUTTON (button), box);
    gtk_widget_set_tooltip_text (button, path);

    g_signal_connect_data (button, "clicked", G_CALLBACK (on_place_clicked),
                           pd, (GClosureNotify) place_data_free, 0);

    gtk_box_append (GTK_BOX (self->button_box), button);
}
