/* snap-batch-rename-dialog.c
 *
 * Batch rename dialog for multiple files.
 * Modes:
 * 1. Find & Replace: replace text in filenames
 * 2. Pattern: use patterns like [N] for number, [E] for extension
 * 3. Case: change case (upper, lower, title)
 *
 * Shows live preview of changes before applying.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-batch-rename-dialog.h"
#include <gio/gio.h>

enum
{
    MODE_FIND_REPLACE,
    MODE_PATTERN,
    MODE_CASE,
};

struct _SnapBatchRenameDialog
{
    AdwDialog parent_instance;

    GList *original_paths; /* gchar* */

    GtkWidget *mode_dropdown;
    GtkWidget *find_entry;
    GtkWidget *replace_entry;
    GtkWidget *pattern_entry;
    GtkWidget *case_dropdown;
    GtkWidget *preview_list;
    GtkWidget *apply_button;

    GtkWidget *find_replace_box;
    GtkWidget *pattern_box;
    GtkWidget *case_box;

    gint current_mode;
};

G_DEFINE_FINAL_TYPE (SnapBatchRenameDialog, snap_batch_rename_dialog, ADW_TYPE_DIALOG)

static void update_preview (SnapBatchRenameDialog *self);

static gchar *
apply_find_replace (const gchar *name,
                    const gchar *find,
                    const gchar *replace)
{
    if (find == NULL || *find == '\0')
        return g_strdup (name);

    g_autoptr(GString) result = g_string_new ("");
    const gchar *pos = name;
    gsize find_len = strlen (find);

    while (*pos != '\0')
    {
        const gchar *found = strstr (pos, find);
        if (found == NULL)
        {
            g_string_append (result, pos);
            break;
        }

        g_string_append_len (result, pos, found - pos);
        g_string_append (result, replace ? replace : "");
        pos = found + find_len;
    }

    return g_string_free (g_steal_pointer (&result), FALSE);
}

static gchar *
apply_pattern (const gchar *name, const gchar *pattern, guint index)
{
    if (pattern == NULL || *pattern == '\0')
        return g_strdup (name);

    /* Get name without extension */
    g_autofree gchar *basename_no_ext = NULL;
    const gchar *ext = strrchr (name, '.');
    if (ext != NULL && ext != name)
    {
        basename_no_ext = g_strndup (name, ext - name);
        ext++; /* skip dot */
    }
    else
    {
        basename_no_ext = g_strdup (name);
        ext = "";
    }

    g_autoptr(GString) result = g_string_new ("");
    const gchar *p = pattern;

    while (*p != '\0')
    {
        if (p[0] == '[' && p[2] == ']')
        {
            switch (p[1])
            {
                case 'N': case 'n':
                    g_string_append_printf (result, "%03u", index + 1);
                    p += 3;
                    continue;
                case 'O': case 'o':
                    g_string_append (result, basename_no_ext);
                    p += 3;
                    continue;
                case 'E': case 'e':
                    g_string_append (result, ext);
                    p += 3;
                    continue;
                default:
                    break;
            }
        }
        g_string_append_c (result, *p);
        p++;
    }

    /* Add extension if not included in pattern */
    if (ext[0] != '\0' && strstr (pattern, "[E]") == NULL && strstr (pattern, "[e]") == NULL)
    {
        g_string_append_c (result, '.');
        g_string_append (result, ext);
    }

    return g_string_free (g_steal_pointer (&result), FALSE);
}

