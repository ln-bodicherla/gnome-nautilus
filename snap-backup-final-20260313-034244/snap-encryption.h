/* snap-encryption.h
 *
 * AES-256-GCM encryption via OpenSSL. Key derivation with HKDF.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

/* Encrypt data with passphrase using AES-256-GCM */
guchar   *snap_encrypt              (const guchar  *plaintext,
                                     gsize          plaintext_len,
                                     const gchar   *passphrase,
                                     gsize         *out_len,
                                     GError       **error);

/* Decrypt data with passphrase */
guchar   *snap_decrypt              (const guchar  *ciphertext,
                                     gsize          ciphertext_len,
                                     const gchar   *passphrase,
                                     gsize         *out_len,
                                     GError       **error);

/* Derive key from passphrase using HKDF */
guchar   *snap_derive_key           (const gchar   *passphrase,
                                     const guchar  *salt,
                                     gsize          salt_len,
                                     gsize          key_len);

/* Check if a blob is encrypted (magic bytes check) */
gboolean  snap_is_encrypted         (const guchar  *data,
                                     gsize          len);

G_END_DECLS
