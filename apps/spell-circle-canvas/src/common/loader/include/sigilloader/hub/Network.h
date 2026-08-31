#pragma once

/** @file
 * The hub's network contract: NetworkPolicy, when an http(s):// ask may
 * touch the network against its on-disk cache, and networkCacheKey(),
 * the cache filename a URL maps to.
 */

#include <string>
#include <string_view>

namespace sigil::loader {

/** When the hub may touch the network for http(s):// URIs. The policy
 *  applies when a resource is first asked for (or asked again after
 *  its entry was dropped); already-loaded entries stay as loaded. */
enum class NetworkPolicy {
  /** Default: a present cache file is served without any network
   *  traffic; a miss fetches and persists. Offline-safe out of the
   *  box once a resource has been seen. */
  CacheFirst,
  /** Ask the network first (pick up upstream changes); a failed fetch
   *  falls back to the cached copy, so flaky networks degrade to
   *  CacheFirst instead of failing. */
  Refresh,
  /** Never touch the network: cache hit or fail. For hermetic runs
   *  and guaranteed-offline hosts. */
  Offline,
};

/** The on-disk cache filename (no directory) a network URL maps to:
 *  hex of the URL's hash plus the URL path's extension, so decode
 *  pathHints keep working. Exposed for tests and cache pre-seeding. */
std::string networkCacheKey(std::string_view url);

}  // namespace sigil::loader
