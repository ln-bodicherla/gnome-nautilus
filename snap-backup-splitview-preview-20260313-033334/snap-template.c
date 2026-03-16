/* snap-template.c
 *
 * Per-directory .snapconfig templates.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-template.h"
#include <json-glib/json-glib.h>

void
snap_template_free (SnapTemplate *tmpl)
{
    if (tmpl == NULL)
        return;
    g_free (tmpl->directory);
    g_free (tmpl->default_branch);
    g_strfreev (tmpl->ignore_patterns);
    g_free (tmpl);
}

SnapTemplate *
snap_template_new_default (const gchar *directory)
{
    SnapTemplate *tmpl = g_new0 (SnapTemplate, 1);
    tmpl->directory = g_strdup (directory);
    tmpl->max_versions = 100;
    tmpl->auto_save = FALSE;
    tmpl->auto_save_interval = 30;
    tmpl->compression = TRUE;
    tmpl->encryption = FALSE;
    tmpl->default_branch = g_strdup ("main");
    tmpl->ignore_patterns = g_new0 (gchar *, 1);
    return tmpl;
}

SnapTemplate *
snap_template_load (const gchar *directory, GError **error)
{
    g_autofree gchar *config_path = g_build_filename (directory, ".snapconfig", NULL);

    if (!g_file_test (config_path, G_FILE_TEST_EXISTS))
        return NULL;

    g_autoptr (JsonParser) parser = json_parser_new ();
    if (!json_parser_load_from_file (parser, config_path, error))
        return NULL;

    JsonNode *root = json_parser_get_root (parser);
    if (!JSON_NODE_HOLDS_OBJECT (root))
        return snap_template_new_default (directory);

    JsonObject *obj = json_node_get_object (root);
    SnapTemplate *tmpl = snap_template_new_default (directory);

    if (json_object_has_member (obj, "maxVersions"))
        tmpl->max_versions = json_object_get_int_member (obj, "maxVersions");
    if (json_object_has_member (obj, "autoSave"))
        tmpl->auto_save = json_object_get_boolean_member (obj, "autoSave");
    if (json_object_has_member (obj, "autoSaveInterval"))
        tmpl->auto_save_interval = json_object_get_int_member (obj, "autoSaveInterval");
    if (json_object_has_member (obj, "compression"))
        tmpl->compression = json_object_get_boolean_member (obj, "compression");
    if (json_object_has_member (obj, "encryption"))
        tmpl->encryption = json_object_get_boolean_member (obj, "encryption");
    if (json_object_has_member (obj, "defaultBranch"))
    {
        g_free (tmpl->default_branch);
        tmpl->default_branch = g_strdup (json_object_get_string_member (obj, "defaultBranch"));
    }
    if (json_object_has_member (obj, "ignorePatterns"))
    {
        g_strfreev (tmpl->ignore_patterns);
        JsonArray *arr = json_object_get_array_member (obj, "ignorePatterns");
        guint len = json_array_get_length (arr);
        tmpl->ignore_patterns = g_new0 (gchar *, len + 1);
        for (guint i = 0; i < len; i++)
            tmpl->ignore_patterns[i] = g_strdup (json_array_get_string_element (arr, i));
    }

    return tmpl;
}

gboolean
snap_template_save (SnapTemplate *tmpl, GError **error)
{
    g_return_val_if_fail (tmpl != NULL, FALSE);

    g_autoptr (JsonBuilder) builder = json_builder_new ();
    json_builder_begin_object (builder);

    json_builder_set_member_name (builder, "maxVersions");
    json_builder_add_int_value (builder, tmpl->max_versions);
    json_builder_set_member_name (builder, "autoSave");
    json_builder_add_boolean_value (builder, tmpl->auto_save);
    json_builder_set_member_name (builder, "autoSaveInterval");
    json_builder_add_int_value (builder, tmpl->auto_save_interval);
    json_builder_set_member_name (builder, "compression");
    json_builder_add_boolean_value (builder, tmpl->compression);
    json_builder_set_member_name (builder, "encryption");
    json_builder_add_boolean_value (builder, tmpl->encryption);
    json_builder_set_member_name (builder, "defaultBranch");
    json_builder_add_string_value (builder, tmpl->default_branch);

    json_builder_set_member_name (builder, "ignorePatterns");
    json_builder_begin_array (builder);
    if (tmpl->ignore_patterns)
        for (gint i = 0; tmpl->ignore_patterns[i] != NULL; i++)
            json_builder_add_string_value (builder, tmpl->ignore_patterns[i]);
    json_builder_end_array (builder);

    json_builder_end_object (builder);

    g_autoptr (JsonGenerator) gen = json_generator_new ();
    json_generator_set_pretty (gen, TRUE);
    json_generator_set_root (gen, json_builder_get_root (builder));

    g_autofree gchar *path = g_build_filename (tmpl->directory, ".snapconfig", NULL);
    return json_generator_to_file (gen, path, error);
}

SnapTemplate *
snap_template_find_for_file (const gchar *file_path)
{
    g_autofree gchar *dir = g_path_get_dirname (file_path);

    /* Walk parent directories looking for .snapconfig */
    while (dir != NULL && g_strcmp0 (dir, "/") != 0 && g_strcmp0 (dir, ".") != 0)
    {
        SnapTemplate *tmpl = snap_template_load (dir, NULL);
        if (tmpl != NULL)
            return tmpl;

        gchar *parent = g_path_get_dirname (dir);
        g_free (dir);
        dir = parent;
    }

    return NULL;
}
