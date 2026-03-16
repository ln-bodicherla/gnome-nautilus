/* snap-config.c
 *
 * Configuration read/write using json-glib.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-config.h"
#include <json-glib/json-glib.h>

struct _SnapConfig
{
    GObject      parent_instance;

    gchar       *config_path;
    gint         max_versions;
    gboolean     auto_save;
    gint         auto_save_interval;
    gboolean     compression;
    gchar       *store_path;
    gboolean     encryption;
    gchar       *default_branch;
    gint         gc_keep;
    gboolean     notifications;
    gboolean     auto_watch;
    gint         server_port;
    gchar      **ignore_patterns;
};

G_DEFINE_FINAL_TYPE (SnapConfig, snap_config, G_TYPE_OBJECT)

static SnapConfig *default_config = NULL;

static void
snap_config_finalize (GObject *object)
{
    SnapConfig *self = SNAP_CONFIG (object);

    g_free (self->config_path);
    g_free (self->store_path);
    g_free (self->default_branch);
    g_strfreev (self->ignore_patterns);

    G_OBJECT_CLASS (snap_config_parent_class)->finalize (object);
}

static void
snap_config_class_init (SnapConfigClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = snap_config_finalize;
}

static void
snap_config_init (SnapConfig *self)
{
    const gchar *home = g_get_home_dir ();
    self->config_path = g_build_filename (home, ".snapstore", "config.json", NULL);
    self->store_path = g_build_filename (home, ".snapstore", NULL);
    self->max_versions = 100;
    self->auto_save = FALSE;
    self->auto_save_interval = 30;
    self->compression = TRUE;
    self->encryption = FALSE;
    self->default_branch = g_strdup ("main");
    self->gc_keep = 10;
    self->notifications = TRUE;
    self->auto_watch = FALSE;
    self->server_port = 7483;
    self->ignore_patterns = g_new0 (gchar *, 1);
}

SnapConfig *
snap_config_get_default (void)
{
    if (default_config == NULL)
    {
        default_config = g_object_new (SNAP_TYPE_CONFIG, NULL);
        snap_config_load (default_config, NULL);
    }
    return default_config;
}

gboolean
snap_config_load (SnapConfig *self, GError **error)
{
    g_return_val_if_fail (SNAP_IS_CONFIG (self), FALSE);

    if (!g_file_test (self->config_path, G_FILE_TEST_EXISTS))
        return TRUE;  /* Use defaults */

    g_autoptr (JsonParser) parser = json_parser_new ();
    if (!json_parser_load_from_file (parser, self->config_path, error))
        return FALSE;

    JsonNode *root = json_parser_get_root (parser);
    if (!JSON_NODE_HOLDS_OBJECT (root))
        return TRUE;

    JsonObject *obj = json_node_get_object (root);

    if (json_object_has_member (obj, "maxVersions"))
        self->max_versions = json_object_get_int_member (obj, "maxVersions");
    if (json_object_has_member (obj, "autoSave"))
        self->auto_save = json_object_get_boolean_member (obj, "autoSave");
    if (json_object_has_member (obj, "autoSaveInterval"))
        self->auto_save_interval = json_object_get_int_member (obj, "autoSaveInterval");
    if (json_object_has_member (obj, "compression"))
        self->compression = json_object_get_boolean_member (obj, "compression");
    if (json_object_has_member (obj, "encryption"))
        self->encryption = json_object_get_boolean_member (obj, "encryption");
    if (json_object_has_member (obj, "defaultBranch"))
    {
        g_free (self->default_branch);
        self->default_branch = g_strdup (json_object_get_string_member (obj, "defaultBranch"));
    }
    if (json_object_has_member (obj, "gcKeep"))
        self->gc_keep = json_object_get_int_member (obj, "gcKeep");
    if (json_object_has_member (obj, "notifications"))
        self->notifications = json_object_get_boolean_member (obj, "notifications");
    if (json_object_has_member (obj, "autoWatch"))
        self->auto_watch = json_object_get_boolean_member (obj, "autoWatch");
    if (json_object_has_member (obj, "serverPort"))
        self->server_port = json_object_get_int_member (obj, "serverPort");
    if (json_object_has_member (obj, "storePath"))
    {
        g_free (self->store_path);
        self->store_path = g_strdup (json_object_get_string_member (obj, "storePath"));
    }
    if (json_object_has_member (obj, "ignorePatterns"))
    {
        g_strfreev (self->ignore_patterns);
        JsonArray *arr = json_object_get_array_member (obj, "ignorePatterns");
        guint len = json_array_get_length (arr);
        self->ignore_patterns = g_new0 (gchar *, len + 1);
        for (guint i = 0; i < len; i++)
            self->ignore_patterns[i] = g_strdup (json_array_get_string_element (arr, i));
    }

    return TRUE;
}

