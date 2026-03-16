/* snap-encryption.c
 *
 * AES-256-GCM encryption via OpenSSL.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-encryption.h"
#include <gio/gio.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <string.h>

/* Magic bytes to identify encrypted data */
static const guchar SNAP_ENCRYPT_MAGIC[] = { 'S', 'N', 'A', 'P', 'E', 'N', 'C', '1' };
#define SNAP_MAGIC_LEN 8
#define SNAP_IV_LEN 12
#define SNAP_TAG_LEN 16
#define SNAP_SALT_LEN 32
#define SNAP_KEY_LEN 32

guchar *
snap_derive_key (const gchar  *passphrase,
                 const guchar *salt,
                 gsize         salt_len,
                 gsize         key_len)
{
    g_return_val_if_fail (passphrase != NULL, NULL);
    g_return_val_if_fail (salt != NULL, NULL);

    guchar *key = g_malloc (key_len);

    EVP_KDF *kdf = EVP_KDF_fetch (NULL, "HKDF", NULL);
    if (kdf == NULL)
    {
        g_free (key);
        return NULL;
    }

    EVP_KDF_CTX *ctx = EVP_KDF_CTX_new (kdf);
    EVP_KDF_free (kdf);

    if (ctx == NULL)
    {
        g_free (key);
        return NULL;
    }

    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string ("digest", (char *) "SHA256", 0),
        OSSL_PARAM_construct_octet_string ("key", (void *) passphrase,
                                           strlen (passphrase)),
        OSSL_PARAM_construct_octet_string ("salt", (void *) salt, salt_len),
        OSSL_PARAM_construct_octet_string ("info", (void *) "snap-encryption", 15),
        OSSL_PARAM_construct_end ()
    };

    int rc = EVP_KDF_derive (ctx, key, key_len, params);
    EVP_KDF_CTX_free (ctx);

    if (rc != 1)
    {
        g_free (key);
        return NULL;
    }

    return key;
}

guchar *
snap_encrypt (const guchar *plaintext,
              gsize         plaintext_len,
              const gchar  *passphrase,
              gsize        *out_len,
              GError      **error)
{
    g_return_val_if_fail (plaintext != NULL, NULL);
    g_return_val_if_fail (passphrase != NULL, NULL);

    /* Generate random salt and IV */
    guchar salt[SNAP_SALT_LEN];
    guchar iv[SNAP_IV_LEN];
    RAND_bytes (salt, SNAP_SALT_LEN);
    RAND_bytes (iv, SNAP_IV_LEN);

    /* Derive key */
    g_autofree guchar *key = snap_derive_key (passphrase, salt, SNAP_SALT_LEN, SNAP_KEY_LEN);
    if (key == NULL)
    {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "Key derivation failed");
        return NULL;
    }

    /* Encrypt with AES-256-GCM */
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new ();
    if (ctx == NULL)
    {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "Failed to create cipher context");
        return NULL;
    }

    if (EVP_EncryptInit_ex (ctx, EVP_aes_256_gcm (), NULL, key, iv) != 1)
    {
        EVP_CIPHER_CTX_free (ctx);
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "Encryption init failed");
        return NULL;
    }

    /* Output: magic + salt + iv + ciphertext + tag */
    gsize max_out = SNAP_MAGIC_LEN + SNAP_SALT_LEN + SNAP_IV_LEN +
                    plaintext_len + 16 + SNAP_TAG_LEN;
    guchar *output = g_malloc (max_out);
    gsize pos = 0;

    /* Write header */
    memcpy (output + pos, SNAP_ENCRYPT_MAGIC, SNAP_MAGIC_LEN);
    pos += SNAP_MAGIC_LEN;
    memcpy (output + pos, salt, SNAP_SALT_LEN);
    pos += SNAP_SALT_LEN;
    memcpy (output + pos, iv, SNAP_IV_LEN);
    pos += SNAP_IV_LEN;

    /* Encrypt */
    int len;
    if (EVP_EncryptUpdate (ctx, output + pos, &len, plaintext, plaintext_len) != 1)
    {
        EVP_CIPHER_CTX_free (ctx);
        g_free (output);
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "Encryption failed");
        return NULL;
    }
    pos += len;

    int final_len;
    if (EVP_EncryptFinal_ex (ctx, output + pos, &final_len) != 1)
    {
        EVP_CIPHER_CTX_free (ctx);
        g_free (output);
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "Encryption finalization failed");
        return NULL;
    }
    pos += final_len;

    /* Get tag */
    guchar tag[SNAP_TAG_LEN];
    EVP_CIPHER_CTX_ctrl (ctx, EVP_CTRL_GCM_GET_TAG, SNAP_TAG_LEN, tag);
    memcpy (output + pos, tag, SNAP_TAG_LEN);
    pos += SNAP_TAG_LEN;

    EVP_CIPHER_CTX_free (ctx);

    *out_len = pos;
    return output;
}

