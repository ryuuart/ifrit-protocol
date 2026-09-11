/** @file
 * Mounts and resolution: a URI prefix mapped onto a directory, the
 * longest-prefix resolve, the local path a non-network URI means, the
 * file read and the file write behind it, and the preamble that sends a
 * network URI the other way.
 */

#include <fstream>
#include <iterator>

#include "Fetch.h"
#include "sigilio/hub/Hub.h"

namespace sigil::io {

namespace detail {

bool beneathMount(std::string_view relative) {
  const std::filesystem::path path{std::string(relative)};
  if (path.has_root_path()) return false;
  for (const std::filesystem::path& component : path)
    if (component == "..") return false;
  return true;
}

std::shared_ptr<const Bytes> readFile(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream) return nullptr;
  // A stream that cannot say where its end is answers -1, which as a
  // size is every byte there could ever be.
  const std::streamsize size = stream.tellg();
  if (size < 0) return nullptr;
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

/** The one preamble every accessor shares: network URI → the network
 *  transport through the disk cache, anything else → mounted
 *  filesystem. */
FetchResult fetchResource(const Hub& hub, const NetworkAccess& network,
                          std::string_view uri) {
  if (isNetworkUri(uri)) return fetchNetwork(network, uri);
  std::filesystem::path path = localPath(hub, uri);
  std::error_code ec;
  // A directory has a write time like any other entry, so the question
  // is whether the URI names a FILE — bytes are what the hub answers.
  if (!std::filesystem::is_regular_file(path, ec) || ec) return {};
  const auto mtime = std::filesystem::last_write_time(path, ec);
  if (ec) return {};
  auto blob = readFile(path);
  if (!blob) return {};
  return {std::move(blob), std::move(path), mtime};
}

}  // namespace detail

void Hub::mount(std::string prefix, std::filesystem::path dir) {
  const std::lock_guard lock(m_mutex);
  for (auto& [existing, path] : m_mounts)
    if (existing == prefix) {
      path = std::move(dir);
      return;
    }
  m_mounts.emplace_back(std::move(prefix), std::move(dir));
}

bool Hub::write(std::string_view uri, const void* bytes, size_t size) {
  // A hub writes local resources through its mounts. A network URI
  // belongs to its server; changing a local cache cannot write there.
  if (detail::isNetworkUri(uri)) return false;
  // The disk write runs with no lock held: a read of another resource
  // never waits behind it.
  const std::filesystem::path path = detail::localPath(*this, uri);
  if (path.empty()) return false;
  if (!writeBytes(path, bytes, size)) return false;
  // Every entry for this URI goes, whatever decode options made it —
  // matched on the uri each entry carries rather than on its key, since
  // a key is never parsed back into the URI it was built from.
  const std::lock_guard lock(m_mutex);
  for (auto it = m_entries.begin(); it != m_entries.end();)
    it = it->second.uri == uri ? m_entries.erase(it) : std::next(it);
  return true;
}

std::filesystem::path Hub::resolve(std::string_view uri) const {
  const std::lock_guard lock(m_mutex);
  const std::pair<std::string, std::filesystem::path>* best = nullptr;
  for (const auto& mountPair : m_mounts)
    if (uri.starts_with(mountPair.first) &&
        (!best || mountPair.first.size() > best->first.size()))
      best = &mountPair;
  if (!best) return {};
  const std::string_view remainder = uri.substr(best->first.size());
  // A mount is a namespace, not a door into the filesystem around it:
  // what it names is beneath its directory, so a remainder that climbs
  // out of it resolves to nothing. The directory walk refuses the same
  // spelling, and one rule is what makes a selector and a fetch agree
  // about what a mount holds.
  if (!detail::beneathMount(remainder)) return {};
  return best->second / std::string(remainder);
}

std::vector<std::pair<std::string, std::filesystem::path>>
Hub::mountedDirectories() const {
  const std::lock_guard lock(m_mutex);
  return m_mounts;
}

}  // namespace sigil::io
