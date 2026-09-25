#pragma once

/** @file
 * @ingroup io-hub
 * The hub's network settings: NetworkPolicy, when an http(s):// ask may
 * touch the network against its on-disk cache, and NetworkOptions, the
 * whole of how a hub fetches.
 */

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace sigil::io {

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

/** A function that answers a URL with its body, or nothing when the
 *  fetch failed. The hub's default is libcurl; a host may set its own. */
using NetworkTransport =
    std::function<std::optional<std::vector<std::byte>>(std::string_view url)>;

/** HOW A HUB FETCHES http(s):// RESOURCES, given once in
 *  `HubOptions::network`. */
struct NetworkOptions {
  /** When the network may be touched against the disk cache. */
  NetworkPolicy policy = NetworkPolicy::CacheFirst;
  /** Where fetches persist; empty is the platform cache location /
   *  "SigilIO/network", the temp directory only where the platform names
   *  no cache location. A present resource is served without touching
   *  the network. */
  std::filesystem::path cacheDirectory;
  /** What answers a URL with its body; empty is libcurl. A host with its
   *  own HTTP stack, or a test that needs a fetch to fail without
   *  touching a resolver, hands one in. */
  NetworkTransport transport;
};

}  // namespace sigil::io
