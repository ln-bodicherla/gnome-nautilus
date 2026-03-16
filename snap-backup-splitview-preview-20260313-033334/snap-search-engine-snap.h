/* snap-search-engine-snap.h
 *
 * Search provider that queries the snap version store.
 * Finds files with version history and searches content
 * across current and previous versions.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "nautilus-search-provider.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_SEARCH_ENGINE_SNAP (nautilus_search_engine_snap_get_type ())
G_DECLARE_FINAL_TYPE (NautilusSearchEngineSnap,
                       nautilus_search_engine_snap,
                       NAUTILUS, SEARCH_ENGINE_SNAP,
                       NautilusSearchProvider)

NautilusSearchEngineSnap *nautilus_search_engine_snap_new (void);

G_END_DECLS
