/* snap-checksum-dialog.c
 *
 * File checksum verification dialog.
 * Computes MD5, SHA1, SHA256, SHA512 and allows
 * comparing against a user-provided hash.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-checksum-dialog.h"
#include <gio/gio.h>

struct _SnapChecksumDialog
{
    AdwDialog parent_instance;

    gchar *file_path;

    GtkWidget *filename_label;
    GtkWidget *md5_row;
    GtkWidget *sha1_row;
    GtkWidget *sha256_row;
    GtkWidget *sha512_row;
    GtkWidget *verify_entry;
    GtkWidget *verify_result;
    GtkWidget *spinner;

    gchar *md5_hash;
    gchar *sha1_hash;
    gchar *sha256_hash;
    gchar *sha512_hash;
};

G_DEFINE_FINAL_TYPE (SnapChecksumDialog, snap_checksum_dialog, ADW_TYPE_DIALOG)

typedef struct
{
    gchar *md5;
    gchar *sha1;
    gchar *sha256;
    gchar *sha512;
} ChecksumResult;

static void
checksum_result_free (ChecksumResult *result)
{
    g_free (result->md5);
    g_free (result->sha1);
    g_free (result->sha256);
    g_free (result->sha512);
    g_free (result);
}

static void
compute_checksums_thread (GTask        *task,
                          gpointer      source_object,
                          gpointer      task_data,
                          GCancellable *cancellable)
{
    const gchar *path = task_data;
    g_autofree gchar *contents = NULL;
    gsize length;

    GFile *file = g_file_new_for_path (path);
    if (!g_file_load_contents (file, cancellable, &contents, &length, NULL, NULL))
    {
        g_object_unref (file);
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                                "Failed to read file");
        return;
    }
    g_object_unref (file);

    ChecksumResult *result = g_new0 (ChecksumResult, 1);
    result->md5 = g_compute_checksum_for_data (G_CHECKSUM_MD5, (guchar *) contents, length);
    result->sha1 = g_compute_checksum_for_data (G_CHECKSUM_SHA1, (guchar *) contents, length);
    result->sha256 = g_compute_checksum_for_data (G_CHECKSUM_SHA256, (guchar *) contents, length);
    result->sha512 = g_compute_checksum_for_data (G_CHECKSUM_SHA512, (guchar *) contents, length);

    g_task_return_pointer (task, result, (GDestroyNotify) checksum_result_free);
}

static void
on_verify_changed (GtkEditable *editable, gpointer user_data)
{
    SnapChecksumDialog *self = SNAP_CHECKSUM_DIALOG (user_data);
    const gchar *input = gtk_editable_get_text (editable);

    if (input == NULL || *input == '\0')
    {
        gtk_label_set_text (GTK_LABEL (self->verify_result), "");
        return;
    }

    /* Normalize input */
    g_autofree gchar *lower = g_ascii_strdown (input, -1);
    g_strstrip (lower);

    const gchar *match = NULL;
    if (self->md5_hash && g_strcmp0 (lower, self->md5_hash) == 0)
        match = "MD5";
    else if (self->sha1_hash && g_strcmp0 (lower, self->sha1_hash) == 0)
        match = "SHA1";
    else if (self->sha256_hash && g_strcmp0 (lower, self->sha256_hash) == 0)
        match = "SHA256";
    else if (self->sha512_hash && g_strcmp0 (lower, self->sha512_hash) == 0)
        match = "SHA512";

    if (match != NULL)
    {
        g_autofree gchar *msg = g_strdup_printf ("Match: %s", match);
        gtk_label_set_text (GTK_LABEL (self->verify_result), msg);
        gtk_widget_add_css_class (self->verify_result, "success");
        gtk_widget_remove_css_class (self->verify_result, "error");
    }
    else
    {
        gtk_label_set_text (GTK_LABEL (self->verify_result), "No match");
        gtk_widget_add_css_class (self->verify_result, "error");
        gtk_widget_remove_css_class (self->verify_result, "success");
    }
}

static void
checksums_ready (GObject      *source,
                 GAsyncResult *result,
                 gpointer      user_data)
{
    SnapChecksumDialog *self = SNAP_CHECKSUM_DIALOG (user_data);

    gtk_widget_set_visible (self->spinner, FALSE);

    ChecksumResult *res = g_task_propagate_pointer (G_TASK (result), NULL);
    if (res == NULL)
        return;

    self->md5_hash = g_strdup (res->md5);
    self->sha1_hash = g_strdup (res->sha1);
    self->sha256_hash = g_strdup (res->sha256);
    self->sha512_hash = g_strdup (res->sha512);

    adw_action_row_set_subtitle (ADW_ACTION_ROW (self->md5_row), res->md5);
    adw_action_row_set_subtitle (ADW_ACTION_ROW (self->sha1_row), res->sha1);
    adw_action_row_set_subtitle (ADW_ACTION_ROW (self->sha256_row), res->sha256);
    adw_action_row_set_subtitle (ADW_ACTION_ROW (self->sha512_row), res->sha512);

    checksum_result_free (res);
}

