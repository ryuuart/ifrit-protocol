/** @file
 * Mounts and resolution: a URI prefix mapped onto a directory, the
 * longest-prefix resolve, the local path a non-network URI means, the
 * file read behind it, and the preamble that sends a network URI the
 * other way.
 */

#include <fstream>

#include "Fetch.h"
#include "sigilloader/hub/Hub.h"

namespace sigil::loader {

namespace detail {

std::shared_ptr<const Bytes> readFile(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream) return nullptr;
  const std::streamsize size = stream.tellg();
  stream.seekg(0);
  auto blob = std::make_shared<Bytes>();
  blob->bytes.resize((size_t)size);
  if (!stream.read(reinterpret_cast<char*>(blob->bytes.data()), size))
    return nullptr;
  return blob;
}

/** The local filesystem path a non-network URI means: file:// strips
 *  to a plain path, mounts resolve, anything else is tried as-is. */
std::filesystem::path localPath(const Hub& hub, std::string_view uri) {
  if (uri.starts_with("file://"))
    return std::filesystem::path(std::string(uri.substr(7)));
  std::filesystem::path path = hub.resolve(uri);
  if (path.empty()) path = std::string(uri);
  return path;
}

/** The one preamble every accessor shares: network URI → NetFetcher
 *  (through the disk cache), anything else → mounted filesystem. */
FetchResult fetchResource(const Hub& hub,
                          const std::filesystem::path& netCacheDir,
                          NetworkPolicy netPolicy, std::string_view uri) {
  if (isNetworkUri(uri)) {
    const std::filesystem::path cacheDir =
        netCacheDir.empty()
            ? std::filesystem::temp_directory_path() / "sigilloader-net-cache"
            : netCacheDir;
    return fetchNetwork(cacheDir, uri, netPolicy);
  }
  std::filesystem::path path = localPath(hub, uri);
  std::error_code ec;
  const auto mtime = std::filesystem::last_write_time(path, ec);
  if (ec) return {};
  auto blob = readFile(path);
  if (!blob) return {};
  return {std::move(blob), std::move(path), mtime};
}

}  // namespace detail

void Hub::mount(std::string prefix, std::filesystem::path dir) {
  for (auto& [existing, path] : m_mounts)
    if (existing == prefix) {
      path = std::move(dir);
      return;
    }
  m_mounts.emplace_back(std::move(prefix), std::move(dir));
}

std::filesystem::path Hub::resolve(std::string_view uri) const {
  const std::pair<std::string, std::filesystem::path>* best = nullptr;
  for (const auto& mountPair : m_mounts)
    if (uri.starts_with(mountPair.first) &&
        (!best || mountPair.first.size() > best->first.size()))
      best = &mountPair;
  if (best) return best->second / std::string(uri.substr(best->first.size()));
  return {};
}

}  // namespace sigil::loader
