/* snap-config.h
 *
 * Configuration read/write from ~/.snapstore/config.json using json-glib.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define SNAP_TYPE_CONFIG (snap_config_get_type ())
G_DECLARE_FINAL_TYPE (SnapConfig, snap_config, SNAP, CONFIG, GObject)

SnapConfig    *snap_config_get_default       (void);

/* Load/save */
gboolean       snap_config_load              (SnapConfig   *self,
                                              GError      **error);
gboolean       snap_config_save              (SnapConfig   *self,
                                              GError      **error);

/* Getters */
gint           snap_config_get_max_versions  (SnapConfig   *self);
gboolean       snap_config_get_auto_save     (SnapConfig   *self);
gint           snap_config_get_auto_save_interval (SnapConfig *self);
gboolean       snap_config_get_compression   (SnapConfig   *self);
const gchar   *snap_config_get_store_path    (SnapConfig   *self);
gboolean       snap_config_get_encryption    (SnapConfig   *self);
const gchar   *snap_config_get_default_branch(SnapConfig   *self);
gint           snap_config_get_gc_keep       (SnapConfig   *self);
gboolean       snap_config_get_notifications (SnapConfig   *self);
gboolean       snap_config_get_auto_watch    (SnapConfig   *self);
gint           snap_config_get_server_port   (SnapConfig   *self);
const gchar * const *snap_config_get_ignore_patterns (SnapConfig *self);

/* Setters */
void           snap_config_set_max_versions  (SnapConfig   *self,
                                              gint          value);
void           snap_config_set_auto_save     (SnapConfig   *self,
                                              gboolean      value);
void           snap_config_set_auto_save_interval (SnapConfig *self,
                                              gint          seconds);
void           snap_config_set_compression   (SnapConfig   *self,
                                              gboolean      value);
void           snap_config_set_store_path    (SnapConfig   *self,
                                              const gchar  *path);
void           snap_config_set_encryption    (SnapConfig   *self,
                                              gboolean      value);
void           snap_config_set_default_branch(SnapConfig   *self,
                                              const gchar  *branch);
void           snap_config_set_gc_keep       (SnapConfig   *self,
                                              gint          value);
void           snap_config_set_notifications (SnapConfig   *self,
                                              gboolean      value);
void           snap_config_set_auto_watch    (SnapConfig   *self,
                                              gboolean      value);
void           snap_config_set_server_port   (SnapConfig   *self,
                                              gint          port);
void           snap_config_set_ignore_patterns (SnapConfig *self,
                                              const gchar * const *patterns);

G_END_DECLS
