/* snap-search-engine-snap.c
 *
 * Search provider that queries the snap version store.
 * Two modes:
 *   1. Name-only: finds tracked files whose names match, annotates with version count
 *   2. Content: searches inside current + previous version content for query text
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "nautilus-search"

#include <config.h>
#include "snap-search-engine-snap.h"

#include "snap-manager.h"
#include "snap-store.h"
#include "nautilus-query.h"
#include "nautilus-search-hit.h"
#include "nautilus-search-provider.h"

#include <string.h>
#include <gio/gio.h>

#define BATCH_SIZE 500
#define SNIPPET_CONTEXT 40

struct _NautilusSearchEngineSnap
{
    NautilusSearchProvider parent_instance;
    gint n_processed;
};

G_DEFINE_FINAL_TYPE (NautilusSearchEngineSnap,
                     nautilus_search_engine_snap,
                     NAUTILUS_TYPE_SEARCH_PROVIDER)

/* Build a short snippet around the first occurrence of needle in content. */
static gchar *
build_snippet (const gchar *content,
               const gchar *needle)
{
    if (content == NULL || needle == NULL)
        return NULL;

    g_autofree gchar *content_lower = g_utf8_strdown (content, -1);
    g_autofree gchar *needle_lower = g_utf8_strdown (needle, -1);

    const gchar *pos = strstr (content_lower, needle_lower);
    if (pos == NULL)
        return NULL;

    gsize offset = (gsize) (pos - content_lower);
    gsize content_len = strlen (content);
    gsize start = (offset > SNIPPET_CONTEXT) ? offset - SNIPPET_CONTEXT : 0;
    gsize end = offset + strlen (needle) + SNIPPET_CONTEXT;
    if (end > content_len)
        end = content_len;

    /* Walk back/forward to avoid splitting mid-character in UTF-8 */
    while (start > 0 && (content[start] & 0xC0) == 0x80)
        start--;
    while (end < content_len && (content[end] & 0xC0) == 0x80)
        end++;

    g_autofree gchar *extract = g_strndup (content + start, end - start);
    return g_strdup_printf ("%s%s%s",
                            start > 0 ? "..." : "",
                            extract,
                            end < content_len ? "..." : "");
}

