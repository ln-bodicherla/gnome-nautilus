/* snap-diff.c
 *
 * LCS-based text diff algorithm.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-diff.h"
#include <string.h>

/* --- Free functions --- */

void
snap_diff_line_free (SnapDiffLine *line)
{
    if (line == NULL)
        return;
    g_free (line->text);
    g_free (line);
}

void
snap_diff_hunk_free (SnapDiffHunk *hunk)
{
    if (hunk == NULL)
        return;
    g_list_free_full (hunk->lines, (GDestroyNotify) snap_diff_line_free);
    g_free (hunk);
}

void
snap_diff_result_free (SnapDiffResult *result)
{
    if (result == NULL)
        return;
    g_list_free_full (result->hunks, (GDestroyNotify) snap_diff_hunk_free);
    g_free (result);
}

void
snap_diff_side_pair_free (SnapDiffSidePair *pair)
{
    if (pair == NULL)
        return;
    snap_diff_line_free (pair->left);
    snap_diff_line_free (pair->right);
    g_free (pair);
}

/* --- LCS computation --- */

static gchar **
split_lines (const gchar *text, gint *out_count)
{
    if (text == NULL || text[0] == '\0')
    {
        *out_count = 0;
        return g_new0 (gchar *, 1);
    }

    gchar **lines = g_strsplit (text, "\n", -1);
    *out_count = g_strv_length (lines);

    /* Remove trailing empty line if text ends with \n */
    if (*out_count > 0 && lines[*out_count - 1][0] == '\0')
    {
        g_free (lines[*out_count - 1]);
        lines[*out_count - 1] = NULL;
        (*out_count)--;
    }

    return lines;
}

typedef enum
{
    EDIT_EQUAL,
    EDIT_INSERT,
    EDIT_DELETE,
} EditType;

typedef struct
{
    EditType type;
    gint     old_idx;
    gint     new_idx;
} EditOp;

static GList *
compute_edit_script (gchar **old_lines, gint old_count,
                     gchar **new_lines, gint new_count)
{
    /* Build LCS table */
    gint **lcs = g_new0 (gint *, old_count + 1);
    for (gint i = 0; i <= old_count; i++)
        lcs[i] = g_new0 (gint, new_count + 1);

    for (gint i = 1; i <= old_count; i++)
    {
        for (gint j = 1; j <= new_count; j++)
        {
            if (g_strcmp0 (old_lines[i - 1], new_lines[j - 1]) == 0)
                lcs[i][j] = lcs[i - 1][j - 1] + 1;
            else
                lcs[i][j] = MAX (lcs[i - 1][j], lcs[i][j - 1]);
        }
    }

    /* Backtrack to get edit script */
    GList *ops = NULL;
    gint i = old_count, j = new_count;

    while (i > 0 || j > 0)
    {
        EditOp *op = g_new0 (EditOp, 1);

        if (i > 0 && j > 0 && g_strcmp0 (old_lines[i - 1], new_lines[j - 1]) == 0)
        {
            op->type = EDIT_EQUAL;
            op->old_idx = i - 1;
            op->new_idx = j - 1;
            i--;
            j--;
        }
        else if (j > 0 && (i == 0 || lcs[i][j - 1] >= lcs[i - 1][j]))
        {
            op->type = EDIT_INSERT;
            op->new_idx = j - 1;
            op->old_idx = -1;
            j--;
        }
        else
        {
            op->type = EDIT_DELETE;
            op->old_idx = i - 1;
            op->new_idx = -1;
            i--;
        }

        ops = g_list_prepend (ops, op);
    }

    /* Free LCS table */
    for (gint k = 0; k <= old_count; k++)
        g_free (lcs[k]);
    g_free (lcs);

    return ops;
}

/* --- Main diff --- */

