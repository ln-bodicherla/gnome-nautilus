/* snap-recent-locations.c
 *
 * Tracks recently visited directories for quick navigation.
 * Stores up to 20 entries in ~/.snapstore/recent_locations.json
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-recent-locations.h"
#include <json-glib/json-glib.h>
#include <gio/gio.h>

#define MAX_ENTRIES 20

struct _SnapRecentLocations
{
    GObject parent_instance;
    GQueue *locations; /* gchar* paths, most recent first */
    gchar  *file_path;
};

G_DEFINE_FINAL_TYPE (SnapRecentLocations, snap_recent_locations, G_TYPE_OBJECT)

static SnapRecentLocations *default_instance = NULL;

static gchar *
get_storage_path (void)
{
    return g_build_filename (g_get_home_dir (), ".snapstore", "recent_locations.json", NULL);
}

static void
snap_recent_locations_save (SnapRecentLocations *self)
{
    JsonBuilder *builder = json_builder_new ();
    json_builder_begin_array (builder);

    for (GList *l = self->locations->head; l != NULL; l = l->next)
        json_builder_add_string_value (builder, (const gchar *) l->data);

    json_builder_end_array (builder);

    JsonGenerator *gen = json_generator_new ();
    JsonNode *root = json_builder_get_root (builder);
    json_generator_set_root (gen, root);
    json_generator_set_pretty (gen, TRUE);

    g_autofree gchar *dir = g_path_get_dirname (self->file_path);
    g_mkdir_with_parents (dir, 0755);

    json_generator_to_file (gen, self->file_path, NULL);

    json_node_unref (root);
    g_object_unref (gen);
    g_object_unref (builder);
}

static void
snap_recent_locations_load (SnapRecentLocations *self)
{
    JsonParser *parser = json_parser_new ();

    if (!json_parser_load_from_file (parser, self->file_path, NULL))
    {
        g_object_unref (parser);
        return;
    }

    JsonNode *root = json_parser_get_root (parser);
    if (root == NULL || !JSON_NODE_HOLDS_ARRAY (root))
    {
        g_object_unref (parser);
        return;
    }

    JsonArray *arr = json_node_get_array (root);
    guint len = json_array_get_length (arr);

    for (guint i = 0; i < len && i < MAX_ENTRIES; i++)
    {
        const gchar *path = json_array_get_string_element (arr, i);
        if (path != NULL)
            g_queue_push_tail (self->locations, g_strdup (path));
    }

    g_object_unref (parser);
}

static void
snap_recent_locations_finalize (GObject *object)
{
    SnapRecentLocations *self = SNAP_RECENT_LOCATIONS (object);

    g_queue_free_full (self->locations, g_free);
    g_free (self->file_path);

    G_OBJECT_CLASS (snap_recent_locations_parent_class)->finalize (object);
}

static void
snap_recent_locations_class_init (SnapRecentLocationsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = snap_recent_locations_finalize;
}

static void
snap_recent_locations_init (SnapRecentLocations *self)
{
    self->locations = g_queue_new ();
    self->file_path = get_storage_path ();
    snap_recent_locations_load (self);
}

SnapRecentLocations *
snap_recent_locations_get_default (void)
{
    if (default_instance == NULL)
        default_instance = g_object_new (SNAP_TYPE_RECENT_LOCATIONS, NULL);
    return default_instance;
}

void
snap_recent_locations_add (SnapRecentLocations *self,
                            const gchar         *path)
{
    g_return_if_fail (SNAP_IS_RECENT_LOCATIONS (self));
    g_return_if_fail (path != NULL);

    /* Remove if already present */
    for (GList *l = self->locations->head; l != NULL; l = l->next)
    {
        if (g_strcmp0 ((const gchar *) l->data, path) == 0)
        {
            g_free (l->data);
            g_queue_delete_link (self->locations, l);
            break;
        }
    }

    /* Add to front */
    g_queue_push_head (self->locations, g_strdup (path));

    /* Trim to max */
    while (g_queue_get_length (self->locations) > MAX_ENTRIES)
    {
        gchar *old = g_queue_pop_tail (self->locations);
        g_free (old);
    }

    snap_recent_locations_save (self);
}

GList *
snap_recent_locations_get_list (SnapRecentLocations *self,
                                 guint                max_entries)
{
    g_return_val_if_fail (SNAP_IS_RECENT_LOCATIONS (self), NULL);

    GList *result = NULL;
    guint count = 0;

    for (GList *l = self->locations->head; l != NULL && count < max_entries; l = l->next, count++)
        result = g_list_prepend (result, g_strdup ((const gchar *) l->data));

    return g_list_reverse (result);
}

void
snap_recent_locations_clear (SnapRecentLocations *self)
{
    g_return_if_fail (SNAP_IS_RECENT_LOCATIONS (self));

    g_queue_free_full (self->locations, g_free);
    self->locations = g_queue_new ();
    snap_recent_locations_save (self);
}
