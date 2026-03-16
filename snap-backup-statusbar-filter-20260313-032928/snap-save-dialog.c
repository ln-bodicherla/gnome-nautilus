/* snap-save-dialog.c
 *
 * Dialog to save a new snapshot with a message.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib/gi18n.h>

#include "snap-save-dialog.h"
#include "snap-manager.h"

struct _SnapSaveDialog
{
    AdwDialog parent_instance;

    GtkWidget *message_entry;
    gchar     *file_path;
};

G_DEFINE_FINAL_TYPE (SnapSaveDialog, snap_save_dialog, ADW_TYPE_DIALOG)

static void
on_cancel_clicked (SnapSaveDialog *self)
{
    adw_dialog_close (ADW_DIALOG (self));
}

static void
on_save_clicked (SnapSaveDialog *self)
{
    const gchar *message;
    g_autoptr (GError) error = NULL;
    SnapManager *manager;

    message = gtk_editable_get_text (GTK_EDITABLE (self->message_entry));
    manager = snap_manager_get_default ();

    snap_manager_save (manager, self->file_path, message, &error);

    if (error != NULL)
    {
        g_warning ("Failed to save snapshot: %s", error->message);
    }

    adw_dialog_close (ADW_DIALOG (self));
}

static void
snap_save_dialog_dispose (GObject *object)
{
    SnapSaveDialog *self = SNAP_SAVE_DIALOG (object);

    g_clear_pointer (&self->file_path, g_free);

    G_OBJECT_CLASS (snap_save_dialog_parent_class)->dispose (object);
}

static void
snap_save_dialog_finalize (GObject *object)
{
    G_OBJECT_CLASS (snap_save_dialog_parent_class)->finalize (object);
}

static void
snap_save_dialog_init (SnapSaveDialog *self)
{
    GtkWidget *content;
    GtkWidget *header_bar;
    GtkWidget *cancel_button;
    GtkWidget *save_button;
    GtkWidget *box;
    GtkWidget *label;

    adw_dialog_set_title (ADW_DIALOG (self), _("Save Snapshot"));
    adw_dialog_set_content_width (ADW_DIALOG (self), 400);
    adw_dialog_set_content_height (ADW_DIALOG (self), 200);

    content = adw_toolbar_view_new ();

    header_bar = adw_header_bar_new ();

    cancel_button = gtk_button_new_with_label (_("Cancel"));
    g_signal_connect_swapped (cancel_button, "clicked",
                              G_CALLBACK (on_cancel_clicked), self);
    adw_header_bar_pack_start (ADW_HEADER_BAR (header_bar), cancel_button);

    save_button = gtk_button_new_with_label (_("Save"));
    gtk_widget_add_css_class (save_button, "suggested-action");
    g_signal_connect_swapped (save_button, "clicked",
                              G_CALLBACK (on_save_clicked), self);
    adw_header_bar_pack_end (ADW_HEADER_BAR (header_bar), save_button);

    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (content), header_bar);

    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top (box, 24);
    gtk_widget_set_margin_bottom (box, 24);
    gtk_widget_set_margin_start (box, 24);
    gtk_widget_set_margin_end (box, 24);

    label = gtk_label_new (_("Enter a message for this snapshot:"));
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_box_append (GTK_BOX (box), label);

    self->message_entry = gtk_entry_new ();
    gtk_entry_set_placeholder_text (GTK_ENTRY (self->message_entry),
                                    _("Snapshot message…"));
    gtk_entry_set_activates_default (GTK_ENTRY (self->message_entry), TRUE);
    gtk_box_append (GTK_BOX (box), self->message_entry);

    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (content), box);
    adw_dialog_set_child (ADW_DIALOG (self), content);
}

static void
snap_save_dialog_class_init (SnapSaveDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = snap_save_dialog_dispose;
    object_class->finalize = snap_save_dialog_finalize;
}

void
snap_save_dialog_present (const gchar *file_path,
                          GtkWindow   *parent)
{
    SnapSaveDialog *self;

    g_return_if_fail (file_path != NULL);

    self = g_object_new (SNAP_TYPE_SAVE_DIALOG, NULL);
    self->file_path = g_strdup (file_path);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
}
