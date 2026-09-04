/** @file
 * Entries, cache and poll: the key an ask is cached under, the blob and
 * every typed view populated independently on first ask, the probe that
 * caches nothing, and the poll that re-stats every entry and re-decodes
 * the changed ones from one read.
 */

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>

#include <boost/container/flat_set.hpp>

#include "Fetch.h"
#include "Residency.h"
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
  std::filesystem::path networkCache;
  NetworkPolicy networkPolicy;
  {
    const std::lock_guard lock(m_synchronization->mutex);
    const auto cached = m_entries.find(key);
    if (cached != m_entries.end() && cached->second.blob)
      return cached->second.blob;
    networkCache = m_netCacheDir;
    networkPolicy = m_netPolicy;
  }

  FetchResult fetched = fetchResource(*this, networkCache, networkPolicy, uri);
  if (!fetched.blob)
    return nullptr;  // not cached: heals as soon as the file appears
  const std::lock_guard lock(m_synchronization->mutex);
  auto it = m_entries.find(key);
  if (it != m_entries.end() && it->second.blob) return it->second.blob;
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

  boost::container::flat_set<std::string, std::less<>> seen;
  std::vector<Pending> pending;
  size_t ready = 0;
  std::filesystem::path networkCache;
  NetworkPolicy networkPolicy;
  {
    const std::lock_guard lock(m_synchronization->mutex);
    networkCache = m_netCacheDir;
    networkPolicy = m_netPolicy;
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
  }

  const auto fetch = [&](size_t from, size_t to) {
    for (size_t i = from; i != to; ++i)
      pending[i].fetched =
          fetchResource(*this, networkCache, networkPolicy, pending[i].uri);
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

  {
    const std::lock_guard lock(m_synchronization->mutex);
    for (Pending& ask : pending) {
      Entry& entry = m_entries[ask.key];
      if (entry.blob) {
        ++ready;
        continue;
      }
      if (!ask.fetched.blob) {
        if (entry.uri.empty()) m_entries.erase(ask.key);
        continue;
      }
      entry.blob = std::move(ask.fetched.blob);
      if (entry.uri.empty()) {
        entry.uri = std::move(ask.uri);
        entry.path = std::move(ask.fetched.path);
        entry.mtime = ask.fetched.mtime;
      }
      ++ready;
    }
  }
  return ready;
}

size_t Hub::discardUnretained() {
  const std::lock_guard cacheLock(m_synchronization->mutex);
  if (!m_residency) {
    const size_t discarded = m_entries.size();
    m_entries.clear();
    return discarded;
  }

  const std::lock_guard residencyLock(m_residency->mutex);
  size_t discarded = 0;
  for (auto entry = m_entries.begin(); entry != m_entries.end();) {
    if (m_residency->pins.contains(entry->second.uri)) {
      ++entry;
    } else {
      entry = m_entries.erase(entry);
      ++discarded;
    }
  }
  return discarded;
}

std::shared_ptr<const void> Hub::loadView(const std::string& key,
                                          std::string_view uri,
                                          std::type_index type,
                                          const Redecode& decode) {
  if (!decode) return nullptr;

  // Bytes already cached by a blob() ask are decoded as they are —
  // one read serves every view of the entry; otherwise fetch fresh.
  std::shared_ptr<const Bytes> bytes;
  std::filesystem::path path;
  std::filesystem::file_time_type mtime;
  std::filesystem::path networkCache;
  NetworkPolicy networkPolicy;
  {
    const std::lock_guard lock(m_synchronization->mutex);
    const auto entry = m_entries.find(key);
    if (entry != m_entries.end()) {
      if (const auto view = entry->second.views.find(type);
          view != entry->second.views.end() && view->second.value)
        return view->second.value;
      if (entry->second.blob) {
        bytes = entry->second.blob;
        path = entry->second.path;
        mtime = entry->second.mtime;
      }
    }
    networkCache = m_netCacheDir;
    networkPolicy = m_netPolicy;
  }

  FetchResult fetched;
  if (!bytes) {
    fetched = fetchResource(*this, networkCache, networkPolicy, uri);
    if (!fetched.blob) return nullptr;
    bytes = fetched.blob;
    path = fetched.path;
    mtime = fetched.mtime;
  }
  auto value = decode(*bytes, path);
  if (!value) return nullptr;

  const std::lock_guard lock(m_synchronization->mutex);
  auto [entry, inserted] = m_entries.try_emplace(key);
  if (!inserted) {
    if (const auto view = entry->second.views.find(type);
        view != entry->second.views.end() && view->second.value)
      return view->second.value;
  } else {
    entry->second.uri = std::string(uri);
    entry->second.path = std::move(path);
    entry->second.mtime = mtime;
  }
  // The encoded bytes are not kept unless blob() asked for them, so
  // a decode-only workload never holds them alive beside the value.
  View& view = entry->second.views[type];
  view.value = std::move(value);
  view.decode = decode;
  return view.value;
}

std::shared_ptr<const void> Hub::loadRegisteredView(const std::string& key,
                                                    std::string_view uri,
                                                    std::type_index type) {
  Redecode decode;
  {
    const std::lock_guard lock(m_synchronization->mutex);
    const auto entry = m_entries.find(key);
    if (entry != m_entries.end())
      if (const auto view = entry->second.views.find(type);
          view != entry->second.views.end() && view->second.value)
        return view->second.value;
    const auto registered = m_decoders.find(type);
    if (registered != m_decoders.end()) decode = registered->second;
  }
  return loadView(key, uri, type, decode);
}

std::shared_ptr<const sigil::image::ImageAsset> Hub::image(
    std::string_view uri, const image::DecodeOptions& options) {
  using sigil::image::ImageAsset;
  const std::type_index type(typeid(ImageAsset));
  if (options == image::DecodeOptions{})
    return std::static_pointer_cast<const ImageAsset>(
        loadRegisteredView(cacheKey(uri, nullptr), uri, type));
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
  std::filesystem::path networkCache;
  NetworkPolicy networkPolicy;
  {
    const std::lock_guard lock(m_synchronization->mutex);
    networkCache = m_netCacheDir;
    networkPolicy = m_netPolicy;
  }
  FetchResult fetched = fetchResource(*this, networkCache, networkPolicy, uri);
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
  const std::lock_guard lock(m_synchronization->mutex);
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
