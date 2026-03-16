/* snap-custom-actions.h
 *
 * User-defined custom actions for context menu.
 * Actions are defined in ~/.snapstore/custom_actions.json
 *
 * Each action has:
 * - name: display name
 * - command: shell command (%f = file, %d = directory, %F = files)
 * - icon: optional icon name
 * - extensions: optional file extension filter (e.g. ".jpg,.png")
 * - directories_only: optional flag
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define SNAP_TYPE_CUSTOM_ACTIONS (snap_custom_actions_get_type ())
G_DECLARE_FINAL_TYPE (SnapCustomActions, snap_custom_actions, SNAP, CUSTOM_ACTIONS, GObject)

typedef struct
{
    gchar *name;
    gchar *command;
    gchar *icon;
    gchar *extensions;
    gboolean directories_only;
} SnapCustomAction;

SnapCustomActions *snap_custom_actions_get_default (void);
GList             *snap_custom_actions_get_all     (SnapCustomActions *self);
void               snap_custom_actions_add         (SnapCustomActions  *self,
                                                      const SnapCustomAction *action);
void               snap_custom_actions_remove      (SnapCustomActions *self,
                                                      const gchar       *name);
void               snap_custom_actions_execute     (SnapCustomActions *self,
                                                      const gchar       *name,
                                                      GList             *files);
void               snap_custom_action_free         (SnapCustomAction  *action);

G_END_DECLS
