/** @file
 * Network fetch and the disk cache: libcurl over the easy API, the
 * policy that decides whether the cache or the network answers first,
 * the cache filename a URL maps to, and the hub's two network settings.
 */

#include "sigilio/hub/Network.h"

#include <curl/curl.h>

#include <cstdio>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "Fetch.h"
#include "Residency.h"

namespace sigil::io {

namespace detail {

namespace {

/** Network entries carry no mtime to watch; poll() skips this. */
constexpr auto kNetworkMtime = std::filesystem::file_time_type::min();

/** libcurl over the easy API: redirects followed, 20s timeout, HTTP
 *  errors (>= 400) fail, body lands in memory. */
struct NetFetcher {
  static size_t write(const char* data, size_t size, size_t nmemb, void* user) {
    auto* out = static_cast<std::vector<std::byte>*>(user);
    const auto* bytes = reinterpret_cast<const std::byte*>(data);
    out->insert(out->end(), bytes, bytes + size * nmemb);
    return size * nmemb;
  }

  static std::optional<std::vector<std::byte>> get(std::string_view url) {
    static const CURLcode globalInit = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (globalInit != CURLE_OK) return std::nullopt;
    CURL* curl = curl_easy_init();
    if (!curl) return std::nullopt;
    const std::string urlString(url);
    std::vector<std::byte> body;
    curl_easy_setopt(curl, CURLOPT_URL, urlString.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "SigilIO/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &NetFetcher::write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    const CURLcode code = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (code != CURLE_OK) return std::nullopt;
    return body;
  }
};

}  // namespace

bool isNetworkUri(std::string_view uri) {
  return uri.starts_with("http://") || uri.starts_with("https://");
}

/** Network fetch behind the disk cache. CacheFirst: a present cache
 *  file is served without touching the network (offline-friendly).
 *  Refresh: the network goes first, the cache catches its failures.
 *  Offline: cache only. A fetch success always persists for the next
 *  run. */
FetchResult fetchNetwork(const std::filesystem::path& cacheDir,
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
    if (FetchResult hit = fromCache(); hit.blob) return hit;
  if (policy == NetworkPolicy::Offline) return {};
  auto body = NetFetcher::get(url);
  if (!body) return fromCache();  // Refresh degrades to the cached copy
  std::ofstream(cached, std::ios::binary)
      .write(reinterpret_cast<const char*>(body->data()),
             (std::streamsize)body->size());  // best-effort persist
  auto blob = std::make_shared<Bytes>();
  blob->bytes = std::move(*body);
  return {std::move(blob), cached, kNetworkMtime};
}

}  // namespace detail

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

std::filesystem::path defaultNetworkCacheDir() {
  return std::filesystem::temp_directory_path() / "sigilio-net-cache";
}

void Hub::setNetworkCacheDir(std::filesystem::path dir) {
  const std::lock_guard lock(m_synchronization->mutex);
  m_netCacheDir = std::move(dir);
}

void Hub::setNetworkPolicy(NetworkPolicy policy) {
  const std::lock_guard lock(m_synchronization->mutex);
  m_netPolicy = policy;
}

}  // namespace sigil::io
