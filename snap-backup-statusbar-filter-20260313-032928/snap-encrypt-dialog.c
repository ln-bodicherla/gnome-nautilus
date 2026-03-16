/* snap-encrypt-dialog.c
 *
 * Dialog for passphrase entry and encryption toggle.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-encrypt-dialog.h"
#include "snap-manager.h"

struct _SnapEncryptDialog
{
    AdwDialog parent_instance;

    gchar *file_path;

    /* Widgets */
    GtkWidget *passphrase_row;
    GtkWidget *confirm_row;
    GtkWidget *encrypt_button;
    GtkWidget *decrypt_button;
    GtkWidget *status_label;
};

G_DEFINE_FINAL_TYPE (SnapEncryptDialog, snap_encrypt_dialog, ADW_TYPE_DIALOG)

static void
on_encrypt_clicked (GtkButton         *button,
                    SnapEncryptDialog *self)
{
    SnapManager *manager = snap_manager_get_default ();
    g_autoptr (GError) error = NULL;
    const gchar *passphrase;
    const gchar *confirm;

    passphrase = gtk_editable_get_text (GTK_EDITABLE (self->passphrase_row));
    confirm = gtk_editable_get_text (GTK_EDITABLE (self->confirm_row));

    if (passphrase == NULL || *passphrase == '\0')
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), "Passphrase cannot be empty");
        return;
    }

    if (g_strcmp0 (passphrase, confirm) != 0)
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), "Passphrases do not match");
        return;
    }

    snap_manager_encrypt (manager, self->file_path, passphrase, &error);
    if (error != NULL)
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), error->message);
        return;
    }

    gtk_label_set_text (GTK_LABEL (self->status_label), "Encryption enabled");
}

static void
on_decrypt_clicked (GtkButton         *button,
                    SnapEncryptDialog *self)
{
    SnapManager *manager = snap_manager_get_default ();
    g_autoptr (GError) error = NULL;
    const gchar *passphrase;

    passphrase = gtk_editable_get_text (GTK_EDITABLE (self->passphrase_row));

    if (passphrase == NULL || *passphrase == '\0')
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), "Passphrase is required");
        return;
    }

    snap_manager_decrypt (manager, self->file_path, passphrase, &error);
    if (error != NULL)
    {
        gtk_label_set_text (GTK_LABEL (self->status_label), error->message);
        return;
    }

    gtk_label_set_text (GTK_LABEL (self->status_label), "Decryption successful");
}

static void
snap_encrypt_dialog_finalize (GObject *object)
{
    SnapEncryptDialog *self = SNAP_ENCRYPT_DIALOG (object);

    g_free (self->file_path);

    G_OBJECT_CLASS (snap_encrypt_dialog_parent_class)->finalize (object);
}

static void
snap_encrypt_dialog_class_init (SnapEncryptDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = snap_encrypt_dialog_finalize;
}

static void
snap_encrypt_dialog_init (SnapEncryptDialog *self)
{
    GtkWidget *toolbar_view;
    GtkWidget *header_bar;
    GtkWidget *content_box;
    GtkWidget *clamp;
    GtkWidget *passphrase_group;
    GtkWidget *button_box;

    adw_dialog_set_title (ADW_DIALOG (self), "Encryption");
    adw_dialog_set_content_width (ADW_DIALOG (self), 400);
    adw_dialog_set_content_height (ADW_DIALOG (self), 350);

    toolbar_view = adw_toolbar_view_new ();
    header_bar = adw_header_bar_new ();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), header_bar);

    content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top (content_box, 12);
    gtk_widget_set_margin_bottom (content_box, 12);
    gtk_widget_set_margin_start (content_box, 12);
    gtk_widget_set_margin_end (content_box, 12);

    /* Passphrase entry */
    passphrase_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (passphrase_group), "Passphrase");

    self->passphrase_row = adw_password_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->passphrase_row), "Passphrase");
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (passphrase_group), self->passphrase_row);

    self->confirm_row = adw_password_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->confirm_row), "Confirm Passphrase");
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (passphrase_group), self->confirm_row);

    gtk_box_append (GTK_BOX (content_box), passphrase_group);

    /* Buttons */
    button_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign (button_box, GTK_ALIGN_CENTER);

    self->encrypt_button = gtk_button_new_with_label ("Encrypt");
    gtk_widget_add_css_class (self->encrypt_button, "suggested-action");
    gtk_widget_add_css_class (self->encrypt_button, "pill");
    g_signal_connect (self->encrypt_button, "clicked", G_CALLBACK (on_encrypt_clicked), self);
    gtk_box_append (GTK_BOX (button_box), self->encrypt_button);

    self->decrypt_button = gtk_button_new_with_label ("Decrypt");
    gtk_widget_add_css_class (self->decrypt_button, "pill");
    g_signal_connect (self->decrypt_button, "clicked", G_CALLBACK (on_decrypt_clicked), self);
    gtk_box_append (GTK_BOX (button_box), self->decrypt_button);

    gtk_box_append (GTK_BOX (content_box), button_box);

    /* Status label */
    self->status_label = gtk_label_new ("");
    gtk_widget_add_css_class (self->status_label, "dim-label");
    gtk_box_append (GTK_BOX (content_box), self->status_label);

    clamp = adw_clamp_new ();
    adw_clamp_set_child (ADW_CLAMP (clamp), content_box);

    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view), clamp);
    adw_dialog_set_child (ADW_DIALOG (self), toolbar_view);
}

void
snap_encrypt_dialog_present (const gchar *file_path,
                             GtkWindow   *parent)
{
    SnapEncryptDialog *self;

    g_return_if_fail (file_path != NULL);

    self = g_object_new (SNAP_TYPE_ENCRYPT_DIALOG, NULL);
    self->file_path = g_strdup (file_path);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
}
