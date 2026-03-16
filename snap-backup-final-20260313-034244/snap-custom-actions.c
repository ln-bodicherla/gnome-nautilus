/* snap-custom-actions.c
 *
 * User-defined custom actions for context menu.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-custom-actions.h"
#include <json-glib/json-glib.h>
#include <gio/gio.h>

struct _SnapCustomActions
{
    GObject parent_instance;
    GList *actions; /* SnapCustomAction* */
    gchar *file_path;
};

G_DEFINE_FINAL_TYPE (SnapCustomActions, snap_custom_actions, G_TYPE_OBJECT)

static SnapCustomActions *default_instance = NULL;

void
snap_custom_action_free (SnapCustomAction *action)
{
    if (action == NULL)
        return;
    g_free (action->name);
    g_free (action->command);
    g_free (action->icon);
    g_free (action->extensions);
    g_free (action);
}

static SnapCustomAction *
snap_custom_action_copy (const SnapCustomAction *action)
{
    SnapCustomAction *copy = g_new0 (SnapCustomAction, 1);
    copy->name = g_strdup (action->name);
    copy->command = g_strdup (action->command);
    copy->icon = g_strdup (action->icon);
    copy->extensions = g_strdup (action->extensions);
    copy->directories_only = action->directories_only;
    return copy;
}

static gchar *
get_storage_path (void)
{
    return g_build_filename (g_get_home_dir (), ".snapstore", "custom_actions.json", NULL);
}

static void
snap_custom_actions_save (SnapCustomActions *self)
{
    JsonBuilder *builder = json_builder_new ();
    json_builder_begin_array (builder);

    for (GList *l = self->actions; l != NULL; l = l->next)
    {
        SnapCustomAction *action = l->data;
        json_builder_begin_object (builder);

        json_builder_set_member_name (builder, "name");
        json_builder_add_string_value (builder, action->name);

        json_builder_set_member_name (builder, "command");
        json_builder_add_string_value (builder, action->command);

        if (action->icon != NULL)
        {
            json_builder_set_member_name (builder, "icon");
            json_builder_add_string_value (builder, action->icon);
        }

        if (action->extensions != NULL)
        {
            json_builder_set_member_name (builder, "extensions");
            json_builder_add_string_value (builder, action->extensions);
        }

        if (action->directories_only)
        {
            json_builder_set_member_name (builder, "directories_only");
            json_builder_add_boolean_value (builder, TRUE);
        }

        json_builder_end_object (builder);
    }

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
snap_custom_actions_load (SnapCustomActions *self)
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

    for (guint i = 0; i < len; i++)
    {
        JsonObject *obj = json_array_get_object_element (arr, i);
        if (obj == NULL)
            continue;

        SnapCustomAction *action = g_new0 (SnapCustomAction, 1);

        if (json_object_has_member (obj, "name"))
            action->name = g_strdup (json_object_get_string_member (obj, "name"));
        if (json_object_has_member (obj, "command"))
            action->command = g_strdup (json_object_get_string_member (obj, "command"));
        if (json_object_has_member (obj, "icon"))
            action->icon = g_strdup (json_object_get_string_member (obj, "icon"));
        if (json_object_has_member (obj, "extensions"))
            action->extensions = g_strdup (json_object_get_string_member (obj, "extensions"));
        if (json_object_has_member (obj, "directories_only"))
            action->directories_only = json_object_get_boolean_member (obj, "directories_only");

        self->actions = g_list_append (self->actions, action);
    }

    g_object_unref (parser);
}

static void
snap_custom_actions_finalize (GObject *object)
{
    SnapCustomActions *self = SNAP_CUSTOM_ACTIONS (object);
    g_list_free_full (self->actions, (GDestroyNotify) snap_custom_action_free);
    g_free (self->file_path);
    G_OBJECT_CLASS (snap_custom_actions_parent_class)->finalize (object);
}

static void
snap_custom_actions_class_init (SnapCustomActionsClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = snap_custom_actions_finalize;
}

