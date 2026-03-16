/* snap-shortcuts-dialog.c
 *
 * Keyboard shortcuts reference dialog.
 * Shows all available shortcuts organized by category.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-shortcuts-dialog.h"

struct _SnapShortcutsDialog
{
    AdwDialog parent_instance;
};

G_DEFINE_FINAL_TYPE (SnapShortcutsDialog, snap_shortcuts_dialog, ADW_TYPE_DIALOG)

typedef struct
{
    const gchar *key;
    const gchar *description;
} ShortcutEntry;

static const ShortcutEntry navigation_shortcuts[] = {
    { "Alt+Left",         "Go back" },
    { "Alt+Right",        "Go forward" },
    { "Alt+Up",           "Go to parent directory" },
    { "Alt+Home",         "Go to home directory" },
    { "Ctrl+L",           "Edit location bar" },
    { "F5",               "Reload" },
    { "Ctrl+D",           "Bookmark current directory" },
    { NULL, NULL }
};

static const ShortcutEntry file_shortcuts[] = {
    { "Ctrl+Shift+N",     "New folder" },
    { "F2",               "Rename" },
    { "Delete",           "Move to trash" },
    { "Shift+Delete",     "Delete permanently" },
    { "Ctrl+C",           "Copy" },
    { "Ctrl+X",           "Cut" },
    { "Ctrl+V",           "Paste" },
    { "Ctrl+A",           "Select all" },
    { "Ctrl+S",           "Select pattern" },
    { "Ctrl+I",           "Properties" },
    { NULL, NULL }
};

static const ShortcutEntry view_shortcuts[] = {
    { "Ctrl+1",           "List view" },
    { "Ctrl+2",           "Grid view" },
    { "Ctrl+H",           "Show/hide hidden files" },
    { "Ctrl+Plus",        "Zoom in" },
    { "Ctrl+Minus",       "Zoom out" },
    { "Ctrl+0",           "Reset zoom" },
    { "F3",               "Toggle split view" },
    { "F4",               "Toggle terminal panel" },
    { "F9",               "Toggle sidebar" },
    { "F10",              "Toggle places toolbar" },
    { "F11",              "Toggle file preview" },
    { "Ctrl+I",           "Quick filter" },
    { NULL, NULL }
};

static const ShortcutEntry tab_shortcuts[] = {
    { "Ctrl+T",           "New tab" },
    { "Ctrl+W",           "Close tab" },
    { "Ctrl+Shift+T",     "Restore closed tab" },
    { "Alt+1...9",        "Switch to tab 1-9" },
    { "Ctrl+Shift+PgUp",  "Move tab left" },
    { "Ctrl+Shift+PgDn",  "Move tab right" },
    { NULL, NULL }
};

static const ShortcutEntry snap_shortcuts[] = {
    { "Ctrl+Shift+S",     "Save snapshot" },
    { "Ctrl+Shift+H",     "Show history" },
    { "Ctrl+Shift+D",     "Show diff" },
    { "Ctrl+Shift+B",     "Browse versions" },
    { "Ctrl+Shift+I",     "Toggle inspector" },
    { NULL, NULL }
};

static const ShortcutEntry app_shortcuts[] = {
    { "Ctrl+N",           "New window" },
    { "Ctrl+Q",           "Quit" },
    { "Ctrl+,",           "Preferences" },
    { "F1",               "Help" },
    { "F10",              "Location menu" },
    { "Ctrl+Z",           "Undo" },
    { "Ctrl+Shift+Z",     "Redo" },
    { "Ctrl+F",           "Search" },
    { "Ctrl+Shift+F",     "Search everywhere" },
    { "Ctrl+.",           "Open terminal" },
    { NULL, NULL }
};

static void
add_shortcut_section (GtkBox          *container,
                      const gchar     *title,
                      const ShortcutEntry *entries)
{
    GtkWidget *group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), title);

    for (const ShortcutEntry *e = entries; e->key != NULL; e++)
    {
        GtkWidget *row = adw_action_row_new ();
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), e->description);

        GtkWidget *label = gtk_label_new (e->key);
        gtk_widget_add_css_class (label, "monospace");
        gtk_widget_add_css_class (label, "dim-label");
        adw_action_row_add_suffix (ADW_ACTION_ROW (row), label);

        adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);
    }

    gtk_box_append (container, group);
}

static void
snap_shortcuts_dialog_class_init (SnapShortcutsDialogClass *klass)
{
}

static void
snap_shortcuts_dialog_init (SnapShortcutsDialog *self)
{
    GtkWidget *toolbar_view;
    GtkWidget *header_bar;
    GtkWidget *scrolled;
    GtkWidget *clamp;
    GtkWidget *content_box;

    adw_dialog_set_title (ADW_DIALOG (self), "Keyboard Shortcuts");
    adw_dialog_set_content_width (ADW_DIALOG (self), 500);
    adw_dialog_set_content_height (ADW_DIALOG (self), 600);

    toolbar_view = adw_toolbar_view_new ();
    header_bar = adw_header_bar_new ();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), header_bar);

    content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 18);
    gtk_widget_set_margin_top (content_box, 12);
    gtk_widget_set_margin_bottom (content_box, 24);
    gtk_widget_set_margin_start (content_box, 12);
    gtk_widget_set_margin_end (content_box, 12);

    add_shortcut_section (GTK_BOX (content_box), "Navigation", navigation_shortcuts);
    add_shortcut_section (GTK_BOX (content_box), "Files", file_shortcuts);
    add_shortcut_section (GTK_BOX (content_box), "View", view_shortcuts);
    add_shortcut_section (GTK_BOX (content_box), "Tabs", tab_shortcuts);
    add_shortcut_section (GTK_BOX (content_box), "Snap Versioning", snap_shortcuts);
    add_shortcut_section (GTK_BOX (content_box), "Application", app_shortcuts);

    clamp = adw_clamp_new ();
    adw_clamp_set_child (ADW_CLAMP (clamp), content_box);

    scrolled = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), clamp);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view), scrolled);
    adw_dialog_set_child (ADW_DIALOG (self), toolbar_view);
}

void
snap_shortcuts_dialog_present (GtkWindow *parent)
{
    SnapShortcutsDialog *self = g_object_new (SNAP_TYPE_SHORTCUTS_DIALOG, NULL);
    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
}
