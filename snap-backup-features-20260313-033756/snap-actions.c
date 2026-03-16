/* snap-actions.c
 *
 * All GActionEntry arrays and callbacks for snap integration.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-actions.h"
#include "snap-manager.h"
#include "snap-save-dialog.h"
#include "snap-history-dialog.h"
#include "snap-diff-dialog.h"
#include "snap-restore-dialog.h"
#include "snap-browse-dialog.h"
#include "snap-log-dialog.h"
#include "snap-branch-dialog.h"
#include "snap-group-dialog.h"
#include "snap-encrypt-dialog.h"
#include "snap-sync-dialog.h"
#include "snap-backup-dialog.h"
#include "snap-config-dialog.h"
#include "snap-gc-dialog.h"
#include "snap-audit-dialog.h"
#include "snap-blame-dialog.h"
#include "snap-stats-dialog.h"
#include "snap-hook-dialog.h"
#include "snap-import-git-dialog.h"
#include "snap-stash-dialog.h"
#include "snap-inspector-panel.h"
#include "snap-shortcuts-dialog.h"
#include "snap-checksum-dialog.h"
#include "snap-batch-rename-dialog.h"
#include "snap-disk-usage.h"

#include "nautilus-files-view.h"
#include "nautilus-file.h"

/* Helper: get first selected file path */
gchar *
snap_actions_get_selected_path (gpointer view)
{
    g_autolist (NautilusFile) selection = nautilus_files_view_get_selection (
        NAUTILUS_FILES_VIEW (view));

    if (selection == NULL)
        return NULL;

    NautilusFile *file = NAUTILUS_FILE (selection->data);
    return nautilus_file_get_location (file) ?
        g_file_get_path (nautilus_file_get_location (file)) : NULL;
}

GList *
snap_actions_get_selected_paths (gpointer view)
{
    g_autolist (NautilusFile) selection = nautilus_files_view_get_selection (
        NAUTILUS_FILES_VIEW (view));

    GList *paths = NULL;
    for (GList *l = selection; l != NULL; l = l->next)
    {
        NautilusFile *file = NAUTILUS_FILE (l->data);
        GFile *location = nautilus_file_get_location (file);
        if (location != NULL)
        {
            gchar *path = g_file_get_path (location);
            if (path != NULL)
                paths = g_list_prepend (paths, path);
        }
    }
    return g_list_reverse (paths);
}

/* --- View action callbacks --- */

static void
action_snap_save (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    g_autofree gchar *path = snap_actions_get_selected_path (user_data);
    if (path == NULL)
        return;

    GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (user_data)));
    snap_save_dialog_present (path, GTK_WINDOW (toplevel));
}

static void
action_snap_history (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    g_autofree gchar *path = snap_actions_get_selected_path (user_data);
    if (path == NULL)
        return;

    GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (user_data)));
    snap_history_dialog_present (path, GTK_WINDOW (toplevel));
}

static void
action_snap_diff (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    g_autofree gchar *path = snap_actions_get_selected_path (user_data);
    if (path == NULL)
        return;

    GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (user_data)));
    snap_diff_dialog_present (path, GTK_WINDOW (toplevel));
}

static void
action_snap_restore (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    g_autofree gchar *path = snap_actions_get_selected_path (user_data);
    if (path == NULL)
        return;

    GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (user_data)));
    snap_restore_dialog_present (path, GTK_WINDOW (toplevel));
}

static void
action_snap_browse (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    g_autofree gchar *path = snap_actions_get_selected_path (user_data);
    if (path == NULL)
        return;

    GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (user_data)));
    snap_browse_dialog_present (path, GTK_WINDOW (toplevel));
}

static void
action_snap_branch (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    g_autofree gchar *path = snap_actions_get_selected_path (user_data);
    if (path == NULL)
        return;

    GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (user_data)));
    snap_branch_dialog_present (path, GTK_WINDOW (toplevel));
}

static void
action_snap_group (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GList *paths = snap_actions_get_selected_paths (user_data);
    GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (user_data)));
    snap_group_dialog_present (paths, GTK_WINDOW (toplevel));
    g_list_free_full (paths, g_free);
}

static void
action_snap_encrypt (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    g_autofree gchar *path = snap_actions_get_selected_path (user_data);
    if (path == NULL)
        return;

    GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (user_data)));
    snap_encrypt_dialog_present (path, GTK_WINDOW (toplevel));
}

static void
action_snap_stash (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    g_autofree gchar *path = snap_actions_get_selected_path (user_data);
    if (path == NULL)
        return;

    GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (user_data)));
    snap_stash_dialog_present (path, GTK_WINDOW (toplevel));
}

static void
action_snap_blame (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    g_autofree gchar *path = snap_actions_get_selected_path (user_data);
    if (path == NULL)
        return;

    GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (user_data)));
    snap_blame_dialog_present (path, GTK_WINDOW (toplevel));
}

static void
action_snap_audit (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    g_autofree gchar *path = snap_actions_get_selected_path (user_data);
    if (path == NULL)
        return;

    GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (user_data)));
    snap_audit_dialog_present (path, GTK_WINDOW (toplevel));
}

static void
action_snap_checksum (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    g_autofree gchar *path = snap_actions_get_selected_path (user_data);
    if (path == NULL)
        return;

    GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (user_data)));
    snap_checksum_dialog_present (path, GTK_WINDOW (toplevel));
}

static void
action_snap_batch_rename (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GList *paths = snap_actions_get_selected_paths (user_data);
    if (paths == NULL)
        return;

    GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (user_data)));
    snap_batch_rename_dialog_present (paths, GTK_WINDOW (toplevel));
    g_list_free_full (paths, g_free);
}

