/* snap-hash.h
 *
 * SHA256 hashing via OpenSSL EVP + Rust FFI wrapper.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

/* Hash data in memory */
gchar    *snap_hash_data            (const guchar *data,
                                     gsize         length);

/* Hash a file by path */
gchar    *snap_hash_file            (const gchar  *file_path,
                                     GError      **error);

/* Hash a string */
gchar    *snap_hash_string          (const gchar  *text);

/* Verify hash matches content */
gboolean  snap_hash_verify          (const gchar  *hash,
                                     const guchar *data,
                                     gsize         length);

/* Rust FFI - if libsnapcore is available */
#ifdef HAVE_SNAPCORE
extern gchar *snapcore_hash_file    (const gchar  *path);
#endif

G_END_DECLS
