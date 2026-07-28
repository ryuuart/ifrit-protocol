#pragma once
// SigilLoader — the runtime resource system: game-engine-style mounted
// URIs over pluggable decode backends.
//
// A Hub maps URI prefixes to providers (today: filesystem directories;
// the scheme leaves room for pack files and in-memory providers), so
// application code asks for "res://ui/logo.png" and never touches the
// filesystem again. Resources are cached, hot-reloadable (poll()
// re-checks everything previously loaded), and typed:
//
//   hub.mount("res://", assetsDir);
//   auto bytes = hub.blob("res://data/table.bin");
//   auto text  = hub.text("res://shaders/glow.sksl");
//   auto img   = hub.image("res://ui/logo.png");            // stills+anim
//   auto hdr   = hub.image("res://light/probe.exr",         // OIIO: EXR,
//                          {.layer = "diffuse"});           //  PSD, TIFF…
//   auto info  = hub.probe("res://light/probe.exr");        // metadata
//
// http:// and https:// URIs bypass mounts and fetch over the network
// (libcurl: redirects followed, 20s timeout, HTTP errors fail).
// Successful fetches persist in an on-disk cache (temp dir /
// "sigilloader-net-cache"; override via setNetworkCacheDir — point it
// at an asset dir to make downloads survive reboots). Under the
// default CacheFirst policy a cache hit never touches the network, so
// offline runs keep working with no flag to set; setNetworkPolicy
// picks Refresh (network first, cache as the fallback) or Offline
// (cache only) when a host wants the explicit behavior. file:// URIs
// strip to plain local paths. poll() skips network entries: they
// carry no mtime to watch.
//
// The loader owns ACCESS: where bytes come from, caching, reload.
// What pixels mean is SigilImage's concern (sigilimage/Decode.h) — the
// Skia codecs plus, when built in, the OpenImageIO backend (EXR with
// layer selection, PSD, TIFF, HDR; float sources land as RGBA_F32).

#include <sigilimage/Decode.h>

#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::loader {

/** Raw bytes of a resource. */
struct Blob {
  std::vector<std::byte> bytes;

  std::string_view asText() const {
    return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
  }
};

/** What a resource is, before (or without) fully decoding it. */
struct ResourceInfo {
  enum class Kind { Data, Image };
  Kind kind = Kind::Data;
  std::uintmax_t byteSize = 0;
  std::string format; // "png", "openexr", "psd", … (decoder's name)

  /** Image metadata (kind == Image); see sigil::image::ImageProbe. */
  sigil::image::ImageProbe image;
};

/** Image decode options come from SigilImage — the loader adds no
 *  format knowledge of its own. */
using ImageOptions = sigil::image::DecodeOptions;

/** When the hub may touch the network for http(s):// URIs. The policy
 *  applies when a resource is first asked for (or asked again after
 *  its entry was dropped); already-loaded entries stay as loaded. */
enum class NetworkPolicy {
  /** Default: a present cache file is served without any network
   *  traffic; a miss fetches and persists. Offline-safe out of the
   *  box once a resource has been seen. */
  CacheFirst,
  /** Ask the network first (pick up upstream changes); a failed fetch
   *  falls back to the cached copy, so flaky networks degrade to
   *  CacheFirst instead of failing. */
  Refresh,
  /** Never touch the network: cache hit or fail. For hermetic runs
   *  and guaranteed-offline hosts. */
  Offline,
};

/** The on-disk cache filename (no directory) a network URL maps to:
 *  hex of the URL's hash plus the URL path's extension, so decode
 *  pathHints keep working. Exposed for tests and cache pre-seeding. */
std::string networkCacheKey(std::string_view url);

/**
 * The resource hub: mount prefixes, ask for resources by URI.
 *
 * Loaded resources are cached per (uri, options); poll() re-stats every
 * previously requested resource and reloads the changed ones, returning
 * true so hosts can re-render (holders of old shared_ptrs keep the old
 * data — swap by re-asking). Failed lookups are NOT cached: a missing
 * file loads as soon as it appears.
 */
class Hub {
public:
  Hub() = default;

  /** Maps every URI starting with `prefix` to files under `dir`
   *  ("res://" + "ui/logo.png" → dir/ui/logo.png). Longest matching
   *  prefix wins; re-mounting a prefix replaces it. */
  void mount(std::string prefix, std::filesystem::path dir);

  /** The mounted filesystem path a URI resolves to (empty when no
   *  mount matches — the URI is then tried as a plain path). */
  std::filesystem::path resolve(std::string_view uri) const;

  /** Where network fetches persist (default: temp dir /
   *  "sigilloader-net-cache"). Files land under networkCacheKey(url);
   *  a present file is served without touching the network. */
  void setNetworkCacheDir(std::filesystem::path dir);

  /** How http(s):// asks may use the network (default: CacheFirst). */
  void setNetworkPolicy(NetworkPolicy policy);

  /** Raw bytes; null when unresolvable/unreadable. */
  std::shared_ptr<const Blob> blob(std::string_view uri);

  /** UTF-8 text convenience over blob(). */
  std::optional<std::string> text(std::string_view uri);

  /** Decoded image (stills and animations); null on failure. */
  std::shared_ptr<const sigil::image::ImageAsset>
  image(std::string_view uri, const ImageOptions &options = {});

  /** The raw decoded color data — every channel the source carries
   *  (EXR layers included) as named float planes; null on failure.
   *  See sigil::image::ChannelData for Skia composition helpers. */
  std::shared_ptr<const sigil::image::ChannelData>
  channels(std::string_view uri);

  /** Metadata without a full decode (dimensions, channels, layers,
   *  float-ness, animation frames); nullopt when unreadable. */
  std::optional<ResourceInfo> probe(std::string_view uri) const;

  /** Re-checks every previously loaded resource; reloads changes and
   *  drops entries whose files vanished. Returns true if anything
   *  changed. */
  bool poll();

private:
  struct Entry {
    std::shared_ptr<const Blob> blob;
    std::shared_ptr<const sigil::image::ImageAsset> image;
    std::shared_ptr<const sigil::image::ChannelData> channels;
    ImageOptions imageOptions;
    std::filesystem::file_time_type mtime{};
  };

  bool reload(const std::string &key, Entry &entry);
  static std::string cacheKey(std::string_view uri,
                              const ImageOptions *options);

  std::vector<std::pair<std::string, std::filesystem::path>> m_mounts;
  std::map<std::string, Entry, std::less<>> m_entries;
  std::filesystem::path m_netCacheDir; // empty = the default temp dir
  NetworkPolicy m_netPolicy = NetworkPolicy::CacheFirst;
};

} // namespace sigil::loader