static gchar *
apply_case_change (const gchar *name, gint case_mode)
{
    switch (case_mode)
    {
        case 0: /* lowercase */
            return g_utf8_strdown (name, -1);
        case 1: /* UPPERCASE */
            return g_utf8_strup (name, -1);
        case 2: /* Title Case */
        {
            g_autofree gchar *lower = g_utf8_strdown (name, -1);
            gchar *result = g_strdup (lower);
            gboolean capitalize_next = TRUE;
            gchar *p = result;

            while (*p != '\0')
            {
                if (capitalize_next && g_ascii_isalpha (*p))
                {
                    *p = g_ascii_toupper (*p);
                    capitalize_next = FALSE;
                }
                else if (*p == ' ' || *p == '_' || *p == '-' || *p == '.')
                {
                    capitalize_next = TRUE;
                }
                p++;
            }
            return result;
        }
        default:
            return g_strdup (name);
    }
}

static gchar *
compute_new_name (SnapBatchRenameDialog *self,
                  const gchar           *original_name,
                  guint                  index)
{
    switch (self->current_mode)
    {
        case MODE_FIND_REPLACE:
            return apply_find_replace (
                original_name,
                gtk_editable_get_text (GTK_EDITABLE (self->find_entry)),
                gtk_editable_get_text (GTK_EDITABLE (self->replace_entry)));

        case MODE_PATTERN:
            return apply_pattern (
                original_name,
                gtk_editable_get_text (GTK_EDITABLE (self->pattern_entry)),
                index);

        case MODE_CASE:
            return apply_case_change (
                original_name,
                gtk_drop_down_get_selected (GTK_DROP_DOWN (self->case_dropdown)));

        default:
            return g_strdup (original_name);
    }
}

static void
update_preview (SnapBatchRenameDialog *self)
{
    /* Clear existing preview */
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child (self->preview_list)) != NULL)
        gtk_list_box_remove (GTK_LIST_BOX (self->preview_list), child);

    guint index = 0;
    for (GList *l = self->original_paths; l != NULL; l = l->next, index++)
    {
        const gchar *path = l->data;
        g_autofree gchar *original = g_path_get_basename (path);
        g_autofree gchar *newname = compute_new_name (self, original, index);

        GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_margin_start (row, 8);
        gtk_widget_set_margin_end (row, 8);
        gtk_widget_set_margin_top (row, 4);
        gtk_widget_set_margin_bottom (row, 4);

        GtkWidget *orig_label = gtk_label_new (original);
        gtk_label_set_xalign (GTK_LABEL (orig_label), 0);
        gtk_label_set_ellipsize (GTK_LABEL (orig_label), PANGO_ELLIPSIZE_END);
        gtk_widget_set_hexpand (orig_label, TRUE);
        gtk_widget_add_css_class (orig_label, "dim-label");
        gtk_box_append (GTK_BOX (row), orig_label);

        GtkWidget *arrow = gtk_label_new ("\xe2\x86\x92"); /* → */
        gtk_widget_add_css_class (arrow, "dim-label");
        gtk_box_append (GTK_BOX (row), arrow);

        GtkWidget *new_label = gtk_label_new (newname);
        gtk_label_set_xalign (GTK_LABEL (new_label), 0);
        gtk_label_set_ellipsize (GTK_LABEL (new_label), PANGO_ELLIPSIZE_END);
        gtk_widget_set_hexpand (new_label, TRUE);

        if (g_strcmp0 (original, newname) != 0)
            gtk_widget_add_css_class (new_label, "accent");

        gtk_box_append (GTK_BOX (row), new_label);

        gtk_list_box_append (GTK_LIST_BOX (self->preview_list), row);
    }
}

static void
on_input_changed (GtkEditable *editable, gpointer user_data)
{
    update_preview (SNAP_BATCH_RENAME_DIALOG (user_data));
}

static void
on_mode_changed (GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    SnapBatchRenameDialog *self = SNAP_BATCH_RENAME_DIALOG (user_data);
    self->current_mode = gtk_drop_down_get_selected (dropdown);

    gtk_widget_set_visible (self->find_replace_box, self->current_mode == MODE_FIND_REPLACE);
    gtk_widget_set_visible (self->pattern_box, self->current_mode == MODE_PATTERN);
    gtk_widget_set_visible (self->case_box, self->current_mode == MODE_CASE);

    update_preview (self);
}