gboolean
snap_config_save (SnapConfig *self, GError **error)
{
    g_return_val_if_fail (SNAP_IS_CONFIG (self), FALSE);

    g_autoptr (JsonBuilder) builder = json_builder_new ();
    json_builder_begin_object (builder);

    json_builder_set_member_name (builder, "maxVersions");
    json_builder_add_int_value (builder, self->max_versions);

    json_builder_set_member_name (builder, "autoSave");
    json_builder_add_boolean_value (builder, self->auto_save);

    json_builder_set_member_name (builder, "autoSaveInterval");
    json_builder_add_int_value (builder, self->auto_save_interval);

    json_builder_set_member_name (builder, "compression");
    json_builder_add_boolean_value (builder, self->compression);

    json_builder_set_member_name (builder, "encryption");
    json_builder_add_boolean_value (builder, self->encryption);

    json_builder_set_member_name (builder, "defaultBranch");
    json_builder_add_string_value (builder, self->default_branch);

    json_builder_set_member_name (builder, "gcKeep");
    json_builder_add_int_value (builder, self->gc_keep);

    json_builder_set_member_name (builder, "notifications");
    json_builder_add_boolean_value (builder, self->notifications);

    json_builder_set_member_name (builder, "autoWatch");
    json_builder_add_boolean_value (builder, self->auto_watch);

    json_builder_set_member_name (builder, "serverPort");
    json_builder_add_int_value (builder, self->server_port);

    json_builder_set_member_name (builder, "storePath");
    json_builder_add_string_value (builder, self->store_path);

    json_builder_set_member_name (builder, "ignorePatterns");
    json_builder_begin_array (builder);
    if (self->ignore_patterns)
    {
        for (gint i = 0; self->ignore_patterns[i] != NULL; i++)
            json_builder_add_string_value (builder, self->ignore_patterns[i]);
    }
    json_builder_end_array (builder);

    json_builder_end_object (builder);

    g_autoptr (JsonGenerator) gen = json_generator_new ();
    json_generator_set_pretty (gen, TRUE);
    json_generator_set_root (gen, json_builder_get_root (builder));

    /* Ensure directory exists */
    g_autofree gchar *dir = g_path_get_dirname (self->config_path);
    g_mkdir_with_parents (dir, 0755);

    return json_generator_to_file (gen, self->config_path, error);
}

/* Getters */
gint snap_config_get_max_versions (SnapConfig *self) { return self->max_versions; }
gboolean snap_config_get_auto_save (SnapConfig *self) { return self->auto_save; }
gint snap_config_get_auto_save_interval (SnapConfig *self) { return self->auto_save_interval; }
gboolean snap_config_get_compression (SnapConfig *self) { return self->compression; }
const gchar *snap_config_get_store_path (SnapConfig *self) { return self->store_path; }
gboolean snap_config_get_encryption (SnapConfig *self) { return self->encryption; }
const gchar *snap_config_get_default_branch (SnapConfig *self) { return self->default_branch; }
gint snap_config_get_gc_keep (SnapConfig *self) { return self->gc_keep; }
gboolean snap_config_get_notifications (SnapConfig *self) { return self->notifications; }
gboolean snap_config_get_auto_watch (SnapConfig *self) { return self->auto_watch; }
gint snap_config_get_server_port (SnapConfig *self) { return self->server_port; }
const gchar * const *snap_config_get_ignore_patterns (SnapConfig *self) { return (const gchar * const *) self->ignore_patterns; }

/* Setters */
void snap_config_set_max_versions (SnapConfig *self, gint value) { self->max_versions = value; }
void snap_config_set_auto_save (SnapConfig *self, gboolean value) { self->auto_save = value; }
void snap_config_set_auto_save_interval (SnapConfig *self, gint seconds) { self->auto_save_interval = seconds; }
void snap_config_set_compression (SnapConfig *self, gboolean value) { self->compression = value; }

void snap_config_set_store_path (SnapConfig *self, const gchar *path)
{
    g_free (self->store_path);
    self->store_path = g_strdup (path);
}

void snap_config_set_encryption (SnapConfig *self, gboolean value) { self->encryption = value; }

void snap_config_set_default_branch (SnapConfig *self, const gchar *branch)
{
    g_free (self->default_branch);
    self->default_branch = g_strdup (branch);
}

void snap_config_set_gc_keep (SnapConfig *self, gint value) { self->gc_keep = value; }
void snap_config_set_notifications (SnapConfig *self, gboolean value) { self->notifications = value; }
void snap_config_set_auto_watch (SnapConfig *self, gboolean value) { self->auto_watch = value; }
void snap_config_set_server_port (SnapConfig *self, gint port) { self->server_port = port; }

void snap_config_set_ignore_patterns (SnapConfig *self, const gchar * const *patterns)
{
    g_strfreev (self->ignore_patterns);
    self->ignore_patterns = g_strdupv ((gchar **) patterns);
}