static GtkWidget *
make_hash_row (const gchar *title)
{
    GtkWidget *row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);
    adw_action_row_set_subtitle_selectable (ADW_ACTION_ROW (row), TRUE);
    adw_action_row_set_subtitle (ADW_ACTION_ROW (row), "Computing...");
    gtk_widget_add_css_class (row, "monospace");
    return row;
}

static void
snap_checksum_dialog_finalize (GObject *object)
{
    SnapChecksumDialog *self = SNAP_CHECKSUM_DIALOG (object);
    g_free (self->file_path);
    g_free (self->md5_hash);
    g_free (self->sha1_hash);
    g_free (self->sha256_hash);
    g_free (self->sha512_hash);
    G_OBJECT_CLASS (snap_checksum_dialog_parent_class)->finalize (object);
}

static void
snap_checksum_dialog_class_init (SnapChecksumDialogClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = snap_checksum_dialog_finalize;
}

static void
snap_checksum_dialog_init (SnapChecksumDialog *self)
{
    GtkWidget *toolbar_view;
    GtkWidget *header_bar;
    GtkWidget *scrolled;
    GtkWidget *clamp;
    GtkWidget *content_box;
    GtkWidget *hash_group;
    GtkWidget *verify_group;

    adw_dialog_set_title (ADW_DIALOG (self), "File Checksums");
    adw_dialog_set_content_width (ADW_DIALOG (self), 550);
    adw_dialog_set_content_height (ADW_DIALOG (self), 500);

    toolbar_view = adw_toolbar_view_new ();
    header_bar = adw_header_bar_new ();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), header_bar);

    content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 18);
    gtk_widget_set_margin_top (content_box, 12);
    gtk_widget_set_margin_bottom (content_box, 24);
    gtk_widget_set_margin_start (content_box, 12);
    gtk_widget_set_margin_end (content_box, 12);

    /* File info */
    self->filename_label = gtk_label_new ("");
    gtk_widget_add_css_class (self->filename_label, "title-4");
    gtk_label_set_ellipsize (GTK_LABEL (self->filename_label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_box_append (GTK_BOX (content_box), self->filename_label);

    self->spinner = gtk_spinner_new ();
    gtk_widget_set_visible (self->spinner, FALSE);
    gtk_box_append (GTK_BOX (content_box), self->spinner);

    /* Hash group */
    hash_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (hash_group), "Checksums");

    self->md5_row = make_hash_row ("MD5");
    self->sha1_row = make_hash_row ("SHA1");
    self->sha256_row = make_hash_row ("SHA256");
    self->sha512_row = make_hash_row ("SHA512");

    adw_preferences_group_add (ADW_PREFERENCES_GROUP (hash_group), self->md5_row);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (hash_group), self->sha1_row);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (hash_group), self->sha256_row);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (hash_group), self->sha512_row);

    gtk_box_append (GTK_BOX (content_box), hash_group);

    /* Verify group */
    verify_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (verify_group), "Verify");
    adw_preferences_group_set_description (ADW_PREFERENCES_GROUP (verify_group),
        "Paste a hash to compare against the computed checksums");

    self->verify_entry = gtk_entry_new ();
    gtk_entry_set_placeholder_text (GTK_ENTRY (self->verify_entry), "Paste hash here...");
    gtk_widget_add_css_class (self->verify_entry, "monospace");
    g_signal_connect (self->verify_entry, "changed", G_CALLBACK (on_verify_changed), self);

    GtkWidget *entry_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (entry_row), "Hash");
    adw_action_row_add_suffix (ADW_ACTION_ROW (entry_row), self->verify_entry);

    self->verify_result = gtk_label_new ("");
    gtk_widget_add_css_class (self->verify_result, "caption");

    GtkWidget *result_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (result_row), "Result");
    adw_action_row_add_suffix (ADW_ACTION_ROW (result_row), self->verify_result);

    adw_preferences_group_add (ADW_PREFERENCES_GROUP (verify_group), entry_row);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (verify_group), result_row);

    gtk_box_append (GTK_BOX (content_box), verify_group);

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
snap_checksum_dialog_present (const gchar *path, GtkWindow *parent)
{
    SnapChecksumDialog *self = g_object_new (SNAP_TYPE_CHECKSUM_DIALOG, NULL);

    self->file_path = g_strdup (path);

    g_autofree gchar *basename = g_path_get_basename (path);
    gtk_label_set_text (GTK_LABEL (self->filename_label), basename);

    gtk_widget_set_visible (self->spinner, TRUE);
    gtk_spinner_start (GTK_SPINNER (self->spinner));

    /* Compute checksums in background */
    GTask *task = g_task_new (self, NULL, checksums_ready, self);
    g_task_set_task_data (task, g_strdup (path), g_free);
    g_task_run_in_thread (task, compute_checksums_thread);
    g_object_unref (task);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent));
}
