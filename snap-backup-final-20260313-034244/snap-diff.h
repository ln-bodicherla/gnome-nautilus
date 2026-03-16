/* snap-diff.h
 *
 * LCS-based text diff algorithm.
 * Port of Diff.swift - unified and side-by-side output.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum
{
    SNAP_DIFF_LINE_CONTEXT,
    SNAP_DIFF_LINE_ADDITION,
    SNAP_DIFF_LINE_DELETION,
} SnapDiffLineType;

typedef struct _SnapDiffLine SnapDiffLine;
struct _SnapDiffLine
{
    SnapDiffLineType  type;
    gchar            *text;
    gint              old_line_number;
    gint              new_line_number;
};

typedef struct _SnapDiffHunk SnapDiffHunk;
struct _SnapDiffHunk
{
    gint   old_start;
    gint   old_count;
    gint   new_start;
    gint   new_count;
    GList *lines;   /* GList of SnapDiffLine */
};

typedef struct _SnapDiffResult SnapDiffResult;
struct _SnapDiffResult
{
    GList  *hunks;          /* GList of SnapDiffHunk */
    gint    additions;
    gint    deletions;
    gint    context_lines;
};

/* Side-by-side pair */
typedef struct _SnapDiffSidePair SnapDiffSidePair;
struct _SnapDiffSidePair
{
    SnapDiffLine *left;
    SnapDiffLine *right;
};

void              snap_diff_line_free        (SnapDiffLine *line);
void              snap_diff_hunk_free        (SnapDiffHunk *hunk);
void              snap_diff_result_free      (SnapDiffResult *result);
void              snap_diff_side_pair_free   (SnapDiffSidePair *pair);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (SnapDiffLine, snap_diff_line_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (SnapDiffHunk, snap_diff_hunk_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (SnapDiffResult, snap_diff_result_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (SnapDiffSidePair, snap_diff_side_pair_free)

/* Compute diff between two texts */
SnapDiffResult   *snap_diff_compute          (const gchar *old_text,
                                              const gchar *new_text,
                                              gint         context_lines);

/* Generate unified diff string */
gchar            *snap_diff_unified          (SnapDiffResult *result,
                                              const gchar    *old_label,
                                              const gchar    *new_label);

/* Generate side-by-side pairs */
GList            *snap_diff_side_by_side     (SnapDiffResult *result);

/* Statistics */
gint              snap_diff_count_additions  (SnapDiffResult *result);
gint              snap_diff_count_deletions  (SnapDiffResult *result);

G_END_DECLS
