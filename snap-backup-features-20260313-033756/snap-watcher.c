/* snap-watcher.c
 *
 * File monitoring via GFileMonitor with debounce.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-watcher.h"
#include "snap-manager.h"

typedef struct
{
    gchar          *file_path;
    GFileMonitor   *monitor;
    guint           debounce_id;
    SnapWatcher    *watcher;
} WatchEntry;

struct _SnapWatcher
{
    GObject      parent_instance;
    GHashTable  *watches;    /* gchar* -> WatchEntry* */
    guint        debounce_ms;
};

G_DEFINE_FINAL_TYPE (SnapWatcher, snap_watcher, G_TYPE_OBJECT)

static void
watch_entry_free (WatchEntry *entry)
{
    if (entry == NULL)
        return;
    if (entry->debounce_id > 0)
        g_source_remove (entry->debounce_id);
    if (entry->monitor != NULL)
    {
        g_file_monitor_cancel (entry->monitor);
        g_object_unref (entry->monitor);
    }
    g_free (entry->file_path);
    g_free (entry);
}

static gboolean
on_debounce_timeout (gpointer user_data)
{
    WatchEntry *entry = (WatchEntry *) user_data;
    entry->debounce_id = 0;

    /* Auto-save the file */
    SnapManager *manager = snap_manager_get_default ();
    snap_manager_save (manager, entry->file_path, "Auto-saved", NULL);

    return G_SOURCE_REMOVE;
}

static void
on_file_changed (GFileMonitor      *monitor,
                 GFile             *file,
                 GFile             *other_file,
                 GFileMonitorEvent  event_type,
                 gpointer           user_data)
{
    WatchEntry *entry = (WatchEntry *) user_data;

    if (event_type != G_FILE_MONITOR_EVENT_CHANGED &&
        event_type != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
        return;

    /* Debounce: reset timer */
    if (entry->debounce_id > 0)
        g_source_remove (entry->debounce_id);

    entry->debounce_id = g_timeout_add (entry->watcher->debounce_ms,
                                         on_debounce_timeout, entry);
}

static void
snap_watcher_finalize (GObject *object)
{
    SnapWatcher *self = SNAP_WATCHER (object);
    g_hash_table_destroy (self->watches);
    G_OBJECT_CLASS (snap_watcher_parent_class)->finalize (object);
}

static void
snap_watcher_class_init (SnapWatcherClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = snap_watcher_finalize;
}

static void
snap_watcher_init (SnapWatcher *self)
{
    self->watches = g_hash_table_new_full (g_str_hash, g_str_equal,
                                            NULL, (GDestroyNotify) watch_entry_free);
    self->debounce_ms = 2000;
}

SnapWatcher *
snap_watcher_new (void)
{
    return g_object_new (SNAP_TYPE_WATCHER, NULL);
}

gboolean
snap_watcher_watch (SnapWatcher *self, const gchar *file_path, GError **error)
{
    g_return_val_if_fail (SNAP_IS_WATCHER (self), FALSE);

    if (g_hash_table_contains (self->watches, file_path))
        return TRUE;  /* Already watching */

    g_autoptr (GFile) file = g_file_new_for_path (file_path);
    GFileMonitor *monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE,
                                                  NULL, error);
    if (monitor == NULL)
        return FALSE;

    WatchEntry *entry = g_new0 (WatchEntry, 1);
    entry->file_path = g_strdup (file_path);
    entry->monitor = monitor;
    entry->debounce_id = 0;
    entry->watcher = self;

    g_signal_connect (monitor, "changed", G_CALLBACK (on_file_changed), entry);

    g_hash_table_insert (self->watches, entry->file_path, entry);
    return TRUE;
}

void
snap_watcher_unwatch (SnapWatcher *self, const gchar *file_path)
{
    g_return_if_fail (SNAP_IS_WATCHER (self));
    g_hash_table_remove (self->watches, file_path);
}

void
snap_watcher_unwatch_all (SnapWatcher *self)
{
    g_return_if_fail (SNAP_IS_WATCHER (self));
    g_hash_table_remove_all (self->watches);
}

gboolean
snap_watcher_is_watching (SnapWatcher *self, const gchar *file_path)
{
    g_return_val_if_fail (SNAP_IS_WATCHER (self), FALSE);
    return g_hash_table_contains (self->watches, file_path);
}

GList *
snap_watcher_get_watched (SnapWatcher *self)
{
    g_return_val_if_fail (SNAP_IS_WATCHER (self), NULL);
    return g_hash_table_get_keys (self->watches);
}

void
snap_watcher_set_debounce (SnapWatcher *self, guint ms)
{
    g_return_if_fail (SNAP_IS_WATCHER (self));
    self->debounce_ms = ms;
}
