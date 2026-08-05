#include "sigilloader/Loader.h"

#include <curl/curl.h>

#include <cstdio>
#include <fstream>

namespace sigil::loader {

namespace {

std::shared_ptr<const Blob> readFile(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream)
    return nullptr;
  const std::streamsize size = stream.tellg();
  stream.seekg(0);
  auto blob = std::make_shared<Blob>();
  blob->bytes.resize((size_t)size);
  if (!stream.read(reinterpret_cast<char *>(blob->bytes.data()), size))
    return nullptr;
  return blob;
}

/** Network entries carry no mtime to watch; poll() skips this. */
constexpr auto kNetworkMtime = std::filesystem::file_time_type::min();

bool isNetworkUri(std::string_view uri) {
  return uri.starts_with("http://") || uri.starts_with("https://");
}

/** The local filesystem path a non-network URI means: file:// strips
 *  to a plain path, mounts resolve, anything else is tried as-is. */
std::filesystem::path localPath(const Hub &hub, std::string_view uri) {
  if (uri.starts_with("file://"))
    return std::filesystem::path(std::string(uri.substr(7)));
  std::filesystem::path path = hub.resolve(uri);
  if (path.empty())
    path = std::string(uri);
  return path;
}

/** libcurl over the easy API: redirects followed, 20s timeout, HTTP
 *  errors (>= 400) fail, body lands in memory. */
struct NetFetcher {
  static size_t write(char *data, size_t size, size_t nmemb,
                      void *user) {
    auto *out = static_cast<std::vector<std::byte> *>(user);
    const auto *bytes = reinterpret_cast<const std::byte *>(data);
    out->insert(out->end(), bytes, bytes + size * nmemb);
    return size * nmemb;
  }