static void
snap_custom_actions_init (SnapCustomActions *self)
{
    self->actions = NULL;
    self->file_path = get_storage_path ();
    snap_custom_actions_load (self);

    /* Add default actions if none exist */
    if (self->actions == NULL)
    {
        SnapCustomAction open_terminal = {
            .name = g_strdup ("Open Terminal Here"),
            .command = g_strdup ("xterm -e 'cd \"%d\" && exec $SHELL'"),
            .icon = g_strdup ("utilities-terminal-symbolic"),
            .extensions = NULL,
            .directories_only = TRUE,
        };
        snap_custom_actions_add (self, &open_terminal);
        g_free (open_terminal.name);
        g_free (open_terminal.command);
        g_free (open_terminal.icon);

        SnapCustomAction open_editor = {
            .name = g_strdup ("Edit with Text Editor"),
            .command = g_strdup ("xdg-open \"%f\""),
            .icon = g_strdup ("text-editor-symbolic"),
            .extensions = g_strdup (".txt,.md,.json,.xml,.yaml,.yml,.toml,.ini,.cfg,.conf"),
            .directories_only = FALSE,
        };
        snap_custom_actions_add (self, &open_editor);
        g_free (open_editor.name);
        g_free (open_editor.command);
        g_free (open_editor.icon);
        g_free (open_editor.extensions);
    }
}

SnapCustomActions *
snap_custom_actions_get_default (void)
{
    if (default_instance == NULL)
        default_instance = g_object_new (SNAP_TYPE_CUSTOM_ACTIONS, NULL);
    return default_instance;
}

GList *
snap_custom_actions_get_all (SnapCustomActions *self)
{
    g_return_val_if_fail (SNAP_IS_CUSTOM_ACTIONS (self), NULL);

    GList *result = NULL;
    for (GList *l = self->actions; l != NULL; l = l->next)
        result = g_list_prepend (result, snap_custom_action_copy (l->data));
    return g_list_reverse (result);
}

void
snap_custom_actions_add (SnapCustomActions       *self,
                          const SnapCustomAction  *action)
{
    g_return_if_fail (SNAP_IS_CUSTOM_ACTIONS (self));
    self->actions = g_list_append (self->actions, snap_custom_action_copy (action));
    snap_custom_actions_save (self);
}

void
snap_custom_actions_remove (SnapCustomActions *self,
                             const gchar       *name)
{
    g_return_if_fail (SNAP_IS_CUSTOM_ACTIONS (self));

    for (GList *l = self->actions; l != NULL; l = l->next)
    {
        SnapCustomAction *action = l->data;
        if (g_strcmp0 (action->name, name) == 0)
        {
            snap_custom_action_free (action);
            self->actions = g_list_delete_link (self->actions, l);
            snap_custom_actions_save (self);
            return;
        }
    }
}

static gchar *
expand_command (const gchar *command,
                const gchar *file_path,
                const gchar *dir_path,
                GList       *files)
{
    g_autoptr(GString) result = g_string_new ("");
    const gchar *p = command;

    while (*p != '\0')
    {
        if (p[0] == '%' && p[1] != '\0')
        {
            switch (p[1])
            {
                case 'f':
                    if (file_path != NULL)
                        g_string_append (result, file_path);
                    p += 2;
                    continue;
                case 'd':
                    if (dir_path != NULL)
                        g_string_append (result, dir_path);
                    p += 2;
                    continue;
                case 'F':
                    for (GList *l = files; l != NULL; l = l->next)
                    {
                        if (l != files)
                            g_string_append_c (result, ' ');
                        g_string_append_printf (result, "\"%s\"", (const gchar *) l->data);
                    }
                    p += 2;
                    continue;
                default:
                    break;
            }
        }
        g_string_append_c (result, *p);
        p++;
    }

    return g_string_free (g_steal_pointer (&result), FALSE);
}

void
snap_custom_actions_execute (SnapCustomActions *self,
                              const gchar       *name,
                              GList             *files)
{
    g_return_if_fail (SNAP_IS_CUSTOM_ACTIONS (self));

    for (GList *l = self->actions; l != NULL; l = l->next)
    {
        SnapCustomAction *action = l->data;
        if (g_strcmp0 (action->name, name) != 0)
            continue;

        const gchar *first_file = files ? files->data : NULL;
        g_autofree gchar *dir = first_file ? g_path_get_dirname (first_file) : NULL;

        g_autofree gchar *expanded = expand_command (action->command, first_file, dir, files);

        /* Run asynchronously */
        g_spawn_command_line_async (expanded, NULL);
        return;
    }
}
