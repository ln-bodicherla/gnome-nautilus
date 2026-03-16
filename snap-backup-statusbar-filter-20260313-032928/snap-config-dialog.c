/* snap-config-dialog.c
 *
 * Preferences dialog with all snap config options.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-config-dialog.h"
#include "snap-config.h"

struct _SnapConfigDialog
{
    AdwPreferencesDialog parent_instance;
};

G_DEFINE_FINAL_TYPE (SnapConfigDialog, snap_config_dialog, ADW_TYPE_PREFERENCES_DIALOG)

static void
snap_config_dialog_class_init (SnapConfigDialogClass *klass)
{
}

static void
snap_config_dialog_init (SnapConfigDialog *self)
{
}

static void
on_auto_save_toggled (GObject *obj, GParamSpec *pspec, gpointer user_data)
{
    SnapConfig *config = snap_config_get_default ();
    gboolean active = adw_switch_row_get_active (ADW_SWITCH_ROW (obj));
    snap_config_set_auto_save (config, active);
    snap_config_save (config, NULL);
}

static void
on_compression_toggled (GObject *obj, GParamSpec *pspec, gpointer user_data)
{
    SnapConfig *config = snap_config_get_default ();
    gboolean active = adw_switch_row_get_active (ADW_SWITCH_ROW (obj));
    snap_config_set_compression (config, active);
    snap_config_save (config, NULL);
}

static void
on_encryption_toggled (GObject *obj, GParamSpec *pspec, gpointer user_data)
{
    SnapConfig *config = snap_config_get_default ();
    gboolean active = adw_switch_row_get_active (ADW_SWITCH_ROW (obj));
    snap_config_set_encryption (config, active);
    snap_config_save (config, NULL);
}

static void
on_notifications_toggled (GObject *obj, GParamSpec *pspec, gpointer user_data)
{
    SnapConfig *config = snap_config_get_default ();
    gboolean active = adw_switch_row_get_active (ADW_SWITCH_ROW (obj));
    snap_config_set_notifications (config, active);
    snap_config_save (config, NULL);
}

static void
on_auto_watch_toggled (GObject *obj, GParamSpec *pspec, gpointer user_data)
{
    SnapConfig *config = snap_config_get_default ();
    gboolean active = adw_switch_row_get_active (ADW_SWITCH_ROW (obj));
    snap_config_set_auto_watch (config, active);
    snap_config_save (config, NULL);
}

void
snap_config_dialog_present (GtkWindow *parent)
{
    SnapConfigDialog *self = g_object_new (SNAP_TYPE_CONFIG_DIALOG, NULL);
    SnapConfig *config = snap_config_get_default ();

    /* General page */
    AdwPreferencesPage *general = ADW_PREFERENCES_PAGE (adw_preferences_page_new ());
    adw_preferences_page_set_title (general, "General");
    adw_preferences_page_set_icon_name (general, "preferences-system-symbolic");

    AdwPreferencesGroup *save_group = ADW_PREFERENCES_GROUP (adw_preferences_group_new ());
    adw_preferences_group_set_title (save_group, "Saving");

    GtkWidget *auto_save_row = adw_switch_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (auto_save_row), "Auto Save");
    adw_action_row_set_subtitle (ADW_ACTION_ROW (auto_save_row), "Automatically save snapshots on file change");
    adw_switch_row_set_active (ADW_SWITCH_ROW (auto_save_row), snap_config_get_auto_save (config));
    g_signal_connect (auto_save_row, "notify::active", G_CALLBACK (on_auto_save_toggled), NULL);
    adw_preferences_group_add (save_group, auto_save_row);

    GtkWidget *compression_row = adw_switch_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (compression_row), "Compression");
    adw_action_row_set_subtitle (ADW_ACTION_ROW (compression_row), "Compress snapshot data with zlib");
    adw_switch_row_set_active (ADW_SWITCH_ROW (compression_row), snap_config_get_compression (config));
    g_signal_connect (compression_row, "notify::active", G_CALLBACK (on_compression_toggled), NULL);
    adw_preferences_group_add (save_group, compression_row);

    GtkWidget *encryption_row = adw_switch_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (encryption_row), "Encryption");
    adw_action_row_set_subtitle (ADW_ACTION_ROW (encryption_row), "Encrypt snapshots with AES-256-GCM");
    adw_switch_row_set_active (ADW_SWITCH_ROW (encryption_row), snap_config_get_encryption (config));
    g_signal_connect (encryption_row, "notify::active", G_CALLBACK (on_encryption_toggled), NULL);
    adw_preferences_group_add (save_group, encryption_row);

    adw_preferences_page_add (general, save_group);

    AdwPreferencesGroup *notify_group = ADW_PREFERENCES_GROUP (adw_preferences_group_new ());
    adw_preferences_group_set_title (notify_group, "Notifications");

    GtkWidget *notify_row = adw_switch_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (notify_row), "Notifications");
    adw_action_row_set_subtitle (ADW_ACTION_ROW (notify_row), "Show notifications for snap operations");
    adw_switch_row_set_active (ADW_SWITCH_ROW (notify_row), snap_config_get_notifications (config));
    g_signal_connect (notify_row, "notify::active", G_CALLBACK (on_notifications_toggled), NULL);
    adw_preferences_group_add (notify_group, notify_row);

    GtkWidget *watch_row = adw_switch_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (watch_row), "Auto Watch");
    adw_action_row_set_subtitle (ADW_ACTION_ROW (watch_row), "Automatically watch tracked files for changes");
    adw_switch_row_set_active (ADW_SWITCH_ROW (watch_row), snap_config_get_auto_watch (config));
    g_signal_connect (watch_row, "notify::active", G_CALLBACK (on_auto_watch_toggled), NULL);
    adw_preferences_group_add (notify_group, watch_row);

    adw_preferences_page_add (general, notify_group);
    adw_preferences_dialog_add (ADW_PREFERENCES_DIALOG (self), general);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
}
