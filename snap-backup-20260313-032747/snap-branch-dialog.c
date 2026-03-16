/* snap-branch-dialog.c
 *
 * Dialog for branch create/list/delete/merge operations.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-branch-dialog.h"
#include "snap-manager.h"

struct _SnapBranchDialog
{
    AdwDialog parent_instance;

    gchar *file_path;

    /* Widgets */
    GtkWidget *stack;
    GtkWidget *branch_list;
    GtkWidget *create_entry;
    GtkWidget *merge_source_entry;
    GtkWidget *merge_target_entry;
    GtkWidget *status_label;
};

G_DEFINE_FINAL_TYPE (SnapBranchDialog, snap_branch_dialog, ADW_TYPE_DIALOG)

static void
snap_branch_dialog_refresh_list (SnapBranchDialog *self)
{
    SnapManager *manager = snap_manager_get_default ();
    g_autoptr (GError) error = NULL;
    GList *branches;
    GList *l;
    GtkWidget *child;

    /* Clear existing rows */
    while ((child = gtk_widget_get_first_child (self->branch_list)))
        gtk_list_box_remove (GTK_LIST_BOX (self->branch_list), child);

    branches = snap_manager_list_branches (manager, self->file_path, &error);
    if (error != NULL)
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), error->message);
        return;
    }

    for (l = branches; l != NULL; l = l->next)
    {
        SnapBranch *branch = l->data;
        GtkWidget *row = adw_action_row_new ();
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), branch->name);

        g_autofree gchar *subtitle = g_strdup_printf ("Forked at version %" G_GINT64_FORMAT, branch->fork_version);
        adw_action_row_set_subtitle (ADW_ACTION_ROW (row), subtitle);

        GtkWidget *delete_btn = gtk_button_new_from_icon_name ("user-trash-symbolic");
        gtk_widget_set_valign (delete_btn, GTK_ALIGN_CENTER);
        gtk_widget_add_css_class (delete_btn, "flat");
        g_object_set_data_full (G_OBJECT (delete_btn), "branch-name",
                                g_strdup (branch->name), g_free);
        adw_action_row_add_suffix (ADW_ACTION_ROW (row), delete_btn);

        gtk_list_box_append (GTK_LIST_BOX (self->branch_list), row);
    }

    g_list_free_full (branches, (GDestroyNotify) snap_branch_free);
}

static void
on_create_clicked (GtkButton        *button,
                   SnapBranchDialog *self)
{
    SnapManager *manager = snap_manager_get_default ();
    g_autoptr (GError) error = NULL;
    const gchar *name;

    name = gtk_editable_get_text (GTK_EDITABLE (self->create_entry));
    if (name == NULL || *name == '\0')
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), "Branch name cannot be empty");
        return;
    }

    snap_manager_create_branch (manager, self->file_path, name, &error);
    if (error != NULL)
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), error->message);
        return;
    }

    gtk_editable_set_text (GTK_EDITABLE (self->create_entry), "");
    gtk_label_set_text (GTK_LABEL (self->status_label), "Branch created");
    snap_branch_dialog_refresh_list (self);
}

static void
on_merge_clicked (GtkButton        *button,
                  SnapBranchDialog *self)
{
    SnapManager *manager = snap_manager_get_default ();
    g_autoptr (GError) error = NULL;
    const gchar *source;
    const gchar *target;

    source = gtk_editable_get_text (GTK_EDITABLE (self->merge_source_entry));
    target = gtk_editable_get_text (GTK_EDITABLE (self->merge_target_entry));

    if ((source == NULL || *source == '\0') || (target == NULL || *target == '\0'))
    {
        gtk_label_set_text (GTK_LABEL (self->status_label),
                            "Source and target branch names are required");
        return;
    }

    snap_manager_merge_branch (manager, self->file_path, source, target, &error);
    if (error != NULL)
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), error->message);
        return;
    }

    gtk_label_set_text (GTK_LABEL (self->status_label), "Branches merged");
    snap_branch_dialog_refresh_list (self);
}

static void
snap_branch_dialog_finalize (GObject *object)
{
    SnapBranchDialog *self = SNAP_BRANCH_DIALOG (object);

    g_free (self->file_path);

    G_OBJECT_CLASS (snap_branch_dialog_parent_class)->finalize (object);
}

static void
snap_branch_dialog_class_init (SnapBranchDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = snap_branch_dialog_finalize;
}

static void
snap_branch_dialog_init (SnapBranchDialog *self)
{
    GtkWidget *toolbar_view;
    GtkWidget *header_bar;
    GtkWidget *content_box;
    GtkWidget *clamp;
    GtkWidget *create_box;
    GtkWidget *create_button;
    GtkWidget *merge_group;
    GtkWidget *merge_button;

    adw_dialog_set_title (ADW_DIALOG (self), "Branches");
    adw_dialog_set_content_width (ADW_DIALOG (self), 400);
    adw_dialog_set_content_height (ADW_DIALOG (self), 500);

    toolbar_view = adw_toolbar_view_new ();
    header_bar = adw_header_bar_new ();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), header_bar);

    content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top (content_box, 12);
    gtk_widget_set_margin_bottom (content_box, 12);
    gtk_widget_set_margin_start (content_box, 12);
    gtk_widget_set_margin_end (content_box, 12);

    /* Create branch section */
    create_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    self->create_entry = gtk_entry_new ();
    gtk_entry_set_placeholder_text (GTK_ENTRY (self->create_entry), "New branch name");
    gtk_widget_set_hexpand (self->create_entry, TRUE);
    gtk_box_append (GTK_BOX (create_box), self->create_entry);

    create_button = gtk_button_new_with_label ("Create");
    gtk_widget_add_css_class (create_button, "suggested-action");
    g_signal_connect (create_button, "clicked", G_CALLBACK (on_create_clicked), self);
    gtk_box_append (GTK_BOX (create_box), create_button);
    gtk_box_append (GTK_BOX (content_box), create_box);

    /* Branch list */
    self->branch_list = gtk_list_box_new ();
    gtk_widget_add_css_class (self->branch_list, "boxed-list");
    gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->branch_list), GTK_SELECTION_NONE);
    gtk_box_append (GTK_BOX (content_box), self->branch_list);

    /* Merge section */
    merge_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (merge_group), "Merge Branches");

    self->merge_source_entry = adw_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->merge_source_entry), "Source");
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (merge_group), self->merge_source_entry);

    self->merge_target_entry = adw_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->merge_target_entry), "Target");
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (merge_group), self->merge_target_entry);

    merge_button = gtk_button_new_with_label ("Merge");
    gtk_widget_add_css_class (merge_button, "suggested-action");
    gtk_widget_set_halign (merge_button, GTK_ALIGN_END);
    gtk_widget_set_margin_top (merge_button, 6);
    g_signal_connect (merge_button, "clicked", G_CALLBACK (on_merge_clicked), self);

    gtk_box_append (GTK_BOX (content_box), merge_group);
    gtk_box_append (GTK_BOX (content_box), merge_button);

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
snap_branch_dialog_present (const gchar *file_path,
                            GtkWindow   *parent)
{
    SnapBranchDialog *self;

    g_return_if_fail (file_path != NULL);

    self = g_object_new (SNAP_TYPE_BRANCH_DIALOG, NULL);
    self->file_path = g_strdup (file_path);

    snap_branch_dialog_refresh_list (self);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
}
