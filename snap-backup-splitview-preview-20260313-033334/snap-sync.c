/* snap-sync.c
 *
 * Sync/backup to external paths.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-sync.h"
#include "snap-config.h"

struct _SnapSync
{
    GObject    parent_instance;
    gdouble    progress;
};

G_DEFINE_FINAL_TYPE (SnapSync, snap_sync, G_TYPE_OBJECT)

static void
snap_sync_class_init (SnapSyncClass *klass)
{
}

static void
snap_sync_init (SnapSync *self)
{
    self->progress = 0.0;
}

SnapSync *
snap_sync_new (void)
{
    return g_object_new (SNAP_TYPE_SYNC, NULL);
}

static gboolean
copy_directory_recursive (GFile *src, GFile *dst, SnapSync *self,
                          GCancellable *cancellable, GError **error)
{
    g_autoptr (GFileEnumerator) enumerator =
        g_file_enumerate_children (src,
                                   G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                   G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                   G_FILE_QUERY_INFO_NONE,
                                   cancellable, error);
    if (enumerator == NULL)
        return FALSE;

    g_file_make_directory_with_parents (dst, cancellable, NULL);

    GFileInfo *info;
    while ((info = g_file_enumerator_next_file (enumerator, cancellable, error)) != NULL)
    {
        const gchar *name = g_file_info_get_name (info);
        g_autoptr (GFile) src_child = g_file_get_child (src, name);
        g_autoptr (GFile) dst_child = g_file_get_child (dst, name);

        if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
        {
            if (!copy_directory_recursive (src_child, dst_child, self,
                                           cancellable, error))
            {
                g_object_unref (info);
                return FALSE;
            }
        }
        else
        {
            if (!g_file_copy (src_child, dst_child, G_FILE_COPY_OVERWRITE,
                              cancellable, NULL, NULL, error))
            {
                g_object_unref (info);
                return FALSE;
            }
        }

        g_object_unref (info);
    }

    return TRUE;
}

static void
sync_thread_func (GTask        *task,
                  gpointer      source_object,
                  gpointer      task_data,
                  GCancellable *cancellable)
{
    SnapSync *self = SNAP_SYNC (source_object);
    const gchar *target_path = (const gchar *) task_data;
    GError *error = NULL;

    SnapConfig *config = snap_config_get_default ();
    const gchar *store_path = snap_config_get_store_path (config);

    g_autoptr (GFile) src = g_file_new_for_path (store_path);
    g_autoptr (GFile) dst = g_file_new_for_path (target_path);

    self->progress = 0.1;

    if (copy_directory_recursive (src, dst, self, cancellable, &error))
    {
        self->progress = 1.0;
        g_task_return_boolean (task, TRUE);
    }
    else
    {
        g_task_return_error (task, error);
    }
}

void
snap_sync_to_path_async (SnapSync           *self,
                         const gchar        *target_path,
                         GCancellable       *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer            user_data)
{
    GTask *task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_task_data (task, g_strdup (target_path), g_free);
    g_task_run_in_thread (task, sync_thread_func);
    g_object_unref (task);
}

gboolean
snap_sync_to_path_finish (SnapSync *self, GAsyncResult *result, GError **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
backup_thread_func (GTask        *task,
                    gpointer      source_object,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
    SnapSync *self = SNAP_SYNC (source_object);
    const gchar *output_path = (const gchar *) task_data;
    GError *error = NULL;

    SnapConfig *config = snap_config_get_default ();
    const gchar *store_path = snap_config_get_store_path (config);

    /* Create tar.gz backup using command line */
    g_autofree gchar *cmd = g_strdup_printf (
        "tar -czf '%s' -C '%s' .", output_path, store_path);

    self->progress = 0.1;

    gint exit_status;
    if (g_spawn_command_line_sync (cmd, NULL, NULL, &exit_status, &error))
    {
        if (exit_status == 0)
        {
            self->progress = 1.0;
            g_task_return_boolean (task, TRUE);
        }
        else
        {
            g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                                    "Backup command failed with exit code %d", exit_status);
        }
    }
    else
    {
        g_task_return_error (task, error);
    }
}

void
snap_sync_backup_async (SnapSync           *self,
                        const gchar        *output_path,
                        GCancellable       *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer            user_data)
{
    GTask *task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_task_data (task, g_strdup (output_path), g_free);
    g_task_run_in_thread (task, backup_thread_func);
    g_object_unref (task);
}

gboolean
snap_sync_backup_finish (SnapSync *self, GAsyncResult *result, GError **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
import_thread_func (GTask        *task,
                    gpointer      source_object,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
    SnapSync *self = SNAP_SYNC (source_object);
    const gchar *archive_path = (const gchar *) task_data;
    GError *error = NULL;

    SnapConfig *config = snap_config_get_default ();
    const gchar *store_path = snap_config_get_store_path (config);

    g_autofree gchar *cmd = g_strdup_printf (
        "tar -xzf '%s' -C '%s'", archive_path, store_path);

    self->progress = 0.1;

    gint exit_status;
    if (g_spawn_command_line_sync (cmd, NULL, NULL, &exit_status, &error))
    {
        if (exit_status == 0)
        {
            self->progress = 1.0;
            g_task_return_boolean (task, TRUE);
        }
        else
        {
            g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                                    "Import failed with exit code %d", exit_status);
        }
    }
    else
    {
        g_task_return_error (task, error);
    }
}

void
snap_sync_import_async (SnapSync           *self,
                        const gchar        *archive_path,
                        GCancellable       *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer            user_data)
{
    GTask *task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_task_data (task, g_strdup (archive_path), g_free);
    g_task_run_in_thread (task, import_thread_func);
    g_object_unref (task);
}

gboolean
snap_sync_import_finish (SnapSync *self, GAsyncResult *result, GError **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

gdouble
snap_sync_get_progress (SnapSync *self)
{
    return self->progress;
}