guchar *
snap_decrypt (const guchar *ciphertext,
              gsize         ciphertext_len,
              const gchar  *passphrase,
              gsize        *out_len,
              GError      **error)
{
    g_return_val_if_fail (ciphertext != NULL, NULL);
    g_return_val_if_fail (passphrase != NULL, NULL);

    gsize header_len = SNAP_MAGIC_LEN + SNAP_SALT_LEN + SNAP_IV_LEN;
    if (ciphertext_len < header_len + SNAP_TAG_LEN)
    {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                             "Ciphertext too short");
        return NULL;
    }

    /* Verify magic */
    if (memcmp (ciphertext, SNAP_ENCRYPT_MAGIC, SNAP_MAGIC_LEN) != 0)
    {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                             "Not a snap encrypted file");
        return NULL;
    }

    /* Extract salt, IV, encrypted data, and tag */
    const guchar *salt = ciphertext + SNAP_MAGIC_LEN;
    const guchar *iv = salt + SNAP_SALT_LEN;
    const guchar *enc_data = iv + SNAP_IV_LEN;
    gsize enc_len = ciphertext_len - header_len - SNAP_TAG_LEN;
    const guchar *tag = ciphertext + ciphertext_len - SNAP_TAG_LEN;

    /* Derive key */
    g_autofree guchar *key = snap_derive_key (passphrase, salt, SNAP_SALT_LEN, SNAP_KEY_LEN);
    if (key == NULL)
    {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "Key derivation failed");
        return NULL;
    }

    /* Decrypt */
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new ();
    if (EVP_DecryptInit_ex (ctx, EVP_aes_256_gcm (), NULL, key, iv) != 1)
    {
        EVP_CIPHER_CTX_free (ctx);
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "Decryption init failed");
        return NULL;
    }

    guchar *output = g_malloc (enc_len + 1);
    int len;
    if (EVP_DecryptUpdate (ctx, output, &len, enc_data, enc_len) != 1)
    {
        EVP_CIPHER_CTX_free (ctx);
        g_free (output);
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "Decryption failed");
        return NULL;
    }

    /* Set tag */
    EVP_CIPHER_CTX_ctrl (ctx, EVP_CTRL_GCM_SET_TAG, SNAP_TAG_LEN, (void *) tag);

    int final_len;
    if (EVP_DecryptFinal_ex (ctx, output + len, &final_len) != 1)
    {
        EVP_CIPHER_CTX_free (ctx);
        g_free (output);
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "Decryption verification failed (wrong passphrase?)");
        return NULL;
    }

    EVP_CIPHER_CTX_free (ctx);

    gsize total_len = len + final_len;
    output[total_len] = '\0';
    *out_len = total_len;

    return output;
}

gboolean
snap_is_encrypted (const guchar *data, gsize len)
{
    if (data == NULL || len < SNAP_MAGIC_LEN)
        return FALSE;

    return memcmp (data, SNAP_ENCRYPT_MAGIC, SNAP_MAGIC_LEN) == 0;
}
