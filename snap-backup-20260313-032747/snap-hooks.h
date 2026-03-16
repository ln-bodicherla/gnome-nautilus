/* snap-hooks.h
 *
 * Hook scripts at ~/.snapstore/hooks/. Uses g_spawn_sync().
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
    SNAP_HOOK_EVENT_PRE_SAVE,
    SNAP_HOOK_EVENT_POST_SAVE,
    SNAP_HOOK_EVENT_PRE_RESTORE,
    SNAP_HOOK_EVENT_POST_RESTORE,
    SNAP_HOOK_EVENT_PRE_DELETE,
    SNAP_HOOK_EVENT_POST_DELETE,
    SNAP_HOOK_EVENT_PRE_BRANCH,
    SNAP_HOOK_EVENT_POST_BRANCH,
    SNAP_HOOK_EVENT_PRE_MERGE,
    SNAP_HOOK_EVENT_POST_MERGE,
} SnapHookEvent;

const gchar   *snap_hook_event_to_string     (SnapHookEvent event);
SnapHookEvent  snap_hook_event_from_string   (const gchar  *str);

/* Run all hooks for an event */
gboolean       snap_hooks_run                (SnapHookEvent event,
                                              const gchar  *file_path,
                                              const gchar  *extra_info,
                                              gchar       **stdout_output,
                                              GError      **error);

/* Install a hook script */
gboolean       snap_hooks_install            (const gchar  *name,
                                              SnapHookEvent event,
                                              const gchar  *script_path,
                                              GError      **error);

/* Remove a hook */
gboolean       snap_hooks_remove             (const gchar  *name,
                                              GError      **error);

/* List installed hooks */
GList         *snap_hooks_list               (GError      **error);

/* Get hooks directory path */
const gchar   *snap_hooks_get_dir            (void);

G_END_DECLS