static void
on_case_changed (GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    update_preview (SNAP_BATCH_RENAME_DIALOG (user_data));
}

static void
on_apply_clicked (GtkButton *button, gpointer user_data)
{
    SnapBatchRenameDialog *self = SNAP_BATCH_RENAME_DIALOG (user_data);

    guint index = 0;
    for (GList *l = self->original_paths; l != NULL; l = l->next, index++)
    {
        const gchar *path = l->data;
        g_autofree gchar *original = g_path_get_basename (path);
        g_autofree gchar *newname = compute_new_name (self, original, index);

        if (g_strcmp0 (original, newname) == 0)
            continue;

        g_autofree gchar *dir = g_path_get_dirname (path);
        g_autofree gchar *newpath = g_build_filename (dir, newname, NULL);

        GFile *src = g_file_new_for_path (path);
        GFile *dst = g_file_new_for_path (newpath);

        g_file_move (src, dst, G_FILE_COPY_NONE, NULL, NULL, NULL, NULL);

        g_object_unref (src);
        g_object_unref (dst);
    }

    adw_dialog_close (ADW_DIALOG (self));
}

static void
snap_batch_rename_dialog_finalize (GObject *object)
{
    SnapBatchRenameDialog *self = SNAP_BATCH_RENAME_DIALOG (object);
    g_list_free_full (self->original_paths, g_free);
    G_OBJECT_CLASS (snap_batch_rename_dialog_parent_class)->finalize (object);
}

static void
snap_batch_rename_dialog_class_init (SnapBatchRenameDialogClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = snap_batch_rename_dialog_finalize;
}

