/* snap-hook-dialog.c
 *
 * Dialog for hook install/remove/list management.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-hook-dialog.h"
#include "snap-hooks.h"
#include "snap-store.h"

struct _SnapHookDialog
{
    AdwDialog parent_instance;

    /* Widgets */
    GtkWidget *hook_list;
    GtkWidget *name_entry;
    GtkWidget *event_dropdown;
    GtkWidget *install_button;
    GtkWidget *status_label;
};

G_DEFINE_FINAL_TYPE (SnapHookDialog, snap_hook_dialog, ADW_TYPE_DIALOG)

static const gchar *hook_event_labels[] = {
    "Pre-Save",
    "Post-Save",
    "Pre-Restore",
    "Post-Restore",
    "Pre-Delete",
    "Post-Delete",
    "Pre-Branch",
    "Post-Branch",
    "Pre-Merge",
    "Post-Merge",
    NULL
};

static void
snap_hook_dialog_refresh_list (SnapHookDialog *self)
{
    g_autoptr (GError) error = NULL;
    GList *hooks;
    GtkWidget *child;

    while ((child = gtk_widget_get_first_child (self->hook_list)))
        gtk_list_box_remove (GTK_LIST_BOX (self->hook_list), child);

    hooks = snap_hooks_list (&error);
    if (error != NULL)
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), error->message);
        return;
    }

    for (GList *l = hooks; l != NULL; l = l->next)
    {
        SnapHook *hook = l->data;
        GtkWidget *row = adw_action_row_new ();
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), hook->name);
        adw_action_row_set_subtitle (ADW_ACTION_ROW (row), hook->event);

        GtkWidget *remove_btn = gtk_button_new_from_icon_name ("user-trash-symbolic");
        gtk_widget_set_valign (remove_btn, GTK_ALIGN_CENTER);
        gtk_widget_add_css_class (remove_btn, "flat");
        g_object_set_data_full (G_OBJECT (remove_btn), "hook-name",
                                g_strdup (hook->name), g_free);
        adw_action_row_add_suffix (ADW_ACTION_ROW (row), remove_btn);

        gtk_list_box_append (GTK_LIST_BOX (self->hook_list), row);
    }

    g_list_free_full (hooks, (GDestroyNotify) snap_hook_free);
}

static void
on_install_open_response (GObject      *source,
                          GAsyncResult *result,
                          gpointer      user_data)
{
    SnapHookDialog *self = SNAP_HOOK_DIALOG (user_data);
    GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
    g_autoptr (GError) error = NULL;
    g_autoptr (GFile) file = NULL;

    file = gtk_file_dialog_open_finish (dialog, result, &error);
    if (file == NULL)
        return;

    g_autofree gchar *script_path = g_file_get_path (file);
    const gchar *name = gtk_editable_get_text (GTK_EDITABLE (self->name_entry));
    guint selected = gtk_drop_down_get_selected (GTK_DROP_DOWN (self->event_dropdown));
    SnapHookEvent event = (SnapHookEvent) selected;

    snap_hooks_install (name, event, script_path, &error);
    if (error != NULL)
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), error->message);
        return;
    }

    gtk_editable_set_text (GTK_EDITABLE (self->name_entry), "");
    gtk_label_set_text (GTK_LABEL (self->status_label), "Hook installed");
    snap_hook_dialog_refresh_list (self);
}

static void
on_install_clicked (GtkButton      *button,
                    SnapHookDialog *self)
{
    const gchar *name;
    GtkFileDialog *dialog;
    GtkWindow *parent;

    name = gtk_editable_get_text (GTK_EDITABLE (self->name_entry));
    if (name == NULL || *name == '\0')
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), "Hook name is required");
        return;
    }

    dialog = gtk_file_dialog_new ();
    gtk_file_dialog_set_title (dialog, "Select Hook Script");

    parent = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));
    gtk_file_dialog_open (dialog, parent, NULL,
                          on_install_open_response, self);
}

static void
snap_hook_dialog_class_init (SnapHookDialogClass *klass)
{
}

static void
snap_hook_dialog_init (SnapHookDialog *self)
{
    GtkWidget *toolbar_view;
    GtkWidget *header_bar;
    GtkWidget *content_box;
    GtkWidget *clamp;
    GtkWidget *install_group;
    GtkWidget *list_group;
    GtkStringList *event_model;

    adw_dialog_set_title (ADW_DIALOG (self), "Hooks");
    adw_dialog_set_content_width (ADW_DIALOG (self), 450);
    adw_dialog_set_content_height (ADW_DIALOG (self), 500);

    toolbar_view = adw_toolbar_view_new ();
    header_bar = adw_header_bar_new ();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), header_bar);

    content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top (content_box, 12);
    gtk_widget_set_margin_bottom (content_box, 12);
    gtk_widget_set_margin_start (content_box, 12);
    gtk_widget_set_margin_end (content_box, 12);

    /* Install section */
    install_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (install_group), "Install Hook");

    self->name_entry = adw_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->name_entry), "Hook Name");
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (install_group), self->name_entry);

    /* Event dropdown */
    event_model = gtk_string_list_new (hook_event_labels);
    self->event_dropdown = gtk_drop_down_new (G_LIST_MODEL (event_model), NULL);
    gtk_widget_set_valign (self->event_dropdown, GTK_ALIGN_CENTER);

    GtkWidget *event_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (event_row), "Event");
    adw_action_row_add_suffix (ADW_ACTION_ROW (event_row), self->event_dropdown);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (install_group), event_row);

    gtk_box_append (GTK_BOX (content_box), install_group);

    self->install_button = gtk_button_new_with_label ("Install Script...");
    gtk_widget_add_css_class (self->install_button, "suggested-action");
    gtk_widget_add_css_class (self->install_button, "pill");
    gtk_widget_set_halign (self->install_button, GTK_ALIGN_CENTER);
    g_signal_connect (self->install_button, "clicked", G_CALLBACK (on_install_clicked), self);
    gtk_box_append (GTK_BOX (content_box), self->install_button);

    /* Hook list */
    list_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (list_group), "Installed Hooks");

    self->hook_list = gtk_list_box_new ();
    gtk_widget_add_css_class (self->hook_list, "boxed-list");
    gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->hook_list), GTK_SELECTION_NONE);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (list_group), self->hook_list);
    gtk_box_append (GTK_BOX (content_box), list_group);

    /* Status label */
    self->status_label = gtk_label_new ("");
    gtk_widget_add_css_class (self->status_label, "dim-label");
    gtk_box_append (GTK_BOX (content_box), self->status_label);

    clamp = adw_clamp_new ();
    adw_clamp_set_child (ADW_CLAMP (clamp), content_box);

    GtkWidget *scrolled = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), clamp);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view), scrolled);
    adw_dialog_set_child (ADW_DIALOG (self), toolbar_view);
}

void
snap_hook_dialog_present (GtkWindow *parent)
{
    SnapHookDialog *self;

    self = g_object_new (SNAP_TYPE_HOOK_DIALOG, NULL);

    snap_hook_dialog_refresh_list (self);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
}