SnapDiffResult *
snap_diff_compute (const gchar *old_text, const gchar *new_text, gint context_lines)
{
    gint old_count, new_count;
    g_auto (GStrv) old_lines = split_lines (old_text, &old_count);
    g_auto (GStrv) new_lines = split_lines (new_text, &new_count);

    if (context_lines < 0)
        context_lines = 3;

    GList *edit_ops = compute_edit_script (old_lines, old_count, new_lines, new_count);

    /* Convert edit ops to diff lines */
    GList *all_lines = NULL;
    gint additions = 0, deletions = 0;

    for (GList *l = edit_ops; l != NULL; l = l->next)
    {
        EditOp *op = (EditOp *) l->data;
        SnapDiffLine *dl = g_new0 (SnapDiffLine, 1);

        switch (op->type)
        {
            case EDIT_EQUAL:
                dl->type = SNAP_DIFF_LINE_CONTEXT;
                dl->text = g_strdup (old_lines[op->old_idx]);
                dl->old_line_number = op->old_idx + 1;
                dl->new_line_number = op->new_idx + 1;
                break;

            case EDIT_INSERT:
                dl->type = SNAP_DIFF_LINE_ADDITION;
                dl->text = g_strdup (new_lines[op->new_idx]);
                dl->old_line_number = -1;
                dl->new_line_number = op->new_idx + 1;
                additions++;
                break;

            case EDIT_DELETE:
                dl->type = SNAP_DIFF_LINE_DELETION;
                dl->text = g_strdup (old_lines[op->old_idx]);
                dl->old_line_number = op->old_idx + 1;
                dl->new_line_number = -1;
                deletions++;
                break;
        }

        all_lines = g_list_append (all_lines, dl);
    }

    g_list_free_full (edit_ops, g_free);

    /* Group into hunks */
    GList *hunks = NULL;
    SnapDiffHunk *current_hunk = NULL;
    gint lines_since_change = 0;
    GList *context_buffer = NULL;

    for (GList *l = all_lines; l != NULL; l = l->next)
    {
        SnapDiffLine *dl = (SnapDiffLine *) l->data;

        if (dl->type != SNAP_DIFF_LINE_CONTEXT)
        {
            /* Start new hunk if needed */
            if (current_hunk == NULL)
            {
                current_hunk = g_new0 (SnapDiffHunk, 1);
                current_hunk->old_start = dl->old_line_number > 0 ? dl->old_line_number : 1;
                current_hunk->new_start = dl->new_line_number > 0 ? dl->new_line_number : 1;

                /* Add buffered context lines */
                gint ctx_count = g_list_length (context_buffer);
                gint skip = ctx_count > context_lines ? ctx_count - context_lines : 0;
                gint idx = 0;
                for (GList *c = context_buffer; c != NULL; c = c->next)
                {
                    if (idx >= skip)
                    {
                        SnapDiffLine *ctx = (SnapDiffLine *) c->data;
                        SnapDiffLine *copy = g_new0 (SnapDiffLine, 1);
                        copy->type = ctx->type;
                        copy->text = g_strdup (ctx->text);
                        copy->old_line_number = ctx->old_line_number;
                        copy->new_line_number = ctx->new_line_number;
                        current_hunk->lines = g_list_append (current_hunk->lines, copy);
                        if (idx == skip)
                        {
                            current_hunk->old_start = copy->old_line_number;
                            current_hunk->new_start = copy->new_line_number;
                        }
                    }
                    idx++;
                }
            }

            current_hunk->lines = g_list_append (current_hunk->lines,
                                                  g_steal_pointer (&dl));
            /* Replace the data pointer so we don't double-free */
            l->data = NULL;
            lines_since_change = 0;

            g_list_free (context_buffer);
            context_buffer = NULL;
        }
        else
        {
            if (current_hunk != NULL)
            {
                lines_since_change++;
                if (lines_since_change <= context_lines)
                {
                    SnapDiffLine *copy = g_new0 (SnapDiffLine, 1);
                    copy->type = dl->type;
                    copy->text = g_strdup (dl->text);
                    copy->old_line_number = dl->old_line_number;
                    copy->new_line_number = dl->new_line_number;
                    current_hunk->lines = g_list_append (current_hunk->lines, copy);
                }
                else
                {
                    /* Finalize hunk */
                    hunks = g_list_append (hunks, current_hunk);
                    current_hunk = NULL;
                    lines_since_change = 0;
                    context_buffer = g_list_append (NULL, dl);
                    l->data = NULL;
                    continue;
                }
            }
            else
            {
                context_buffer = g_list_append (context_buffer, dl);
                l->data = NULL;
                continue;
            }
        }
    }

    if (current_hunk != NULL)
        hunks = g_list_append (hunks, current_hunk);

    g_list_free_full (context_buffer, (GDestroyNotify) snap_diff_line_free);
    g_list_free_full (all_lines, (GDestroyNotify) snap_diff_line_free);

    /* Compute hunk line counts */
    for (GList *h = hunks; h != NULL; h = h->next)
    {
        SnapDiffHunk *hunk = (SnapDiffHunk *) h->data;
        hunk->old_count = 0;
        hunk->new_count = 0;
        for (GList *ll = hunk->lines; ll != NULL; ll = ll->next)
        {
            SnapDiffLine *dl = (SnapDiffLine *) ll->data;
            if (dl->type != SNAP_DIFF_LINE_ADDITION)
                hunk->old_count++;
            if (dl->type != SNAP_DIFF_LINE_DELETION)
                hunk->new_count++;
        }
    }

    SnapDiffResult *result = g_new0 (SnapDiffResult, 1);
    result->hunks = hunks;
    result->additions = additions;
    result->deletions = deletions;
    result->context_lines = context_lines;

    return result;
}

