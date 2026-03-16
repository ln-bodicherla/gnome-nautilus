/* snap-terminal-panel.c
 *
 * Embedded terminal panel (like Dolphin's F4 terminal).
 * Provides a simple command entry at the bottom of the window
 * that runs commands in the current directory.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-terminal-panel.h"
#include <gio/gio.h>

struct _SnapTerminalPanel
{
    GtkBox parent_instance;

    GtkWidget *entry;
    GtkWidget *output_view;
    GtkWidget *scrolled;
    GtkWidget *prompt_label;
    gchar     *current_directory;
};

G_DEFINE_FINAL_TYPE (SnapTerminalPanel, snap_terminal_panel, GTK_TYPE_BOX)

static void
append_output (SnapTerminalPanel *self,
               const gchar       *text,
               gboolean           is_error)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->output_view));
    GtkTextIter iter;

    gtk_text_buffer_get_end_iter (buffer, &iter);
    gtk_text_buffer_insert (buffer, &iter, text, -1);
    gtk_text_buffer_insert (buffer, &iter, "\n", 1);

    /* Auto-scroll to bottom */
    gtk_text_buffer_get_end_iter (buffer, &iter);
    GtkTextMark *mark = gtk_text_buffer_get_mark (buffer, "end");
    if (mark == NULL)
        mark = gtk_text_buffer_create_mark (buffer, "end", &iter, FALSE);
    else
        gtk_text_buffer_move_mark (buffer, mark, &iter);
    gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (self->output_view), mark);
}

static void
on_command_activate (GtkEntry          *entry,
                     SnapTerminalPanel *self)
{
    const gchar *command = gtk_editable_get_text (GTK_EDITABLE (entry));

    if (command == NULL || *command == '\0')
        return;

    /* Show the command in output */
    g_autofree gchar *prompt_text = g_strdup_printf ("$ %s", command);
    append_output (self, prompt_text, FALSE);

    /* Handle built-in 'cd' */
    if (g_str_has_prefix (command, "cd "))
    {
        const gchar *dir = command + 3;
        while (*dir == ' ') dir++;

        g_autofree gchar *new_dir = NULL;
        if (dir[0] == '/')
        {
            new_dir = g_strdup (dir);
        }
        else if (g_strcmp0 (dir, "~") == 0 || *dir == '\0')
        {
            new_dir = g_strdup (g_get_home_dir ());
        }
        else
        {
            new_dir = g_build_filename (self->current_directory, dir, NULL);
        }

        if (g_file_test (new_dir, G_FILE_TEST_IS_DIR))
        {
            snap_terminal_panel_set_directory (self, new_dir);
        }
        else
        {
            g_autofree gchar *err = g_strdup_printf ("cd: no such directory: %s", dir);
            append_output (self, err, TRUE);
        }

        gtk_editable_set_text (GTK_EDITABLE (entry), "");
        return;
    }

    /* Run command asynchronously */
    gchar *stdout_text = NULL;
    gchar *stderr_text = NULL;
    gint exit_status = 0;
    GError *error = NULL;

    gchar *argv[] = { "/bin/sh", "-c", (gchar *) command, NULL };

    g_spawn_sync (self->current_directory,
                  argv,
                  NULL, /* env */
                  G_SPAWN_SEARCH_PATH,
                  NULL, NULL,
                  &stdout_text,
                  &stderr_text,
                  &exit_status,
                  &error);

    if (error != NULL)
    {
        append_output (self, error->message, TRUE);
        g_error_free (error);
    }
    else
    {
        if (stdout_text != NULL && *stdout_text != '\0')
        {
            /* Trim trailing newline */
            gsize len = strlen (stdout_text);
            if (len > 0 && stdout_text[len - 1] == '\n')
                stdout_text[len - 1] = '\0';
            append_output (self, stdout_text, FALSE);
        }
        if (stderr_text != NULL && *stderr_text != '\0')
        {
            gsize len = strlen (stderr_text);
            if (len > 0 && stderr_text[len - 1] == '\n')
                stderr_text[len - 1] = '\0';
            append_output (self, stderr_text, TRUE);
        }
    }

    g_free (stdout_text);
    g_free (stderr_text);

    gtk_editable_set_text (GTK_EDITABLE (entry), "");
}

