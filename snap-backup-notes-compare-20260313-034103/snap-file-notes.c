/* snap-file-notes.c
 *
 * Per-file notes and tags system.
 * Stores data in ~/.snapstore/notes.json
 *
 * JSON structure:
 * {
 *   "/path/to/file": {
 *     "note": "some note text",
 *     "tags": ["tag1", "tag2"]
 *   }
 * }
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-file-notes.h"
#include <json-glib/json-glib.h>
#include <gio/gio.h>

struct _SnapFileNotes
{
    GObject parent_instance;
    GHashTable *entries; /* path -> NoteEntry */
    gchar *file_path;
};

typedef struct
{
    gchar *note;
    GPtrArray *tags; /* gchar* */
} NoteEntry;

G_DEFINE_FINAL_TYPE (SnapFileNotes, snap_file_notes, G_TYPE_OBJECT)

static SnapFileNotes *default_instance = NULL;

static NoteEntry *
note_entry_new (void)
{
    NoteEntry *entry = g_new0 (NoteEntry, 1);
    entry->tags = g_ptr_array_new_with_free_func (g_free);
    return entry;
}

static void
note_entry_free (NoteEntry *entry)
{
    g_free (entry->note);
    g_ptr_array_unref (entry->tags);
    g_free (entry);
}

static gchar *
get_storage_path (void)
{
    return g_build_filename (g_get_home_dir (), ".snapstore", "notes.json", NULL);
}

static void
snap_file_notes_save (SnapFileNotes *self)
{
    JsonBuilder *builder = json_builder_new ();
    json_builder_begin_object (builder);

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init (&iter, self->entries);

    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        NoteEntry *entry = value;
        json_builder_set_member_name (builder, (const gchar *) key);
        json_builder_begin_object (builder);

        if (entry->note != NULL)
        {
            json_builder_set_member_name (builder, "note");
            json_builder_add_string_value (builder, entry->note);
        }

        if (entry->tags->len > 0)
        {
            json_builder_set_member_name (builder, "tags");
            json_builder_begin_array (builder);
            for (guint i = 0; i < entry->tags->len; i++)
                json_builder_add_string_value (builder, g_ptr_array_index (entry->tags, i));
            json_builder_end_array (builder);
        }

        json_builder_end_object (builder);
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
snap_file_notes_load (SnapFileNotes *self)
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
        JsonObject *entry_obj = json_object_get_object_member (obj, path);

        if (entry_obj == NULL)
            continue;

        NoteEntry *entry = note_entry_new ();

        if (json_object_has_member (entry_obj, "note"))
            entry->note = g_strdup (json_object_get_string_member (entry_obj, "note"));

        if (json_object_has_member (entry_obj, "tags"))
        {
            JsonArray *tags = json_object_get_array_member (entry_obj, "tags");
            guint len = json_array_get_length (tags);
            for (guint i = 0; i < len; i++)
                g_ptr_array_add (entry->tags,
                                 g_strdup (json_array_get_string_element (tags, i)));
        }

        g_hash_table_insert (self->entries, g_strdup (path), entry);
    }

    g_list_free (members);
    g_object_unref (parser);
}

static void
snap_file_notes_finalize (GObject *object)
{
    SnapFileNotes *self = SNAP_FILE_NOTES (object);
    g_hash_table_unref (self->entries);
    g_free (self->file_path);
    G_OBJECT_CLASS (snap_file_notes_parent_class)->finalize (object);
}

static void
snap_file_notes_class_init (SnapFileNotesClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = snap_file_notes_finalize;
}

static void
snap_file_notes_init (SnapFileNotes *self)
{
    self->entries = g_hash_table_new_full (g_str_hash, g_str_equal,
                                            g_free, (GDestroyNotify) note_entry_free);
    self->file_path = get_storage_path ();
    snap_file_notes_load (self);
}

SnapFileNotes *
snap_file_notes_get_default (void)
{
    if (default_instance == NULL)
        default_instance = g_object_new (SNAP_TYPE_FILE_NOTES, NULL);
    return default_instance;
}

static NoteEntry *
ensure_entry (SnapFileNotes *self, const gchar *path)
{
    NoteEntry *entry = g_hash_table_lookup (self->entries, path);
    if (entry == NULL)
    {
        entry = note_entry_new ();
        g_hash_table_insert (self->entries, g_strdup (path), entry);
    }
    return entry;
}

void
snap_file_notes_set_note (SnapFileNotes *self,
                           const gchar   *path,
                           const gchar   *note)
{
    g_return_if_fail (SNAP_IS_FILE_NOTES (self));

    NoteEntry *entry = ensure_entry (self, path);
    g_free (entry->note);
    entry->note = g_strdup (note);
    snap_file_notes_save (self);
}

const gchar *
snap_file_notes_get_note (SnapFileNotes *self,
                           const gchar   *path)
{
    g_return_val_if_fail (SNAP_IS_FILE_NOTES (self), NULL);

    NoteEntry *entry = g_hash_table_lookup (self->entries, path);
    return entry ? entry->note : NULL;
}

void
snap_file_notes_remove_note (SnapFileNotes *self,
                              const gchar   *path)
{
    g_return_if_fail (SNAP_IS_FILE_NOTES (self));

    g_hash_table_remove (self->entries, path);
    snap_file_notes_save (self);
}

void
snap_file_notes_add_tag (SnapFileNotes *self,
                          const gchar   *path,
                          const gchar   *tag)
{
    g_return_if_fail (SNAP_IS_FILE_NOTES (self));

    NoteEntry *entry = ensure_entry (self, path);

    /* Check if tag already exists */
    for (guint i = 0; i < entry->tags->len; i++)
    {
        if (g_strcmp0 (g_ptr_array_index (entry->tags, i), tag) == 0)
            return;
    }

    g_ptr_array_add (entry->tags, g_strdup (tag));
    snap_file_notes_save (self);
}

void
snap_file_notes_remove_tag (SnapFileNotes *self,
                             const gchar   *path,
                             const gchar   *tag)
{
    g_return_if_fail (SNAP_IS_FILE_NOTES (self));

    NoteEntry *entry = g_hash_table_lookup (self->entries, path);
    if (entry == NULL)
        return;

    for (guint i = 0; i < entry->tags->len; i++)
    {
        if (g_strcmp0 (g_ptr_array_index (entry->tags, i), tag) == 0)
        {
            g_ptr_array_remove_index (entry->tags, i);
            snap_file_notes_save (self);
            return;
        }
    }
}

GList *
snap_file_notes_get_tags (SnapFileNotes *self,
                           const gchar   *path)
{
    g_return_val_if_fail (SNAP_IS_FILE_NOTES (self), NULL);

    NoteEntry *entry = g_hash_table_lookup (self->entries, path);
    if (entry == NULL)
        return NULL;

    GList *result = NULL;
    for (guint i = 0; i < entry->tags->len; i++)
        result = g_list_prepend (result, g_strdup (g_ptr_array_index (entry->tags, i)));

    return g_list_reverse (result);
}

gboolean
snap_file_notes_has_tag (SnapFileNotes *self,
                          const gchar   *path,
                          const gchar   *tag)
{
    g_return_val_if_fail (SNAP_IS_FILE_NOTES (self), FALSE);

    NoteEntry *entry = g_hash_table_lookup (self->entries, path);
    if (entry == NULL)
        return FALSE;

    for (guint i = 0; i < entry->tags->len; i++)
    {
        if (g_strcmp0 (g_ptr_array_index (entry->tags, i), tag) == 0)
            return TRUE;
    }

    return FALSE;
}
