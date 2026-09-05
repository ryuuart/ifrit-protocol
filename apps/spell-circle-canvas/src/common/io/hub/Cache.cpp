/** @file
 * Entries, cache and poll: the key an ask is cached under, the blob and
 * every typed view populated independently on first ask, the probe that
 * caches nothing, and the poll that re-stats every entry and re-decodes
 * the changed ones from one read.
 */

#include <sigilcore/schedule/ConcurrentIo.h>

#include <boost/container/flat_set.hpp>

#include <optional>
#include <system_error>
#include <utility>
#include <vector>

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
  detail::NetworkAccess network;
  {
    const std::lock_guard lock(m_mutex);
    const auto cached = m_entries.find(key);
    if (cached != m_entries.end() && cached->second.blob)
      return cached->second.blob;
    network = {m_networkCacheDir, m_networkPolicy, m_networkTransport};
  }

  FetchResult fetched = fetchResource(*this, network, uri);
  if (!fetched.blob)
    return nullptr;  // not cached: heals as soon as the file appears
  const std::lock_guard lock(m_mutex);
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
  detail::NetworkAccess network;
  {
    const std::lock_guard lock(m_mutex);
    network = {m_networkCacheDir, m_networkPolicy, m_networkTransport};
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

  // A fetch waits on a disk or on a server, so it runs on the threads a
  // blocking call gets rather than on the ones a computation divides
  // itself over: a preload of a hundred URLs must not be able to stall
  // every parallel range in the process for as long as a server takes.
  // Each ask writes its own element and nothing else, and the cache is
  // not held meanwhile.
  core::schedule::concurrentIo(pending, [&](Pending& ask) {
    ask.fetched = fetchResource(*this, network, ask.uri);
  });

  {
    const std::lock_guard lock(m_mutex);
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
  const std::lock_guard cacheLock(m_mutex);
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
  detail::NetworkAccess network;
  {
    const std::lock_guard lock(m_mutex);
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
    network = {m_networkCacheDir, m_networkPolicy, m_networkTransport};
  }

  FetchResult fetched;
  if (!bytes) {
    fetched = fetchResource(*this, network, uri);
    if (!fetched.blob) return nullptr;
    bytes = fetched.blob;
    path = fetched.path;
    mtime = fetched.mtime;
  }
  auto value = decode(*bytes, path);
  if (!value) return nullptr;

  const std::lock_guard lock(m_mutex);
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
    const std::lock_guard lock(m_mutex);
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
  detail::NetworkAccess network;
  {
    const std::lock_guard lock(m_mutex);
    network = {m_networkCacheDir, m_networkPolicy, m_networkTransport};
  }
  FetchResult fetched = fetchResource(*this, network, uri);
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

/** Re-reads one entry's file and re-decodes every populated view from
 *  that one read, holding no lock: a decoder may take as long as it
 *  likes, and may even ask this hub for another resource, without
 *  stalling anyone else's cache hit. Nothing is committed until every
 *  decode has succeeded, so a half-written file cannot leave the views
 *  disagreeing with each other. Null when the read or any decode
 *  fails. */
std::optional<Hub::Reloaded> Hub::reload(const Reload& pending) const {
  const std::filesystem::path path = localPath(*this, pending.uri);
  auto bytes = readFile(path);
  if (!bytes) return std::nullopt;
  Reloaded reloaded;
  for (const auto& [type, decode] : pending.decodes) {
    auto value = decode(*bytes, path);
    if (!value) return std::nullopt;
    reloaded.views.emplace_back(type, std::move(value));
  }
  reloaded.path = path;
  reloaded.mtime = pending.mtime;
  if (pending.holdsBlob) reloaded.blob = std::move(bytes);
  return reloaded;
}

bool Hub::poll() {
  // Three phases, so the lock is never held across a stat or a decode:
  // snapshot what every local entry needs under the lock, stat and
  // reload outside it, then re-lock and commit only into entries that
  // still carry the stamp the snapshot saw. An entry that another
  // thread replaced or dropped meanwhile keeps that thread's answer.
  std::vector<Reload> pending;
  {
    const std::lock_guard lock(m_mutex);
    pending.reserve(m_entries.size());
    for (const auto& [key, entry] : m_entries) {
      if (isNetworkUri(entry.uri))
        continue;  // no mtime to watch: network entries stay as fetched
      Reload reload{key, entry.uri, entry.mtime, entry.blob != nullptr, {}};
      for (const auto& [type, view] : entry.views)
        if (view.value) reload.decodes.emplace_back(type, view.decode);
      pending.push_back(std::move(reload));
    }
  }

  struct Outcome {
    const Reload* reload;
    bool vanished = false;
    std::optional<Reloaded> reloaded;
  };
  std::vector<Outcome> outcomes;
  for (const Reload& reload : pending) {
    const std::filesystem::path path = localPath(*this, reload.uri);
    std::error_code ec;
    const auto mtime = std::filesystem::last_write_time(path, ec);
    if (ec) {
      outcomes.push_back({&reload, true, std::nullopt});
      continue;
    }
    if (mtime == reload.mtime) continue;
    Reload changed = reload;
    changed.mtime = mtime;
    if (auto reloaded = this->reload(changed))
      outcomes.push_back({&reload, false, std::move(reloaded)});
  }
  if (outcomes.empty()) return false;

  bool changed = false;
  const std::lock_guard lock(m_mutex);
  for (Outcome& outcome : outcomes) {
    const auto found = m_entries.find(outcome.reload->key);
    if (found == m_entries.end() || found->second.mtime != outcome.reload->mtime)
      continue;  // replaced or dropped since the snapshot: not ours
    if (outcome.vanished) {
      m_entries.erase(found);  // vanished: next ask sees the truth
      changed = true;
      continue;
    }
    Entry& entry = found->second;
    Reloaded& reloaded = *outcome.reloaded;
    for (auto& [type, value] : reloaded.views) {
      const auto view = entry.views.find(type);
      if (view != entry.views.end()) view->second.value = std::move(value);
    }
    if (entry.blob && reloaded.blob) entry.blob = std::move(reloaded.blob);
    entry.path = std::move(reloaded.path);
    entry.mtime = reloaded.mtime;
    changed = true;
  }
  return changed;
}

}  // namespace sigil::io