/* View-level action entries */
static const GActionEntry snap_view_entries[] =
{
    { .name = "snap-save",      .activate = action_snap_save },
    { .name = "snap-history",   .activate = action_snap_history },
    { .name = "snap-diff",      .activate = action_snap_diff },
    { .name = "snap-restore",   .activate = action_snap_restore },
    { .name = "snap-browse",    .activate = action_snap_browse },
    { .name = "snap-branch",    .activate = action_snap_branch },
    { .name = "snap-group",     .activate = action_snap_group },
    { .name = "snap-encrypt",   .activate = action_snap_encrypt },
    { .name = "snap-stash",     .activate = action_snap_stash },
    { .name = "snap-blame",     .activate = action_snap_blame },
    { .name = "snap-audit",     .activate = action_snap_audit },
    { .name = "snap-checksum",  .activate = action_snap_checksum },
    { .name = "snap-batch-rename", .activate = action_snap_batch_rename },
};

void
snap_actions_register_view (GSimpleActionGroup *action_group, gpointer view)
{
    g_action_map_add_action_entries (G_ACTION_MAP (action_group),
                                     snap_view_entries,
                                     G_N_ELEMENTS (snap_view_entries),
                                     view);
}

/* --- Window action callbacks --- */

static void
action_snap_inspector (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    snap_inspector_panel_toggle (user_data);
}

static const GActionEntry snap_win_entries[] =
{
    { .name = "snap-inspector", .activate = action_snap_inspector },
};

void
snap_actions_register_window (GActionMap *action_map, gpointer window)
{
    g_action_map_add_action_entries (action_map,
                                     snap_win_entries,
                                     G_N_ELEMENTS (snap_win_entries),
                                     window);
}

/* --- App action callbacks --- */

static void
action_snap_log (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkApplication *app = GTK_APPLICATION (user_data);
    GtkWindow *win = gtk_application_get_active_window (app);
    snap_log_dialog_present (win);
}

static void
action_snap_sync (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkApplication *app = GTK_APPLICATION (user_data);
    GtkWindow *win = gtk_application_get_active_window (app);
    snap_sync_dialog_present (win);
}

static void
action_snap_backup (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkApplication *app = GTK_APPLICATION (user_data);
    GtkWindow *win = gtk_application_get_active_window (app);
    snap_backup_dialog_present (win);
}

static void
action_snap_config (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkApplication *app = GTK_APPLICATION (user_data);
    GtkWindow *win = gtk_application_get_active_window (app);
    snap_config_dialog_present (win);
}

static void
action_snap_gc (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkApplication *app = GTK_APPLICATION (user_data);
    GtkWindow *win = gtk_application_get_active_window (app);
    snap_gc_dialog_present (win);
}

static void
action_snap_stats (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkApplication *app = GTK_APPLICATION (user_data);
    GtkWindow *win = gtk_application_get_active_window (app);
    snap_stats_dialog_present (win);
}

static void
action_snap_hooks (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkApplication *app = GTK_APPLICATION (user_data);
    GtkWindow *win = gtk_application_get_active_window (app);
    snap_hook_dialog_present (win);
}

static void
action_snap_import_git (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkApplication *app = GTK_APPLICATION (user_data);
    GtkWindow *win = gtk_application_get_active_window (app);
    snap_import_git_dialog_present (win);
}

static void
action_snap_shortcuts (GSimpleAction *action, GVariant *param, gpointer user_data)
{
    GtkApplication *app = GTK_APPLICATION (user_data);
    GtkWindow *win = gtk_application_get_active_window (app);
    snap_shortcuts_dialog_present (win);
}

static const GActionEntry snap_app_entries[] =
{
    { .name = "snap-log",        .activate = action_snap_log },
    { .name = "snap-sync",       .activate = action_snap_sync },
    { .name = "snap-backup",     .activate = action_snap_backup },
    { .name = "snap-config",     .activate = action_snap_config },
    { .name = "snap-gc",         .activate = action_snap_gc },
    { .name = "snap-stats",      .activate = action_snap_stats },
    { .name = "snap-hooks",      .activate = action_snap_hooks },
    { .name = "snap-import-git", .activate = action_snap_import_git },
    { .name = "snap-shortcuts",  .activate = action_snap_shortcuts },
};

void
snap_actions_register_app (GApplication *app)
{
    g_action_map_add_action_entries (G_ACTION_MAP (app),
                                     snap_app_entries,
                                     G_N_ELEMENTS (snap_app_entries),
                                     app);

    /* Keyboard accelerators */
    GtkApplication *gtk_app = GTK_APPLICATION (app);
    const gchar *save_accels[] = { "<Shift><Control>s", NULL };
    const gchar *history_accels[] = { "<Shift><Control>h", NULL };
    const gchar *diff_accels[] = { "<Shift><Control>d", NULL };
    const gchar *browse_accels[] = { "<Shift><Control>b", NULL };
    const gchar *inspector_accels[] = { "<Shift><Control>i", NULL };

    gtk_application_set_accels_for_action (gtk_app, "view.snap-save", save_accels);
    gtk_application_set_accels_for_action (gtk_app, "view.snap-history", history_accels);
    gtk_application_set_accels_for_action (gtk_app, "view.snap-diff", diff_accels);
    gtk_application_set_accels_for_action (gtk_app, "view.snap-browse", browse_accels);
    gtk_application_set_accels_for_action (gtk_app, "win.snap-inspector", inspector_accels);
}