static void
update_prompt (SnapTerminalPanel *self)
{
    if (self->current_directory != NULL)
    {
        g_autofree gchar *basename = g_path_get_basename (self->current_directory);
        g_autofree gchar *prompt = g_strdup_printf ("%s $", basename);
        gtk_label_set_text (GTK_LABEL (self->prompt_label), prompt);
    }
}

static void
snap_terminal_panel_finalize (GObject *object)
{
    SnapTerminalPanel *self = SNAP_TERMINAL_PANEL (object);
    g_free (self->current_directory);
    G_OBJECT_CLASS (snap_terminal_panel_parent_class)->finalize (object);
}

static void
snap_terminal_panel_class_init (SnapTerminalPanelClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = snap_terminal_panel_finalize;
}

static void
snap_terminal_panel_init (SnapTerminalPanel *self)
{
    GtkWidget *separator;
    GtkWidget *input_box;

    gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_size_request (GTK_WIDGET (self), -1, 150);

    /* Separator at top */
    separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append (GTK_BOX (self), separator);

    /* Scrolled output area */
    self->scrolled = gtk_scrolled_window_new ();
    gtk_widget_set_vexpand (self->scrolled, TRUE);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolled),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    self->output_view = gtk_text_view_new ();
    gtk_text_view_set_editable (GTK_TEXT_VIEW (self->output_view), FALSE);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (self->output_view), FALSE);
    gtk_text_view_set_monospace (GTK_TEXT_VIEW (self->output_view), TRUE);
    gtk_text_view_set_left_margin (GTK_TEXT_VIEW (self->output_view), 6);
    gtk_text_view_set_right_margin (GTK_TEXT_VIEW (self->output_view), 6);
    gtk_text_view_set_top_margin (GTK_TEXT_VIEW (self->output_view), 4);
    gtk_text_view_set_bottom_margin (GTK_TEXT_VIEW (self->output_view), 4);
    gtk_widget_add_css_class (self->output_view, "snap-terminal-output");

    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (self->scrolled), self->output_view);
    gtk_box_append (GTK_BOX (self), self->scrolled);

    /* Input row: prompt label + entry */
    input_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_margin_start (input_box, 6);
    gtk_widget_set_margin_end (input_box, 6);
    gtk_widget_set_margin_top (input_box, 2);
    gtk_widget_set_margin_bottom (input_box, 2);

    self->prompt_label = gtk_label_new ("~ $");
    gtk_widget_add_css_class (self->prompt_label, "snap-terminal-prompt");
    gtk_box_append (GTK_BOX (input_box), self->prompt_label);

    self->entry = gtk_entry_new ();
    gtk_widget_set_hexpand (self->entry, TRUE);
    gtk_widget_add_css_class (self->entry, "snap-terminal-entry");
    gtk_entry_set_placeholder_text (GTK_ENTRY (self->entry), "Enter command...");
    g_signal_connect (self->entry, "activate", G_CALLBACK (on_command_activate), self);
    gtk_box_append (GTK_BOX (input_box), self->entry);

    gtk_box_append (GTK_BOX (self), input_box);

    self->current_directory = g_strdup (g_get_home_dir ());
    update_prompt (self);
}

GtkWidget *
snap_terminal_panel_new (void)
{
    return g_object_new (SNAP_TYPE_TERMINAL_PANEL, NULL);
}

void
snap_terminal_panel_set_directory (SnapTerminalPanel *self,
                                    const gchar       *directory)
{
    g_return_if_fail (SNAP_IS_TERMINAL_PANEL (self));

    if (g_strcmp0 (self->current_directory, directory) == 0)
        return;

    g_free (self->current_directory);
    self->current_directory = g_strdup (directory);
    update_prompt (self);
}