static gpointer
search_thread_func (NautilusSearchEngineSnap *self)
{
    GCancellable *cancellable = nautilus_search_provider_get_cancellable (self);
    NautilusQuery *query = nautilus_search_provider_get_query (self);
    const gchar *text = nautilus_query_get_text (query);
    gboolean search_content = nautilus_query_get_search_content (query);

    if (text == NULL || *text == '\0')
    {
        g_idle_add_once ((GSourceOnceFunc) nautilus_search_provider_finished, self);
        return NULL;
    }

    SnapManager *manager = snap_manager_get_default ();
    SnapStore *store = snap_manager_get_store (manager);

    if (store == NULL)
    {
        g_idle_add_once ((GSourceOnceFunc) nautilus_search_provider_finished, self);
        return NULL;
    }

    /* Get the search location to scope results */
    g_autoptr (GFile) search_location = nautilus_query_get_location (query);
    g_autofree gchar *search_path = NULL;
    if (search_location != NULL)
        search_path = g_file_get_path (search_location);

    /* Get all tracked files from the snap store */
    GList *tracked_files = snap_store_get_tracked_files (store, NULL);

    self->n_processed = 0;

    for (GList *l = tracked_files; l != NULL; l = l->next)
    {
        if (nautilus_search_provider_should_stop (self))
            break;

        const gchar *file_path = (const gchar *) l->data;

        /* Scope: only include files under the search location */
        if (search_path != NULL && !g_str_has_prefix (file_path, search_path))
            continue;

        /* Check if the file still exists on disk */
        if (!g_file_test (file_path, G_FILE_TEST_EXISTS))
            continue;

        gboolean matched = FALSE;
        gdouble rank = 0.0;
        gchar *snippet = NULL;
        gint64 version_count = snap_store_get_snapshot_count (store, file_path);

        /* 1. Name matching — always check */
        g_autofree gchar *basename = g_path_get_basename (file_path);
        gdouble name_match = nautilus_query_matches_string (query, basename);
        if (name_match > -1)
        {
            matched = TRUE;
            rank = name_match;
        }

        /* 2. Content search — search current file + previous versions */
        if (search_content && !matched)
        {
            /* Search current file content */
            g_autofree gchar *current_content = NULL;
            gsize len = 0;
            GError *err = NULL;

            if (g_file_get_contents (file_path, &current_content, &len, &err))
            {
                /* Only search text files (check for NUL bytes in first 8KB) */
                gsize check_len = MIN (len, 8192);
                gboolean is_text = TRUE;
                for (gsize i = 0; i < check_len; i++)
                {
                    if (current_content[i] == '\0')
                    {
                        is_text = FALSE;
                        break;
                    }
                }

                if (is_text)
                {
                    snippet = build_snippet (current_content, text);
                    if (snippet != NULL)
                    {
                        matched = TRUE;
                        rank = 0.5;
                    }
                }
            }
            else
            {
                g_clear_error (&err);
            }

            /* If not found in current, search previous versions */
            if (!matched && version_count > 0)
            {
                GList *history = snap_store_get_history (store, file_path,
                                                         "main", 5, NULL);
                for (GList *h = history; h != NULL; h = h->next)
                {
                    if (nautilus_search_provider_should_stop (self))
                        break;

                    SnapSnapshot *snap = (SnapSnapshot *) h->data;
                    g_autofree gchar *content = snap_store_get_snapshot_content (
                        store, snap, NULL);
                    if (content == NULL)
                        continue;

                    snippet = build_snippet (content, text);
                    if (snippet != NULL)
                    {
                        matched = TRUE;
                        rank = 0.3; /* lower rank for old-version matches */

                        /* Annotate snippet with version info */
                        gchar *versioned_snippet = g_strdup_printf (
                            "[v%" G_GINT64_FORMAT "] %s",
                            snap->version, snippet);
                        g_free (snippet);
                        snippet = versioned_snippet;
                        break;
                    }
                }
                g_list_free_full (history, (GDestroyNotify) snap_snapshot_free);
            }
        }

        if (matched)
        {
            g_autofree gchar *uri = g_filename_to_uri (file_path, NULL, NULL);
            if (uri == NULL)
                continue;

            NautilusSearchHit *hit = nautilus_search_hit_new (uri);
            nautilus_search_hit_set_fts_rank (hit, rank);

            if (snippet != NULL)
            {
                /* Append version count to snippet */
                if (version_count > 0)
                {
                    gchar *full_snippet = g_strdup_printf (
                        "%s  [%" G_GINT64_FORMAT " version%s]",
                        snippet,
                        version_count,
                        version_count == 1 ? "" : "s");
                    nautilus_search_hit_set_fts_snippet (hit, full_snippet);
                    g_free (full_snippet);
                }
                else
                {
                    nautilus_search_hit_set_fts_snippet (hit, snippet);
                }
                g_free (snippet);
                snippet = NULL;
            }
            else if (version_count > 0)
            {
                g_autofree gchar *version_info = g_strdup_printf (
                    "%" G_GINT64_FORMAT " version%s",
                    version_count,
                    version_count == 1 ? "" : "s");
                nautilus_search_hit_set_fts_snippet (hit, version_info);
            }

            /* Set file times from stat */
            g_autoptr (GFile) file = g_file_new_for_path (file_path);
            g_autoptr (GFileInfo) info = g_file_query_info (
                file,
                G_FILE_ATTRIBUTE_TIME_MODIFIED ","
                G_FILE_ATTRIBUTE_TIME_ACCESS ","
                G_FILE_ATTRIBUTE_TIME_CREATED,
                G_FILE_QUERY_INFO_NONE, cancellable, NULL);

            if (info != NULL)
            {
                g_autoptr (GDateTime) mtime = g_file_info_get_modification_date_time (info);
                g_autoptr (GDateTime) atime = g_file_info_get_access_date_time (info);
                g_autoptr (GDateTime) ctime = g_file_info_get_creation_date_time (info);
                nautilus_search_hit_set_modification_time (hit, mtime);
                nautilus_search_hit_set_access_time (hit, atime);
                nautilus_search_hit_set_creation_time (hit, ctime);
            }

            nautilus_search_provider_add_hit (self, hit);

            self->n_processed++;
            if (self->n_processed > BATCH_SIZE)
            {
                self->n_processed = 0;
                nautilus_search_provider_flush_hits (self);
            }
        }
    }

    g_list_free_full (tracked_files, g_free);

    g_idle_add_once ((GSourceOnceFunc) nautilus_search_provider_finished, self);
    return NULL;
}

static void
start_search (NautilusSearchProvider *provider)
{
    g_autoptr (GThread) thread = g_thread_new (
        "nautilus-search-snap",
        (GThreadFunc) search_thread_func,
        provider);
}

static const char *
get_name (NautilusSearchProvider *provider)
{
    return "snap";
}

static gboolean
run_in_thread (NautilusSearchProvider *provider)
{
    return TRUE;
}

static guint
search_delay (NautilusSearchProvider *provider)
{
    return 500;
}

static gboolean
should_search (NautilusSearchProvider *provider,
               NautilusQuery          *query)
{
    /* Always search snap store when we have a location */
    g_autoptr (GFile) location = nautilus_query_get_location (query);
    return location != NULL;
}

static void
nautilus_search_engine_snap_class_init (NautilusSearchEngineSnapClass *klass)
{
    NautilusSearchProviderClass *provider_class = NAUTILUS_SEARCH_PROVIDER_CLASS (klass);

    provider_class->get_name = get_name;
    provider_class->run_in_thread = run_in_thread;
    provider_class->search_delay = search_delay;
    provider_class->should_search = should_search;
    provider_class->start_search = start_search;
}

static void
nautilus_search_engine_snap_init (NautilusSearchEngineSnap *self)
{
}

NautilusSearchEngineSnap *
nautilus_search_engine_snap_new (void)
{
    return g_object_new (NAUTILUS_TYPE_SEARCH_ENGINE_SNAP, NULL);
}