/* --- Unified diff string --- */

gchar *
snap_diff_unified (SnapDiffResult *result,
                   const gchar    *old_label,
                   const gchar    *new_label)
{
    g_return_val_if_fail (result != NULL, NULL);

    GString *str = g_string_new (NULL);

    g_string_append_printf (str, "--- %s\n", old_label ? old_label : "a");
    g_string_append_printf (str, "+++ %s\n", new_label ? new_label : "b");

    for (GList *h = result->hunks; h != NULL; h = h->next)
    {
        SnapDiffHunk *hunk = (SnapDiffHunk *) h->data;

        g_string_append_printf (str, "@@ -%d,%d +%d,%d @@\n",
                                hunk->old_start, hunk->old_count,
                                hunk->new_start, hunk->new_count);

        for (GList *l = hunk->lines; l != NULL; l = l->next)
        {
            SnapDiffLine *dl = (SnapDiffLine *) l->data;
            switch (dl->type)
            {
                case SNAP_DIFF_LINE_CONTEXT:
                    g_string_append_printf (str, " %s\n", dl->text);
                    break;
                case SNAP_DIFF_LINE_ADDITION:
                    g_string_append_printf (str, "+%s\n", dl->text);
                    break;
                case SNAP_DIFF_LINE_DELETION:
                    g_string_append_printf (str, "-%s\n", dl->text);
                    break;
            }
        }
    }

    return g_string_free (str, FALSE);
}

/* --- Side-by-side --- */

GList *
snap_diff_side_by_side (SnapDiffResult *result)
{
    g_return_val_if_fail (result != NULL, NULL);

    GList *pairs = NULL;

    for (GList *h = result->hunks; h != NULL; h = h->next)
    {
        SnapDiffHunk *hunk = (SnapDiffHunk *) h->data;

        for (GList *l = hunk->lines; l != NULL; l = l->next)
        {
            SnapDiffLine *dl = (SnapDiffLine *) l->data;
            SnapDiffSidePair *pair = g_new0 (SnapDiffSidePair, 1);

            switch (dl->type)
            {
                case SNAP_DIFF_LINE_CONTEXT:
                {
                    SnapDiffLine *left = g_new0 (SnapDiffLine, 1);
                    left->type = SNAP_DIFF_LINE_CONTEXT;
                    left->text = g_strdup (dl->text);
                    left->old_line_number = dl->old_line_number;

                    SnapDiffLine *right = g_new0 (SnapDiffLine, 1);
                    right->type = SNAP_DIFF_LINE_CONTEXT;
                    right->text = g_strdup (dl->text);
                    right->new_line_number = dl->new_line_number;

                    pair->left = left;
                    pair->right = right;
                    break;
                }

                case SNAP_DIFF_LINE_DELETION:
                {
                    SnapDiffLine *left = g_new0 (SnapDiffLine, 1);
                    left->type = SNAP_DIFF_LINE_DELETION;
                    left->text = g_strdup (dl->text);
                    left->old_line_number = dl->old_line_number;

                    pair->left = left;
                    pair->right = NULL;

                    /* Check if next line is an insertion (modification pair) */
                    if (l->next != NULL)
                    {
                        SnapDiffLine *next_dl = (SnapDiffLine *) l->next->data;
                        if (next_dl->type == SNAP_DIFF_LINE_ADDITION)
                        {
                            SnapDiffLine *right = g_new0 (SnapDiffLine, 1);
                            right->type = SNAP_DIFF_LINE_ADDITION;
                            right->text = g_strdup (next_dl->text);
                            right->new_line_number = next_dl->new_line_number;
                            pair->right = right;
                            l = l->next;  /* Skip the insertion */
                        }
                    }
                    break;
                }

                case SNAP_DIFF_LINE_ADDITION:
                {
                    SnapDiffLine *right = g_new0 (SnapDiffLine, 1);
                    right->type = SNAP_DIFF_LINE_ADDITION;
                    right->text = g_strdup (dl->text);
                    right->new_line_number = dl->new_line_number;

                    pair->left = NULL;
                    pair->right = right;
                    break;
                }
            }

            pairs = g_list_append (pairs, pair);
        }
    }

    return pairs;
}

gint
snap_diff_count_additions (SnapDiffResult *result)
{
    return result ? result->additions : 0;
}

gint
snap_diff_count_deletions (SnapDiffResult *result)
{
    return result ? result->deletions : 0;
}
