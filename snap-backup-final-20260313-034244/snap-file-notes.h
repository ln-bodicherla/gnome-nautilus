/* snap-file-notes.h
 *
 * Per-file notes and tags system.
 * Stores notes in ~/.snapstore/notes.json keyed by file path.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define SNAP_TYPE_FILE_NOTES (snap_file_notes_get_type ())
G_DECLARE_FINAL_TYPE (SnapFileNotes, snap_file_notes, SNAP, FILE_NOTES, GObject)

SnapFileNotes *snap_file_notes_get_default (void);

void           snap_file_notes_set_note    (SnapFileNotes *self,
                                             const gchar   *path,
                                             const gchar   *note);
const gchar   *snap_file_notes_get_note    (SnapFileNotes *self,
                                             const gchar   *path);
void           snap_file_notes_remove_note (SnapFileNotes *self,
                                             const gchar   *path);

void           snap_file_notes_add_tag     (SnapFileNotes *self,
                                             const gchar   *path,
                                             const gchar   *tag);
void           snap_file_notes_remove_tag  (SnapFileNotes *self,
                                             const gchar   *path,
                                             const gchar   *tag);
GList         *snap_file_notes_get_tags    (SnapFileNotes *self,
                                             const gchar   *path);
gboolean       snap_file_notes_has_tag     (SnapFileNotes *self,
                                             const gchar   *path,
                                             const gchar   *tag);

G_END_DECLS
