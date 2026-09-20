#pragma once

/** @file
 * @ingroup io-hub
 * The hub's network contract: NetworkPolicy, when an http(s):// ask may
 * touch the network against its on-disk cache, and the resource operations
 * that inspect and populate that cache without contacting a server.
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

/** The byte count of a regular file retained for an http(s):// URL,
 *  without fetching or decoding it. Nothing when the URL is not a
 *  network resource, the cache entry is missing, or its size cannot be
 *  read; zero is a present, empty resource. An empty @p cacheDirectory
 *  selects the same platform cache directory as a Hub given no
 *  override. */
std::optional<std::uintmax_t> probeNetworkCache(
    std::string_view url, const std::filesystem::path& cacheDirectory = {});

/** Retains @p bytes for an http(s):// URL without contacting its server,
 *  so later disk-cache reads answer them until another seed or fetch
 *  replaces them; empty bytes are valid. False for a non-network URL or
 *  a failed write. An empty @p cacheDirectory selects the same platform
 *  cache directory as a Hub given no override.
 *  @silent a Hub's already-loaded views, which keep their values. */
bool seedNetworkCache(std::string_view url, std::span<const std::byte> bytes,
                      const std::filesystem::path& cacheDirectory = {});

}  // namespace sigil::io