  static std::optional<std::vector<std::byte>> get(std::string_view url) {
    static const CURLcode globalInit =
        curl_global_init(CURL_GLOBAL_DEFAULT);
    if (globalInit != CURLE_OK)
      return std::nullopt;
    CURL *curl = curl_easy_init();
    if (!curl)
      return std::nullopt;
    const std::string urlString(url);
    std::vector<std::byte> body;
    curl_easy_setopt(curl, CURLOPT_URL, urlString.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "SigilLoader/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &NetFetcher::write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    const CURLcode code = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (code != CURLE_OK)
      return std::nullopt;
    return body;
  }
};

/** What one resource ask produced, wherever the bytes came from:
 *  `path` is the decode pathHint (the cache file for network URIs, so
 *  extension-based format hints keep working). */
struct FetchResult {
  std::shared_ptr<const Blob> blob;
  std::filesystem::path path;
  std::filesystem::file_time_type mtime{};
};

/** Network fetch behind the disk cache. CacheFirst: a present cache
 *  file is served without touching the network (offline-friendly).
 *  Refresh: the network goes first, the cache catches its failures.
 *  Offline: cache only. A fetch success always persists for the next
 *  run. */
FetchResult fetchNetwork(const std::filesystem::path &cacheDir,
                         std::string_view url, NetworkPolicy policy) {
  std::error_code ec;
  std::filesystem::create_directories(cacheDir, ec);
  const std::filesystem::path cached = cacheDir / networkCacheKey(url);
  const auto fromCache = [&]() -> FetchResult {
    if (std::filesystem::exists(cached, ec) && !ec)
      if (auto blob = readFile(cached))
        return {std::move(blob), cached, kNetworkMtime};
    return {};
  };
  if (policy != NetworkPolicy::Refresh)
    if (FetchResult hit = fromCache(); hit.blob)
      return hit;
  if (policy == NetworkPolicy::Offline)
    return {};
  auto body = NetFetcher::get(url);
  if (!body)
    return fromCache(); // Refresh degrades to the cached copy
  std::ofstream(cached, std::ios::binary)
      .write(reinterpret_cast<const char *>(body->data()),
             (std::streamsize)body->size()); // best-effort persist
  auto blob = std::make_shared<Blob>();
  blob->bytes = std::move(*body);
  return {std::move(blob), cached, kNetworkMtime};
}

/** The one preamble every accessor shares: network URI → NetFetcher
 *  (through the disk cache), anything else → mounted filesystem. */
FetchResult fetchResource(const Hub &hub,
                          const std::filesystem::path &netCacheDir,
                          NetworkPolicy netPolicy,
                          std::string_view uri) {
  if (isNetworkUri(uri)) {
    const std::filesystem::path cacheDir =
        netCacheDir.empty() ? std::filesystem::temp_directory_path() /
                                  "sigilloader-net-cache"
                            : netCacheDir;
    return fetchNetwork(cacheDir, uri, netPolicy);
  }
  std::filesystem::path path = localPath(hub, uri);
  std::error_code ec;
  const auto mtime = std::filesystem::last_write_time(path, ec);
  if (ec)
    return {};
  auto blob = readFile(path);
  if (!blob)
    return {};
  return {std::move(blob), std::move(path), mtime};
}

} // namespace

std::string networkCacheKey(std::string_view url) {
  const size_t hash = std::hash<std::string_view>{}(url);
  char name[2 * sizeof(hash) + 1];
  std::snprintf(name, sizeof(name), "%zx", hash);
  std::string key(name);
  // The URL path's extension rides along so pathHints keep working.
  const std::string_view path = url.substr(0, url.find_first_of("?#"));
  const size_t slash = path.rfind('/');
  const size_t dot = path.rfind('.');
  if (dot != std::string_view::npos &&
      (slash == std::string_view::npos || dot > slash) &&
      path.size() - dot <= 8)
    key += path.substr(dot);
  return key;
}

void Hub::mount(std::string prefix, std::filesystem::path dir) {
  for (auto &[existing, path] : m_mounts)
    if (existing == prefix) {
      path = std::move(dir);
      return;
    }
  m_mounts.emplace_back(std::move(prefix), std::move(dir));
}

std::filesystem::path Hub::resolve(std::string_view uri) const {
  const std::pair<std::string, std::filesystem::path> *best = nullptr;
  for (const auto &mountPair : m_mounts)
    if (uri.starts_with(mountPair.first) &&
        (!best || mountPair.first.size() > best->first.size()))
      best = &mountPair;
  if (best)
    return best->second / std::string(uri.substr(best->first.size()));
  return {};
}

void Hub::setNetworkCacheDir(std::filesystem::path dir) {
  m_netCacheDir = std::move(dir);
}

void Hub::setNetworkPolicy(NetworkPolicy policy) {
  m_netPolicy = policy;
}

std::string Hub::cacheKey(std::string_view uri,
                          const ImageOptions *options) {
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

std::shared_ptr<const Blob> Hub::blob(std::string_view uri) {
  const std::string key = cacheKey(uri, nullptr);
  auto it = m_entries.find(key);
  if (it != m_entries.end() && it->second.blob)
    return it->second.blob;
  FetchResult fetched =
      fetchResource(*this, m_netCacheDir, m_netPolicy, uri);
  if (!fetched.blob)
    return nullptr; // not cached: heals as soon as the file appears
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
  if (!bytes)
    return std::nullopt;
  return std::string(bytes->asText());
}

std::shared_ptr<const sigil::image::ImageAsset>
Hub::image(std::string_view uri, const ImageOptions &options) {
  const std::string key = cacheKey(uri, &options);
  auto it = m_entries.find(key);
  if (it != m_entries.end() && it->second.image)
    return it->second.image;
  // Bytes already cached by a blob() ask are decoded as they are —
  // one read serves every view of the entry; otherwise fetch fresh.
  std::shared_ptr<const Blob> bytes;
  std::filesystem::path path;
  FetchResult fetched;
  if (it != m_entries.end() && it->second.blob) {
    bytes = it->second.blob;
    path = it->second.path;
  } else {
    fetched = fetchResource(*this, m_netCacheDir, m_netPolicy, uri);
    if (!fetched.blob)
      return nullptr;
    bytes = fetched.blob;
    path = fetched.path;
  }
  auto decoded = sigil::image::decodeImage(
      bytes->bytes.data(), bytes->bytes.size(), options, path);
  if (!decoded)
    return nullptr;
  if (it == m_entries.end()) {
    it = m_entries.emplace(key, Entry{}).first;
    it->second.uri = std::string(uri);
    it->second.path = std::move(path);
    it->second.mtime = fetched.mtime;
  }
  // The encoded bytes are not kept unless blob() asked for them, so
  // an image-only workload never holds them alive beside the pixels.
  it->second.image = std::make_shared<sigil::image::ImageAsset>(
      std::move(*decoded));
  it->second.imageOptions = options;
  return it->second.image;
}

std::shared_ptr<const sigil::image::ChannelData>
Hub::channels(std::string_view uri) {
  const std::string key = cacheKey(uri, nullptr);
  auto it = m_entries.find(key);
  if (it != m_entries.end() && it->second.channels)
    return it->second.channels;
  std::shared_ptr<const Blob> bytes;
  std::filesystem::path path;
  FetchResult fetched;
  if (it != m_entries.end() && it->second.blob) {
    bytes = it->second.blob;
    path = it->second.path;
  } else {
    fetched = fetchResource(*this, m_netCacheDir, m_netPolicy, uri);
    if (!fetched.blob)
      return nullptr;
    bytes = fetched.blob;
    path = fetched.path;
  }
  auto decoded = sigil::image::decodeChannels(
      bytes->bytes.data(), bytes->bytes.size(), path);
  if (!decoded)
    return nullptr;
  if (it == m_entries.end()) {
    it = m_entries.emplace(key, Entry{}).first;
    it->second.uri = std::string(uri);
    it->second.path = std::move(path);
    it->second.mtime = fetched.mtime;
  }
  it->second.channels = std::make_shared<sigil::image::ChannelData>(
      std::move(*decoded));
  return it->second.channels;
}

std::optional<ResourceInfo> Hub::probe(std::string_view uri) const {
  FetchResult fetched =
      fetchResource(*this, m_netCacheDir, m_netPolicy, uri);
  if (!fetched.blob)
    return std::nullopt;
  if (auto probe = sigil::image::probeImage(fetched.blob->bytes.data(),
                                            fetched.blob->bytes.size(),
                                            fetched.path)) {
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
bool Hub::reload(Entry &entry) {
  const std::filesystem::path path = localPath(*this, entry.uri);
  auto bytes = readFile(path);
  if (!bytes)
    return false;
  std::shared_ptr<const sigil::image::ImageAsset> image;
  if (entry.image) {
    auto decoded = sigil::image::decodeImage(
        bytes->bytes.data(), bytes->bytes.size(), entry.imageOptions,
        path);
    if (!decoded)
      return false;
    image = std::make_shared<sigil::image::ImageAsset>(
        std::move(*decoded));
  }
  std::shared_ptr<const sigil::image::ChannelData> channels;
  if (entry.channels) {
    auto decoded = sigil::image::decodeChannels(
        bytes->bytes.data(), bytes->bytes.size(), path);
    if (!decoded)
      return false;
    channels = std::make_shared<sigil::image::ChannelData>(
        std::move(*decoded));
  }
  if (entry.image)
    entry.image = std::move(image);
  if (entry.channels)
    entry.channels = std::move(channels);
  if (entry.blob)
    entry.blob = std::move(bytes);
  entry.path = path;
  return true;
}

bool Hub::poll() {
  bool changed = false;
  for (auto it = m_entries.begin(); it != m_entries.end();) {
    Entry &entry = it->second;
    if (isNetworkUri(entry.uri)) {
      ++it; // no mtime to watch: network entries stay as fetched
      continue;
    }
    const std::filesystem::path path = localPath(*this, entry.uri);
    std::error_code ec;
    const auto mtime = std::filesystem::last_write_time(path, ec);
    if (ec) {
      it = m_entries.erase(it); // vanished: next ask sees the truth
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

} // namespace sigil::loader
