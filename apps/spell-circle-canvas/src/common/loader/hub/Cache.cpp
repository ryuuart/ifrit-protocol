/** @file
 * Entries, cache and poll: the key an ask is cached under, the blob and
 * every typed view populated independently on first ask, the probe that
 * caches nothing, and the poll that re-stats every entry and re-decodes
 * the changed ones from one read.
 */

#include "Fetch.h"
#include "sigilloader/hub/Hub.h"

namespace sigil::loader {

using detail::fetchResource;
using detail::FetchResult;
using detail::isNetworkUri;
using detail::localPath;
using detail::readFile;

std::string Hub::cacheKey(std::string_view uri, const ImageOptions* options) {
  std::string key(uri);
  // Each option component rides behind a '\0' separator. No URI that
  // names a real resource can contain that byte, so an option suffix
  // can never alias another URI's key. Nothing ever parses a key back
  // apart — the entry stores its own uri.
  if (options && !options->layer.empty()) {
    key += '\0';
    key += "layer=";
    key += options->layer;
  }
  if (options && (options->width || options->height)) {
    key += '\0';
    key += "size=";
    key += std::to_string(options->width);
    key += "x";
    key += std::to_string(options->height);
  }
  return key;
}

std::shared_ptr<const Bytes> Hub::blob(std::string_view uri) {
  const std::string key = cacheKey(uri, nullptr);
  auto it = m_entries.find(key);
  if (it != m_entries.end() && it->second.blob) return it->second.blob;
  FetchResult fetched = fetchResource(*this, m_netCacheDir, m_netPolicy, uri);
  if (!fetched.blob)
    return nullptr;  // not cached: heals as soon as the file appears
  if (it == m_entries.end()) {
    it = m_entries.emplace(key, Entry{}).first;
    it->second.uri = std::string(uri);
    it->second.path = std::move(fetched.path);
    it->second.mtime = fetched.mtime;
  }
  it->second.blob = std::move(fetched.blob);
  return it->second.blob;
}

std::optional<std::string> Hub::text(std::string_view uri) {
  auto bytes = blob(uri);
  if (!bytes) return std::nullopt;
  return std::string(bytes->asText());
}

std::shared_ptr<const void> Hub::loadView(const std::string& key,
                                          std::string_view uri,
                                          std::type_index type,
                                          const Redecode& decode) {
  if (!decode) return nullptr;
  auto it = m_entries.find(key);
  if (it != m_entries.end())
    if (const auto view = it->second.views.find(type);
        view != it->second.views.end() && view->second.value)
      return view->second.value;
  // Bytes already cached by a blob() ask are decoded as they are —
  // one read serves every view of the entry; otherwise fetch fresh.
  std::shared_ptr<const Bytes> bytes;
  std::filesystem::path path;
  FetchResult fetched;
  if (it != m_entries.end() && it->second.blob) {
    bytes = it->second.blob;
    path = it->second.path;
  } else {
    fetched = fetchResource(*this, m_netCacheDir, m_netPolicy, uri);
    if (!fetched.blob) return nullptr;
    bytes = fetched.blob;
    path = fetched.path;
  }
  auto value = decode(*bytes, path);
  if (!value) return nullptr;
  if (it == m_entries.end()) {
    it = m_entries.emplace(key, Entry{}).first;
    it->second.uri = std::string(uri);
    it->second.path = std::move(path);
    it->second.mtime = fetched.mtime;
  }
  // The encoded bytes are not kept unless blob() asked for them, so
  // a decode-only workload never holds them alive beside the value.
  View& view = it->second.views[type];
  view.value = std::move(value);
  view.decode = decode;
  return view.value;
}

std::shared_ptr<const sigil::image::ImageAsset> Hub::image(
    std::string_view uri, const ImageOptions& options) {
  using sigil::image::ImageAsset;
  const std::type_index type(typeid(ImageAsset));
  if (options == ImageOptions{})
    return std::static_pointer_cast<const ImageAsset>(
        loadView(cacheKey(uri, nullptr), uri, type, registeredDecoder(type)));
  // A layer or a size is a different decode: its own entry, with the
  // options riding in the decode so poll() re-runs the same one.
  const Redecode decode =
      [options](
          const Bytes& bytes,
          const std::filesystem::path& path) -> std::shared_ptr<const void> {
    auto decoded = sigil::image::decodeImage(bytes.bytes.data(),
                                             bytes.bytes.size(), options, path);
    if (!decoded) return nullptr;
    return std::make_shared<const ImageAsset>(std::move(*decoded));
  };
  return std::static_pointer_cast<const ImageAsset>(
      loadView(cacheKey(uri, &options), uri, type, decode));
}

std::shared_ptr<const sigil::image::ChannelData> Hub::channels(
    std::string_view uri) {
  return load<sigil::image::ChannelData>(uri);
}

std::optional<ResourceInfo> Hub::probe(std::string_view uri) const {
  FetchResult fetched = fetchResource(*this, m_netCacheDir, m_netPolicy, uri);
  if (!fetched.blob) return std::nullopt;
  if (auto probe =
          sigil::image::probeImage(fetched.blob->bytes.data(),
                                   fetched.blob->bytes.size(), fetched.path)) {
    ResourceInfo info;
    info.kind = ResourceInfo::Kind::Image;
    info.byteSize = fetched.blob->bytes.size();
    info.format = probe->format;
    info.image = std::move(*probe);
    return info;
  }
  ResourceInfo info;
  info.kind = ResourceInfo::Kind::Data;
  info.byteSize = fetched.blob->bytes.size();
  return info;
}

/** Re-reads entry.uri from disk and re-decodes every populated view
 *  from that one read. Nothing is committed until every decode has
 *  succeeded, so a half-written file cannot leave the views
 *  disagreeing with each other. */
bool Hub::reload(Entry& entry) {
  const std::filesystem::path path = localPath(*this, entry.uri);
  auto bytes = readFile(path);
  if (!bytes) return false;
  std::vector<std::pair<View*, std::shared_ptr<const void>>> decoded;
  for (auto& [type, view] : entry.views) {
    if (!view.value) continue;
    auto value = view.decode(*bytes, path);
    if (!value) return false;
    decoded.emplace_back(&view, std::move(value));
  }
  for (auto& [view, value] : decoded) view->value = std::move(value);
  if (entry.blob) entry.blob = std::move(bytes);
  entry.path = path;
  return true;
}

bool Hub::poll() {
  bool changed = false;
  for (auto it = m_entries.begin(); it != m_entries.end();) {
    Entry& entry = it->second;
    if (isNetworkUri(entry.uri)) {
      ++it;  // no mtime to watch: network entries stay as fetched
      continue;
    }
    const std::filesystem::path path = localPath(*this, entry.uri);
    std::error_code ec;
    const auto mtime = std::filesystem::last_write_time(path, ec);
    if (ec) {
      it = m_entries.erase(it);  // vanished: next ask sees the truth
      changed = true;
      continue;
    }
    if (mtime != entry.mtime && reload(entry)) {
      entry.mtime = mtime;
      changed = true;
    }
    ++it;
  }
  return changed;
}

}  // namespace sigil::loader