static void
snap_batch_rename_dialog_init (SnapBatchRenameDialog *self)
{
    GtkWidget *toolbar_view, *header_bar, *scrolled, *content_box;
    GtkWidget *mode_group, *preview_group;

    adw_dialog_set_title (ADW_DIALOG (self), "Batch Rename");
    adw_dialog_set_content_width (ADW_DIALOG (self), 600);
    adw_dialog_set_content_height (ADW_DIALOG (self), 500);

    toolbar_view = adw_toolbar_view_new ();
    header_bar = adw_header_bar_new ();

    self->apply_button = gtk_button_new_with_label ("Rename");
    gtk_widget_add_css_class (self->apply_button, "suggested-action");
    g_signal_connect (self->apply_button, "clicked", G_CALLBACK (on_apply_clicked), self);
    adw_header_bar_pack_end (ADW_HEADER_BAR (header_bar), self->apply_button);

    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), header_bar);

    content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 18);
    gtk_widget_set_margin_top (content_box, 12);
    gtk_widget_set_margin_bottom (content_box, 24);
    gtk_widget_set_margin_start (content_box, 12);
    gtk_widget_set_margin_end (content_box, 12);

    /* Mode selector */
    mode_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (mode_group), "Rename Mode");

    const gchar *modes[] = { "Find & Replace", "Pattern", "Case", NULL };
    self->mode_dropdown = gtk_drop_down_new_from_strings (modes);
    g_signal_connect (self->mode_dropdown, "notify::selected", G_CALLBACK (on_mode_changed), self);

    GtkWidget *mode_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (mode_row), "Mode");
    adw_action_row_add_suffix (ADW_ACTION_ROW (mode_row), self->mode_dropdown);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (mode_group), mode_row);

    gtk_box_append (GTK_BOX (content_box), mode_group);

    /* Find & Replace inputs */
    self->find_replace_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);

    self->find_entry = gtk_entry_new ();
    gtk_entry_set_placeholder_text (GTK_ENTRY (self->find_entry), "Find text...");
    g_signal_connect (self->find_entry, "changed", G_CALLBACK (on_input_changed), self);

    self->replace_entry = gtk_entry_new ();
    gtk_entry_set_placeholder_text (GTK_ENTRY (self->replace_entry), "Replace with...");
    g_signal_connect (self->replace_entry, "changed", G_CALLBACK (on_input_changed), self);

    GtkWidget *fr_group = adw_preferences_group_new ();
    GtkWidget *find_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (find_row), "Find");
    adw_action_row_add_suffix (ADW_ACTION_ROW (find_row), self->find_entry);

    GtkWidget *replace_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (replace_row), "Replace");
    adw_action_row_add_suffix (ADW_ACTION_ROW (replace_row), self->replace_entry);

    adw_preferences_group_add (ADW_PREFERENCES_GROUP (fr_group), find_row);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (fr_group), replace_row);
    gtk_box_append (GTK_BOX (self->find_replace_box), fr_group);
    gtk_box_append (GTK_BOX (content_box), self->find_replace_box);

    /* Pattern input */
    self->pattern_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_visible (self->pattern_box, FALSE);

    self->pattern_entry = gtk_entry_new ();
    gtk_entry_set_placeholder_text (GTK_ENTRY (self->pattern_entry), "[O]_[N].[E]");
    g_signal_connect (self->pattern_entry, "changed", G_CALLBACK (on_input_changed), self);

    GtkWidget *pat_group = adw_preferences_group_new ();
    adw_preferences_group_set_description (ADW_PREFERENCES_GROUP (pat_group),
        "[O] = Original name, [N] = Number, [E] = Extension");

    GtkWidget *pat_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (pat_row), "Pattern");
    adw_action_row_add_suffix (ADW_ACTION_ROW (pat_row), self->pattern_entry);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (pat_group), pat_row);
    gtk_box_append (GTK_BOX (self->pattern_box), pat_group);
    gtk_box_append (GTK_BOX (content_box), self->pattern_box);

    /* Case input */
    self->case_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_visible (self->case_box, FALSE);

    const gchar *cases[] = { "lowercase", "UPPERCASE", "Title Case", NULL };
    self->case_dropdown = gtk_drop_down_new_from_strings (cases);
    g_signal_connect (self->case_dropdown, "notify::selected", G_CALLBACK (on_case_changed), self);

    GtkWidget *case_group = adw_preferences_group_new ();
    GtkWidget *case_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (case_row), "Case");
    adw_action_row_add_suffix (ADW_ACTION_ROW (case_row), self->case_dropdown);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (case_group), case_row);
    gtk_box_append (GTK_BOX (self->case_box), case_group);
    gtk_box_append (GTK_BOX (content_box), self->case_box);

    /* Preview */
    preview_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (preview_group), "Preview");

    self->preview_list = gtk_list_box_new ();
    gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->preview_list), GTK_SELECTION_NONE);
    gtk_widget_add_css_class (self->preview_list, "boxed-list");

    adw_preferences_group_add (ADW_PREFERENCES_GROUP (preview_group), self->preview_list);
    gtk_box_append (GTK_BOX (content_box), preview_group);

    scrolled = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), content_box);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view), scrolled);
    adw_dialog_set_child (ADW_DIALOG (self), toolbar_view);

    self->current_mode = MODE_FIND_REPLACE;
}

void
snap_batch_rename_dialog_present (GList *paths, GtkWindow *parent)
{
    SnapBatchRenameDialog *self = g_object_new (SNAP_TYPE_BATCH_RENAME_DIALOG, NULL);

    /* Copy paths */
    self->original_paths = NULL;
    for (GList *l = paths; l != NULL; l = l->next)
        self->original_paths = g_list_prepend (self->original_paths, g_strdup (l->data));
    self->original_paths = g_list_reverse (self->original_paths);

    /* Set title with count */
    g_autofree gchar *title = g_strdup_printf ("Batch Rename (%u files)",
                                                g_list_length (self->original_paths));
    adw_dialog_set_title (ADW_DIALOG (self), title);

    update_preview (self);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
}
