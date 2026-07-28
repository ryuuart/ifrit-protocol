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
  if (options && !options->layer.empty()) {
    key += "#layer=";
    key += options->layer;
  }
  if (options && (options->width || options->height)) {
    key += "#size=";
    key += std::to_string(options->width);
    key += "x";
    key += std::to_string(options->height);
  }
  return key;
}

std::shared_ptr<const Blob> Hub::blob(std::string_view uri) {
  const std::string key = cacheKey(uri, nullptr);
  if (auto it = m_entries.find(key); it != m_entries.end())
    return it->second.blob;
  FetchResult fetched =
      fetchResource(*this, m_netCacheDir, m_netPolicy, uri);
  if (!fetched.blob)
    return nullptr; // not cached: heals as soon as the file appears
  Entry entry;
  entry.blob = std::move(fetched.blob);
  entry.mtime = fetched.mtime;
  return m_entries.emplace(key, std::move(entry)).first->second.blob;
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
  if (auto it = m_entries.find(key); it != m_entries.end())
    return it->second.image;
  FetchResult fetched =
      fetchResource(*this, m_netCacheDir, m_netPolicy, uri);
  if (!fetched.blob)
    return nullptr;
  auto decoded = sigil::image::decodeImage(fetched.blob->bytes.data(),
                                           fetched.blob->bytes.size(),
                                           options, fetched.path);
  if (!decoded)
    return nullptr;
  Entry entry;
  entry.image = std::make_shared<sigil::image::ImageAsset>(
      std::move(*decoded));
  entry.imageOptions = options;
  entry.mtime = fetched.mtime;
  return m_entries.emplace(key, std::move(entry)).first->second.image;
}

std::shared_ptr<const sigil::image::ChannelData>
Hub::channels(std::string_view uri) {
  const std::string key = std::string(uri) + "#channels";
  if (auto it = m_entries.find(key); it != m_entries.end())
    return it->second.channels;
  FetchResult fetched =
      fetchResource(*this, m_netCacheDir, m_netPolicy, uri);
  if (!fetched.blob)
    return nullptr;
  auto decoded = sigil::image::decodeChannels(
      fetched.blob->bytes.data(), fetched.blob->bytes.size(),
      fetched.path);
  if (!decoded)
    return nullptr;
  Entry entry;
  entry.channels = std::make_shared<sigil::image::ChannelData>(
      std::move(*decoded));
  entry.mtime = fetched.mtime;
  return m_entries.emplace(key, std::move(entry))
      .first->second.channels;
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

bool Hub::reload(const std::string &key, Entry &entry) {
  // The key embeds the uri (and options); recover the uri part.
  const size_t hash = key.find('#');
  const std::string uri = key.substr(0, hash);
  const std::filesystem::path path = localPath(*this, uri);
  auto bytes = readFile(path);
  if (!bytes)
    return false;
  if (entry.image) {
    auto decoded = sigil::image::decodeImage(
        bytes->bytes.data(), bytes->bytes.size(), entry.imageOptions,
        path);
    if (!decoded)
      return false;
    entry.image = std::make_shared<sigil::image::ImageAsset>(
        std::move(*decoded));
  } else if (entry.channels) {
    auto decoded = sigil::image::decodeChannels(
        bytes->bytes.data(), bytes->bytes.size(), path);
    if (!decoded)
      return false;
    entry.channels = std::make_shared<sigil::image::ChannelData>(
        std::move(*decoded));
  } else {
    entry.blob = std::move(bytes);
  }
  return true;
}

bool Hub::poll() {
  bool changed = false;
  for (auto it = m_entries.begin(); it != m_entries.end();) {
    const std::string uri =
        it->first.substr(0, it->first.find('#'));
    if (isNetworkUri(uri)) {
      ++it; // no mtime to watch: network entries stay as fetched
      continue;
    }
    const std::filesystem::path path = localPath(*this, uri);
    std::error_code ec;
    const auto mtime = std::filesystem::last_write_time(path, ec);
    if (ec) {
      it = m_entries.erase(it); // vanished: next ask sees the truth
      changed = true;
      continue;
    }
    if (mtime != it->second.mtime && reload(it->first, it->second)) {
      it->second.mtime = mtime;
      changed = true;
    }
    ++it;
  }
  return changed;
}

} // namespace sigil::loader
