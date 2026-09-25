#pragma once

/** @file
 * @ingroup io-hub
 * THE NETWORK CACHE, AND A HUB'S NETWORK SETTINGS AFTER IT WAS MADE:
 * inspecting and seeding the on-disk cache without contacting a server,
 * and changing how a standing hub fetches. `HubOptions::network` is the
 * ordinary way to set the latter.
 */

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "sigilio/hub/Network.h"

namespace sigil::io {

class Hub;

/** THE ON-DISK CACHE http(s):// resources are kept in, read and written
 *  without contacting a server. An empty directory is the same platform
 *  cache directory a Hub given none uses. */
class NetworkCache {
 public:
  explicit NetworkCache(std::filesystem::path directory = {})
      : m_directory(std::move(directory)) {}

  /** The byte count of the regular file retained for @p url, without
   *  fetching or decoding it. Nothing when the URL is not a network
   *  resource, the entry is missing, or its size cannot be read; zero is
   *  a present, empty resource. */
  std::optional<std::uintmax_t> byteSize(std::string_view url) const;

  /** Retains @p bytes for @p url without contacting its server, so later
   *  disk-cache reads answer them until another put or fetch replaces
   *  them; empty bytes are valid. False for a non-network URL or a
   *  failed write.
   *  @silent a Hub's already-loaded views, which keep their values. */
  bool put(std::string_view url, std::span<const std::byte> bytes) const;

 private:
  std::filesystem::path m_directory;
};

/** Where @p hub's network fetches persist from now on. */
void setNetworkCacheDirectory(Hub& hub, std::filesystem::path directory);

/** How @p hub's http(s):// asks may use the network from now on. */
void setNetworkPolicy(Hub& hub, NetworkPolicy policy);

/** What answers @p hub's http(s):// URLs from now on; an empty function
 *  restores libcurl. The disk cache and the policy stay in front of
 *  whichever transport is set. */
void setNetworkTransport(Hub& hub, NetworkTransport transport);

}  // namespace sigil::io
