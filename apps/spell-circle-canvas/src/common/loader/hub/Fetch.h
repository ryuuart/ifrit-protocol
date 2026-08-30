#pragma once

/** @file
 * The seam between the hub's translation units: what one resource ask
 * produces, the local read behind a mount and the network fetch behind
 * the disk cache, and the one preamble every accessor runs to pick
 * between them. Private to the hub feature.
 */

#include <filesystem>
#include <memory>
#include <string_view>

#include "sigilloader/hub/Hub.h"

namespace sigil::loader::detail {

/** The whole file as one Bytes; null when it cannot be opened or read. */
std::shared_ptr<const Bytes> readFile(const std::filesystem::path& path);

bool isNetworkUri(std::string_view uri);

/** The local filesystem path a non-network URI means: file:// strips
 *  to a plain path, mounts resolve, anything else is tried as-is. */
std::filesystem::path localPath(const Hub& hub, std::string_view uri);

/** What one resource ask produced, wherever the bytes came from:
 *  `path` is the decode pathHint (the cache file for network URIs, so
 *  extension-based format hints keep working). */
struct FetchResult {
  std::shared_ptr<const Bytes> blob;
  std::filesystem::path path;
  std::filesystem::file_time_type mtime{};
};

/** Network fetch behind the disk cache. CacheFirst: a present cache
 *  file is served without touching the network (offline-friendly).
 *  Refresh: the network goes first, the cache catches its failures.
 *  Offline: cache only. A fetch success always persists for the next
 *  run. */
FetchResult fetchNetwork(const std::filesystem::path& cacheDir,
                         std::string_view url, NetworkPolicy policy);

/** The one preamble every accessor shares: network URI → NetFetcher
 *  (through the disk cache), anything else → mounted filesystem. */
FetchResult fetchResource(const Hub& hub,
                          const std::filesystem::path& netCacheDir,
                          NetworkPolicy netPolicy, std::string_view uri);

}  // namespace sigil::loader::detail
