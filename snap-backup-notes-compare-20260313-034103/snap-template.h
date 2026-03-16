/* snap-template.h
 *
 * Per-directory .snapconfig templates.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef struct _SnapTemplate SnapTemplate;
struct _SnapTemplate
{
    gchar   *directory;
    gint     max_versions;
    gboolean auto_save;
    gint     auto_save_interval;
    gboolean compression;
    gboolean encryption;
    gchar  **ignore_patterns;
    gchar   *default_branch;
};

void           snap_template_free            (SnapTemplate *tmpl);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (SnapTemplate, snap_template_free)

/* Load .snapconfig from a directory */
SnapTemplate  *snap_template_load            (const gchar  *directory,
                                              GError      **error);

/* Save .snapconfig to a directory */
gboolean       snap_template_save            (SnapTemplate *tmpl,
                                              GError      **error);

/* Find the effective template for a file (walks parent dirs) */
SnapTemplate  *snap_template_find_for_file   (const gchar  *file_path);

/* Create default template */
SnapTemplate  *snap_template_new_default     (const gchar  *directory);

G_END_DECLS
