/** @file
 * Network fetch and the disk cache: libcurl over the easy API, the
 * policy that decides whether the cache or the network answers first,
 * the resource operations over that cache, and the hub's network settings.
 */

#include "sigilio/hub/Network.h"

#include <curl/curl.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "Fetch.h"
#include "sigilio/source/Sink.h"

namespace sigil::io {

namespace detail {

namespace {

/** Network entries carry no mtime to watch; poll() skips this. */
constexpr auto kNetworkMtime = std::filesystem::file_time_type::min();

/** libcurl over the easy API: redirects followed, 20s timeout, HTTP
 *  errors (>= 400) fail, body lands in memory. The transport a hub
 *  runs when it was given no other. */
struct CurlTransport {
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
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CurlTransport::write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    const CURLcode code = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (code != CURLE_OK) return std::nullopt;
    return body;
  }
};

}  // namespace

/** A name no other writer of this cache directory is using: the thread
 *  within the process, and the clock to tell two processes apart. */
static std::string writerSuffix() {
  std::ostringstream name;
  name << std::hex << std::hash<std::thread::id>{}(std::this_thread::get_id())
       << '.'
       << (unsigned long long)std::chrono::steady_clock::now()
              .time_since_epoch()
              .count();
  return name.str();
}

bool isNetworkUri(std::string_view uri) {
  return uri.starts_with("http://") || uri.starts_with("https://");
}

static std::filesystem::path networkCachePath(
    std::string_view url, const std::filesystem::path& cacheDir) {
  return (cacheDir.empty() ? defaultNetworkCacheDir() : cacheDir) /
         networkCacheKey(url);
}

/** A write publishes only when every byte is there. A sibling belongs to
 *  one writer, so concurrent fetches and seeds cannot truncate each other's
 *  bytes before either takes the resource's cache name. */
static bool persistNetworkResource(const std::filesystem::path& cached,
                                   std::span<const std::byte> bytes) {
  const std::filesystem::path partial =
      cached.string() + ".part." + writerSuffix();
  std::error_code ec;
  if (writeBytes(partial, bytes.data(), bytes.size())) {
    std::filesystem::rename(partial, cached, ec);
    if (!ec) return true;
  }
  std::filesystem::remove(partial, ec);
  return false;
}

/** Network fetch behind the disk cache. CacheFirst: a present cache
 *  file is served without touching the network (offline-friendly).
 *  Refresh: the network goes first, the cache catches its failures.
 *  Offline: cache only. A fetch success always persists for the next
 *  run. */
FetchResult fetchNetwork(const NetworkAccess& access, std::string_view url) {
  const std::filesystem::path cached = networkCachePath(url, access.cacheDir);
  std::error_code ec;
  const auto fromCache = [&]() -> FetchResult {
    if (std::filesystem::exists(cached, ec) && !ec)
      if (auto blob = readFile(cached))
        return {std::move(blob), cached, kNetworkMtime};
    return {};
  };
  if (access.policy != NetworkPolicy::Refresh)
    if (FetchResult hit = fromCache(); hit.blob) return hit;
  if (access.policy == NetworkPolicy::Offline) return {};
  auto body =
      access.transport ? access.transport(url) : CurlTransport::get(url);
  if (!body) return fromCache();  // Refresh degrades to the cached copy
  // Persistence is best-effort: a fetched resource remains usable when
  // the cache directory cannot accept it.
  (void)persistNetworkResource(cached, *body);
  auto blob = std::make_shared<Bytes>();
  blob->bytes = std::move(*body);
  return {std::move(blob), cached, kNetworkMtime};
}

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

namespace {

/** The directory this platform keeps caches under, or nothing when the
 *  platform names none: caches are the OS's to evict, and a cache that
 *  a lane depends on has to outlive the temp directory's policy, which
 *  on macOS deletes anything untouched for three days. Resolved from
 *  the environment rather than from a UI toolkit, because SigilIO is
 *  below every toolkit. */
std::filesystem::path platformCacheRoot() {
  const auto fromEnv = [](const char* name) -> const char* {
    const char* value = std::getenv(name);
    return (value && *value) ? value : nullptr;
  };
#if defined(_WIN32)
  if (const char* local = fromEnv("LOCALAPPDATA")) return local;
#elif defined(__APPLE__)
  if (const char* home = fromEnv("HOME"))
    return std::filesystem::path(home) / "Library" / "Caches";
#else
  if (const char* xdg = fromEnv("XDG_CACHE_HOME")) return xdg;
  if (const char* home = fromEnv("HOME"))
    return std::filesystem::path(home) / ".cache";
#endif
  return {};
}

}  // namespace

std::filesystem::path defaultNetworkCacheDir() {
  const std::filesystem::path root = platformCacheRoot();
  const std::filesystem::path base =
      root.empty() ? std::filesystem::temp_directory_path() : root;
  return base / "SigilIO" / "network";
}

}  // namespace detail

std::optional<std::uintmax_t> probeNetworkCache(
    std::string_view url, const std::filesystem::path& cacheDir) {
  if (!detail::isNetworkUri(url)) return std::nullopt;
  const std::filesystem::path cached = detail::networkCachePath(url, cacheDir);
  std::error_code ec;
  if (!std::filesystem::is_regular_file(cached, ec) || ec) return std::nullopt;
  const std::uintmax_t size = std::filesystem::file_size(cached, ec);
  return ec ? std::nullopt : std::optional<std::uintmax_t>(size);
}

bool seedNetworkCache(std::string_view url, std::span<const std::byte> bytes,
                      const std::filesystem::path& cacheDir) {
  if (!detail::isNetworkUri(url)) return false;
  return detail::persistNetworkResource(detail::networkCachePath(url, cacheDir),
                                        bytes);
}

void Hub::setNetworkCacheDir(std::filesystem::path dir) {
  const std::lock_guard lock(m_mutex);
  m_networkCacheDir = std::move(dir);
}

void Hub::setNetworkPolicy(NetworkPolicy policy) {
  const std::lock_guard lock(m_mutex);
  m_networkPolicy = policy;
}

void Hub::setNetworkTransport(NetworkTransport transport) {
  const std::lock_guard lock(m_mutex);
  m_networkTransport = std::move(transport);
}

}  // namespace sigil::io
