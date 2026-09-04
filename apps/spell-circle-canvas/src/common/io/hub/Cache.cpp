/** @file
 * Entries, cache and poll: the key an ask is cached under, the blob and
 * every typed view populated independently on first ask, the probe that
 * caches nothing, and the poll that re-stats every entry and re-decodes
 * the changed ones from one read.
 */

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <set>

#include "Fetch.h"
#include "sigilio/hub/Hub.h"

namespace sigil::io {

using detail::fetchResource;
using detail::FetchResult;
using detail::isNetworkUri;
using detail::localPath;
using detail::readFile;

std::string Hub::cacheKey(std::string_view uri,
                          const image::DecodeOptions* options) {
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
    key += 'x';
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

size_t Hub::preload(std::span<const std::string_view> uris) {
  struct Pending {
    std::string uri;
    std::string key;
    FetchResult fetched;
  };

  std::set<std::string, std::less<>> seen;
  std::vector<Pending> pending;
  size_t ready = 0;
  for (std::string_view uri : uris) {
    if (!seen.emplace(uri).second) continue;
    const std::string key = cacheKey(uri, nullptr);
    const auto cached = m_entries.find(key);
    if (cached != m_entries.end() && cached->second.blob) {
      ++ready;
      continue;
    }
    pending.push_back({std::string(uri), key, {}});
  }

  const auto fetch = [&](size_t from, size_t to) {
    for (size_t i = from; i != to; ++i)
      pending[i].fetched =
          fetchResource(*this, m_netCacheDir, m_netPolicy, pending[i].uri);
  };
  if (pending.size() < 4) {
    fetch(0, pending.size());
  } else {
    oneapi::tbb::parallel_for(
        oneapi::tbb::blocked_range<size_t>(0, pending.size(), 2),
        [&](const oneapi::tbb::blocked_range<size_t>& range) {
          fetch(range.begin(), range.end());
        });
  }

  for (Pending& ask : pending) {
    if (!ask.fetched.blob) continue;
    Entry& entry = m_entries[ask.key];
    entry.uri = std::move(ask.uri);
    entry.blob = std::move(ask.fetched.blob);
    entry.path = std::move(ask.fetched.path);
    entry.mtime = ask.fetched.mtime;
    ++ready;
  }
  return ready;
}

size_t Hub::preloadDirectory(std::string_view uriPrefix) {
  if (isNetworkUri(uriPrefix)) return 0;
  const std::filesystem::path directory = localPath(*this, uriPrefix);
  std::error_code error;
  if (!std::filesystem::is_directory(directory, error) || error) return 0;

  std::string base(uriPrefix);
  if (!base.empty() && !base.ends_with('/')) base += '/';
  std::vector<std::string> names;
  for (std::filesystem::recursive_directory_iterator it(directory, error), end;
       !error && it != end; it.increment(error)) {
    if (!it->is_regular_file(error) || error) continue;
    const std::filesystem::path relative =
        std::filesystem::relative(it->path(), directory, error);
    if (error) break;
    names.push_back(base + relative.generic_string());
  }
  std::ranges::sort(names);
  std::vector<std::string_view> uris;
  uris.reserve(names.size());
  for (const std::string& name : names) uris.push_back(name);
  return preload(uris);
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
    std::string_view uri, const image::DecodeOptions& options) {
  using sigil::image::ImageAsset;
  const std::type_index type(typeid(ImageAsset));
  if (options == image::DecodeOptions{})
    return std::static_pointer_cast<const ImageAsset>(
        loadView(cacheKey(uri, nullptr), uri, type, registeredDecoder(type)));
  // A layer or a size is a different decode: its own entry, with the
  // options riding in the decode so poll() re-runs the same one.
  const Redecode decode =
      [options = options](
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

}  // namespace sigil::io
