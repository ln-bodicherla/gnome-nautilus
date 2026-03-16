/* snap-hash.c
 *
 * SHA256 hashing via OpenSSL EVP.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-hash.h"
#include <openssl/evp.h>
#include <string.h>

gchar *
snap_hash_data (const guchar *data, gsize length)
{
    g_return_val_if_fail (data != NULL, NULL);

    EVP_MD_CTX *ctx = EVP_MD_CTX_new ();
    if (ctx == NULL)
        return NULL;

    if (EVP_DigestInit_ex (ctx, EVP_sha256 (), NULL) != 1)
    {
        EVP_MD_CTX_free (ctx);
        return NULL;
    }

    if (EVP_DigestUpdate (ctx, data, length) != 1)
    {
        EVP_MD_CTX_free (ctx);
        return NULL;
    }

    guchar hash[EVP_MAX_MD_SIZE];
    guint hash_len;
    if (EVP_DigestFinal_ex (ctx, hash, &hash_len) != 1)
    {
        EVP_MD_CTX_free (ctx);
        return NULL;
    }

    EVP_MD_CTX_free (ctx);

    /* Convert to hex string */
    GString *hex = g_string_sized_new (hash_len * 2);
    for (guint i = 0; i < hash_len; i++)
        g_string_append_printf (hex, "%02x", hash[i]);

    return g_string_free (hex, FALSE);
}

gchar *
snap_hash_file (const gchar *file_path, GError **error)
{
    g_return_val_if_fail (file_path != NULL, NULL);

#ifdef HAVE_SNAPCORE
    /* Use Rust FFI if available */
    gchar *result = snapcore_hash_file (file_path);
    if (result != NULL)
        return result;
#endif

    gchar *content = NULL;
    gsize length;
    if (!g_file_get_contents (file_path, &content, &length, error))
        return NULL;

    gchar *hash = snap_hash_data ((const guchar *) content, length);
    g_free (content);

    return hash;
}

gchar *
snap_hash_string (const gchar *text)
{
    g_return_val_if_fail (text != NULL, NULL);
    return snap_hash_data ((const guchar *) text, strlen (text));
}

gboolean
snap_hash_verify (const gchar *hash, const guchar *data, gsize length)
{
    g_return_val_if_fail (hash != NULL, FALSE);
    g_return_val_if_fail (data != NULL, FALSE);

    g_autofree gchar *computed = snap_hash_data (data, length);
    return g_strcmp0 (hash, computed) == 0;
}
