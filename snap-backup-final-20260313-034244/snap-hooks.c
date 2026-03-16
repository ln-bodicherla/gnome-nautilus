/* snap-hooks.c
 *
 * Hook scripts management.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-hooks.h"
#include "snap-store.h"
#include <gio/gio.h>

static gchar *hooks_dir = NULL;

const gchar *
snap_hooks_get_dir (void)
{
    if (hooks_dir == NULL)
        hooks_dir = g_build_filename (g_get_home_dir (), ".snapstore", "hooks", NULL);
    return hooks_dir;
}

const gchar *
snap_hook_event_to_string (SnapHookEvent event)
{
    switch (event)
    {
        case SNAP_HOOK_EVENT_PRE_SAVE:     return "pre-save";
        case SNAP_HOOK_EVENT_POST_SAVE:    return "post-save";
        case SNAP_HOOK_EVENT_PRE_RESTORE:  return "pre-restore";
        case SNAP_HOOK_EVENT_POST_RESTORE: return "post-restore";
        case SNAP_HOOK_EVENT_PRE_DELETE:   return "pre-delete";
        case SNAP_HOOK_EVENT_POST_DELETE:  return "post-delete";
        case SNAP_HOOK_EVENT_PRE_BRANCH:   return "pre-branch";
        case SNAP_HOOK_EVENT_POST_BRANCH:  return "post-branch";
        case SNAP_HOOK_EVENT_PRE_MERGE:    return "pre-merge";
        case SNAP_HOOK_EVENT_POST_MERGE:   return "post-merge";
        default: return "unknown";
    }
}

SnapHookEvent
snap_hook_event_from_string (const gchar *str)
{
    if (g_strcmp0 (str, "pre-save") == 0)     return SNAP_HOOK_EVENT_PRE_SAVE;
    if (g_strcmp0 (str, "post-save") == 0)    return SNAP_HOOK_EVENT_POST_SAVE;
    if (g_strcmp0 (str, "pre-restore") == 0)  return SNAP_HOOK_EVENT_PRE_RESTORE;
    if (g_strcmp0 (str, "post-restore") == 0) return SNAP_HOOK_EVENT_POST_RESTORE;
    if (g_strcmp0 (str, "pre-delete") == 0)   return SNAP_HOOK_EVENT_PRE_DELETE;
    if (g_strcmp0 (str, "post-delete") == 0)  return SNAP_HOOK_EVENT_POST_DELETE;
    if (g_strcmp0 (str, "pre-branch") == 0)   return SNAP_HOOK_EVENT_PRE_BRANCH;
    if (g_strcmp0 (str, "post-branch") == 0)  return SNAP_HOOK_EVENT_POST_BRANCH;
    if (g_strcmp0 (str, "pre-merge") == 0)    return SNAP_HOOK_EVENT_PRE_MERGE;
    if (g_strcmp0 (str, "post-merge") == 0)   return SNAP_HOOK_EVENT_POST_MERGE;
    return SNAP_HOOK_EVENT_PRE_SAVE;
}

gboolean
snap_hooks_run (SnapHookEvent  event,
                const gchar   *file_path,
                const gchar   *extra_info,
                gchar        **stdout_output,
                GError       **error)
{
    SnapStore *store = snap_store_get_default ();
    const gchar *event_str = snap_hook_event_to_string (event);

    GList *hooks = snap_store_list_hooks (store, NULL);
    gboolean all_ok = TRUE;

    for (GList *l = hooks; l != NULL; l = l->next)
    {
        SnapHook *hook = (SnapHook *) l->data;

        if (!hook->enabled || g_strcmp0 (hook->event, event_str) != 0)
            continue;

        if (!g_file_test (hook->script_path, G_FILE_TEST_IS_EXECUTABLE))
            continue;

        gchar *argv[] = {
            (gchar *) hook->script_path,
            (gchar *) event_str,
            (gchar *) (file_path ? file_path : ""),
            (gchar *) (extra_info ? extra_info : ""),
            NULL
        };

        gchar *std_out = NULL;
        gchar *std_err = NULL;
        gint exit_status;

        gboolean ok = g_spawn_sync (NULL, argv, NULL,
                                    G_SPAWN_DEFAULT,
                                    NULL, NULL,
                                    &std_out, &std_err,
                                    &exit_status, error);

        if (ok && stdout_output != NULL && std_out != NULL)
        {
            if (*stdout_output == NULL)
                *stdout_output = g_strdup (std_out);
            else
            {
                gchar *combined = g_strconcat (*stdout_output, "\n", std_out, NULL);
                g_free (*stdout_output);
                *stdout_output = combined;
            }
        }

        g_free (std_out);
        g_free (std_err);

        if (!ok || exit_status != 0)
        {
            all_ok = FALSE;
            break;
        }
    }

    g_list_free_full (hooks, (GDestroyNotify) snap_hook_free);
    return all_ok;
}

gboolean
snap_hooks_install (const gchar   *name,
                    SnapHookEvent  event,
                    const gchar   *script_path,
                    GError       **error)
{
    /* Ensure hooks directory exists */
    g_mkdir_with_parents (snap_hooks_get_dir (), 0755);

    /* Copy script to hooks directory */
    g_autofree gchar *dest = g_build_filename (snap_hooks_get_dir (), name, NULL);

    g_autoptr (GFile) src = g_file_new_for_path (script_path);
    g_autoptr (GFile) dst = g_file_new_for_path (dest);

    if (!g_file_copy (src, dst, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, error))
        return FALSE;

    /* Make executable */
    GFileInfo *info = g_file_query_info (dst, G_FILE_ATTRIBUTE_UNIX_MODE,
                                         G_FILE_QUERY_INFO_NONE, NULL, NULL);
    if (info != NULL)
    {
        guint32 mode = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE);
        g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE, mode | 0111);
        g_file_set_attributes_from_info (dst, info, G_FILE_QUERY_INFO_NONE, NULL, NULL);
        g_object_unref (info);
    }

    /* Register in database */
    SnapStore *store = snap_store_get_default ();
    return snap_store_add_hook (store, name, snap_hook_event_to_string (event), dest, error);
}

gboolean
snap_hooks_remove (const gchar *name, GError **error)
{
    /* Remove from database */
    SnapStore *store = snap_store_get_default ();
    snap_store_remove_hook (store, name, NULL);

    /* Remove script file */
    g_autofree gchar *path = g_build_filename (snap_hooks_get_dir (), name, NULL);
    g_autoptr (GFile) file = g_file_new_for_path (path);
    g_file_delete (file, NULL, NULL);

    return TRUE;
}

GList *
snap_hooks_list (GError **error)
{
    SnapStore *store = snap_store_get_default ();
    return snap_store_list_hooks (store, error);
}
