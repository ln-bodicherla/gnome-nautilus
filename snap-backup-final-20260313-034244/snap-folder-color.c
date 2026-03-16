/* snap-folder-color.c
 *
 * Custom folder colors system.
 * Persists to ~/.snapstore/folder_colors.json
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-folder-color.h"
#include <json-glib/json-glib.h>
#include <gio/gio.h>

struct _SnapFolderColor
{
    GObject parent_instance;
    GHashTable *colors; /* path -> color_name */
    gchar *file_path;
};

G_DEFINE_FINAL_TYPE (SnapFolderColor, snap_folder_color, G_TYPE_OBJECT)

static SnapFolderColor *default_instance = NULL;

static gchar *
get_storage_path (void)
{
    return g_build_filename (g_get_home_dir (), ".snapstore", "folder_colors.json", NULL);
}

static void
snap_folder_color_save (SnapFolderColor *self)
{
    JsonBuilder *builder = json_builder_new ();
    json_builder_begin_object (builder);

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init (&iter, self->colors);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        json_builder_set_member_name (builder, (const gchar *) key);
        json_builder_add_string_value (builder, (const gchar *) value);
    }

    json_builder_end_object (builder);

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
snap_folder_color_load (SnapFolderColor *self)
{
    JsonParser *parser = json_parser_new ();
    if (!json_parser_load_from_file (parser, self->file_path, NULL))
    {
        g_object_unref (parser);
        return;
    }

    JsonNode *root = json_parser_get_root (parser);
    if (root == NULL || !JSON_NODE_HOLDS_OBJECT (root))
    {
        g_object_unref (parser);
        return;
    }

    JsonObject *obj = json_node_get_object (root);
    GList *members = json_object_get_members (obj);
    for (GList *l = members; l != NULL; l = l->next)
    {
        const gchar *path = l->data;
        const gchar *color = json_object_get_string_member (obj, path);
        g_hash_table_insert (self->colors, g_strdup (path), g_strdup (color));
    }
    g_list_free (members);
    g_object_unref (parser);
}

static void
snap_folder_color_finalize (GObject *object)
{
    SnapFolderColor *self = SNAP_FOLDER_COLOR (object);
    g_hash_table_unref (self->colors);
    g_free (self->file_path);
    G_OBJECT_CLASS (snap_folder_color_parent_class)->finalize (object);
}

static void
snap_folder_color_class_init (SnapFolderColorClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = snap_folder_color_finalize;
}

static void
snap_folder_color_init (SnapFolderColor *self)
{
    self->colors = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    self->file_path = get_storage_path ();
    snap_folder_color_load (self);
}

SnapFolderColor *
snap_folder_color_get_default (void)
{
    if (default_instance == NULL)
        default_instance = g_object_new (SNAP_TYPE_FOLDER_COLOR, NULL);
    return default_instance;
}

void
snap_folder_color_set (SnapFolderColor *self,
                        const gchar     *path,
                        const gchar     *color_name)
{
    g_return_if_fail (SNAP_IS_FOLDER_COLOR (self));
    g_hash_table_insert (self->colors, g_strdup (path), g_strdup (color_name));
    snap_folder_color_save (self);
}

const gchar *
snap_folder_color_get (SnapFolderColor *self,
                        const gchar     *path)
{
    g_return_val_if_fail (SNAP_IS_FOLDER_COLOR (self), NULL);
    return g_hash_table_lookup (self->colors, path);
}

void
snap_folder_color_remove (SnapFolderColor *self,
                           const gchar     *path)
{
    g_return_if_fail (SNAP_IS_FOLDER_COLOR (self));
    g_hash_table_remove (self->colors, path);
    snap_folder_color_save (self);
}
