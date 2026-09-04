#pragma once

/** @file
 * The resource hub: game-engine-style mounted URIs over pluggable
 * decode backends.
 *
 * A Hub maps URI prefixes to providers (today: filesystem directories;
 * the scheme leaves room for pack files and in-memory providers), so
 * application code asks for "res://ui/logo.png" and never touches the
 * filesystem again. Resources are cached, hot-reloadable (poll()
 * re-checks everything previously loaded), and typed:
 *
 *   hub.mount("res://", assetsDir);
 *   auto bytes = hub.blob("res://data/table.bin");
 *   auto text  = hub.text("res://shaders/glow.sksl");
 *   auto img   = hub.image("res://ui/logo.png");            // stills+anim
 *   auto hdr   = hub.image("res://light/probe.exr",         // OIIO: EXR,
 *                          {.layer = "diffuse"});           //  PSD, TIFF…
 *   auto info  = hub.probe("res://light/probe.exr");        // metadata
 *   hub.registerDecoder<Mesh>(parseMesh);                   // any T
 *   auto mesh  = hub.load<Mesh>("res://props/crate.obj");
 *
 * http:// and https:// URIs bypass mounts and fetch over the network
 * (libcurl: redirects followed, 20s timeout, HTTP errors fail).
 * Successful fetches persist in an on-disk cache (temp dir /
 * "sigilio-net-cache"; override via setNetworkCacheDir — point it
 * at an asset dir to make downloads survive reboots). Under the
 * default CacheFirst policy a cache hit never touches the network, so
 * offline runs keep working with no flag to set; setNetworkPolicy
 * picks Refresh (network first, cache as the fallback) or Offline
 * (cache only) when a host wants the explicit behavior. file:// URIs
 * strip to plain local paths. poll() skips network entries: they
 * carry no mtime to watch.
 *
 * SigilIO owns ACCESS: where bytes come from, caching, reload. A Hub
 * is a ByteSource: fetch() answers a URI with bytes, and every typed
 * view is a registered Decoder run over those bytes. What pixels mean
 * is SigilImage's concern — the Skia codecs plus, when built in, the
 * OpenImageIO backend (EXR with layer selection, PSD, TIFF, HDR; float
 * sources land as RGBA_F32) — and the hub registers those decoders by
 * default.
 */

#include <sigilimage/decode/Decode.h>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <typeindex>
#include <utility>
#include <vector>

#include "sigilio/hub/Network.h"
#include "sigilio/source/Sink.h"
#include "sigilio/source/Source.h"

namespace sigil::io {

namespace detail {
struct Residency;
struct Synchronization;
}  // namespace detail

class Hub;

/** A movable lease that keeps an inspectable set of resource URIs resident.
 *
 * Selectors are snapshots. include() adds a selector and immediately refreshes
 * the union; refresh() reruns every selector so newly created files join and
 * vanished files leave. Multiple leases may retain the same URI independently.
 * Destroying a lease releases only its own claim. The Hub must outlive calls on
 * its leases, but a lease may be destroyed safely after its Hub. */
class ResourceLease {
 public:
  ResourceLease() = default;
  ~ResourceLease();

  ResourceLease(ResourceLease&& other) noexcept;
  ResourceLease& operator=(ResourceLease&& other) noexcept;
  ResourceLease(const ResourceLease&) = delete;
  ResourceLease& operator=(const ResourceLease&) = delete;

  /** Adds @p selector and refreshes the retained union. Returns its new size.
   */
  size_t include(std::string_view selector);

  /** Reruns every included selector and updates the retained URI snapshot. */
  size_t refresh();

  /** Loads the retained resources into their Hub's byte cache concurrently. */
  size_t preload();

  /** The sorted, duplicate-free URI snapshot this lease currently retains. */
  std::span<const std::string> uris() const { return m_uris; }

 private:
  friend class Hub;
  ResourceLease(Hub& hub, std::shared_ptr<detail::Residency> residency,
                std::vector<std::string> selectors);

  void release();

  Hub* m_hub = nullptr;
  std::weak_ptr<detail::Residency> m_residency;
  std::vector<std::string> m_selectors;
  std::vector<std::string> m_uris;
};

/** What a resource is, before (or without) fully decoding it. */
struct ResourceInfo {
  enum class Kind { Data, Image };
  Kind kind = Kind::Data;
  std::uintmax_t byteSize = 0;
  std::string format;  // "png", "openexr", "psd", … (decoder's name)

  /** Image metadata (kind == Image); see sigil::image::ImageProbe. */
  sigil::image::ImageProbe image;
};

/**
 * The resource hub: mount prefixes, ask for resources by URI.
 *
 * Each URI is cached as one entry whose blob and decoded views are
 * independent: each populates the first time its accessor is asked, and
 * asking for one never affects another. A view is one decoded type —
 * image(), channels() and load<T>() each populate their own — and an
 * image() ask with a layer or an explicit size is a different decode
 * that gets its own entry. poll() re-stats every previously requested
 * resource and reloads the changed ones, returning true so hosts can
 * re-render (holders of old shared_ptrs keep the old data — swap by
 * re-asking). Failed lookups are NOT cached: a missing file loads as
 * soon as it appears.
 *
 * Calls on one Hub may overlap: mount, decoder, cache and retention state are
 * synchronized internally. A Hub satisfies ByteSource and
 * ResolvingByteSource.
 */
class Hub {
 public:
  /** Registers the SigilImage decoders: ImageAsset (the routed decode
   *  at default options) and ChannelData. */
  Hub();
  ~Hub();

  Hub(const Hub&) = delete;
  Hub& operator=(const Hub&) = delete;
  Hub(Hub&&) = delete;
  Hub& operator=(Hub&&) = delete;

  /** Maps every URI starting with `prefix` to files under `dir`
   *  ("res://" + "ui/logo.png" → dir/ui/logo.png). Longest matching
   *  prefix wins; re-mounting a prefix replaces it. */
  void mount(std::string prefix, std::filesystem::path dir);

  /** The mounted filesystem path a URI resolves to (empty when no
   *  mount matches — the URI is then tried as a plain path). */
  std::filesystem::path resolve(std::string_view uri) const;

  /** Where network fetches persist (default: temp dir /
   *  "sigilio-net-cache"). Files land under networkCacheKey(url);
   *  a present file is served without touching the network. */
  void setNetworkCacheDir(std::filesystem::path dir);

  /** How http(s):// asks may use the network (default: CacheFirst). */
  void setNetworkPolicy(NetworkPolicy policy);

  /** Raw bytes; null when unresolvable/unreadable. Never decodes:
   *  bytes load and cache whether or not any decoder accepts them. */
  std::shared_ptr<const Bytes> blob(std::string_view uri);

  /** The ByteSource spelling of blob(): the same bytes, the same cache
   *  entry. */
  std::shared_ptr<const Bytes> fetch(std::string_view uri) { return blob(uri); }

  /** Stores @p size bytes under @p uri, through the same mount table a
   *  read resolves by, creating the directories above the file. What
   *  the bytes MEAN is nobody's business here: a caller with an image
   *  encodes it first and hands the result over.
   *
   *  Every cached view of that URI is dropped, so the next ask reads
   *  the file back rather than serving what was there before the write.
   *  A network URI cannot be written and answers false — a hub writes
   *  where it mounts. */
  bool write(std::string_view uri, const void* bytes, size_t size);
  bool write(std::string_view uri, const Bytes& bytes) {
    return write(uri, bytes.bytes.data(), bytes.bytes.size());
  }

  /** Registers how a T is decoded from bytes, so load<T>() can answer.
   *  `hint` is the resource's local path when it has one (the disk
   *  cache file for a network URI). Replaces any decoder already
   *  registered for T; views already decoded keep their values until
   *  poll() reloads them through the new decoder. ImageAsset and
   *  ChannelData are registered by the constructor. */
  template <typename T>
  void registerDecoder(
      std::function<std::optional<T>(const Bytes&, std::string_view hint)>
          decode) {
    setDecoder(
        std::type_index(typeid(T)),
        [decode = std::move(decode)](
            const Bytes& bytes,
            const std::filesystem::path& path) -> std::shared_ptr<const void> {
          auto value = decode(bytes, path.native());
          if (!value) return nullptr;
          return std::make_shared<const T>(std::move(*value));
        });
  }

  /** The same, from any object satisfying the Decoder concept. */
  template <typename T, Decoder<T> D>
  void registerDecoder(D decoder) {
    registerDecoder<T>([decoder = std::move(decoder)](const Bytes& bytes,
                                                      std::string_view hint) {
      return decoder.decode(bytes, hint);
    });
  }

  /** The resource decoded as a T through the decoder registered for T;
   *  null on failure, and null (with no fetch) when no decoder is
   *  registered for T. Decodes on the first ask, from bytes a prior
   *  blob() ask already cached when they are present, and caches the
   *  result as one view of the URI's entry. load<ImageAsset>(uri) is
   *  image(uri) and shares its view. */
  template <typename T>
  std::shared_ptr<const T> load(std::string_view uri) {
    return std::static_pointer_cast<const T>(loadRegisteredView(
        cacheKey(uri, nullptr), uri, std::type_index(typeid(T))));
  }

  /** UTF-8 text convenience over blob(). */
  std::optional<std::string> text(std::string_view uri);

  /** The regular-file URIs named by @p selector, in lexical order.
   *
   *  An exact file selects itself. A directory URI selects every regular file
   *  below it recursively. In a glob, `*` matches within one path segment,
   *  `?` matches one non-separator character, and `**` crosses `/`; a
   *  backslash quotes the next character. Selection enumerates local
   *  filesystem resources (mounted URIs, file:// URLs and plain paths) without
   *  reading file contents. A network selector without a star is one exact URL
   *  and selects itself without a fetch; network globs cannot be enumerated. */
  std::vector<std::string> select(std::string_view selector) const;

  /** Fetches the distinct @p uris concurrently into the byte cache and
   *  returns how many are ready. No decoding is performed. */
  size_t preload(std::span<const std::string_view> uris);

  /** Selects @p selector and concurrently fetches the resulting files. */
  size_t preload(std::string_view selector);

  /** Recursively selects and preloads a directory URI. */
  size_t preloadDirectory(std::string_view uriPrefix);

  /** An empty resource-retention lease bound to this Hub. */
  ResourceLease retain();

  /** A lease retaining the current files selected by @p selector. */
  ResourceLease retain(std::string_view selector);

  /** A lease retaining the union of the current selector snapshots. */
  ResourceLease retain(std::span<const std::string_view> selectors);
  ResourceLease retain(std::initializer_list<std::string_view> selectors);

  /** Discards every cached entry not protected by a resource lease. Values
   *  already returned in shared_ptrs stay alive for their holders. Returns the
   *  number of cache entries discarded. */
  size_t discardUnretained();

  /** Decoded image (stills and animations); null on failure. Decodes
   *  on this first ask, from bytes a prior blob() ask already cached
   *  when they are present (no second read of the source). At default
   *  options this is the ImageAsset decoder registered on the hub, so
   *  it answers whatever load<ImageAsset>() answers; with a layer or
   *  size named it is its own decode in its own entry. */
  std::shared_ptr<const sigil::image::ImageAsset> image(
      std::string_view uri, const image::DecodeOptions& options = {});

  /** The raw decoded color data — every channel the source carries
   *  (EXR layers included) as named float planes; null on failure.
   *  See sigil::image::ChannelData for Skia composition helpers. */
  std::shared_ptr<const sigil::image::ChannelData> channels(
      std::string_view uri);

  /** Metadata without a full decode (dimensions, channels, layers,
   *  float-ness, animation frames); nullopt when unreadable.
   *
   *  const but neither cheap nor side-effect-free: every call performs
   *  a full fetch of the resource and caches nothing in the hub. For a
   *  network URI that can mean a network round trip and a write into
   *  the disk cache directory. */
  std::optional<ResourceInfo> probe(std::string_view uri) const;

  /** Re-checks every previously loaded resource; reloads changes and
   *  drops entries whose files vanished. Returns true if anything
   *  changed. */
  bool poll();

 private:
  /** Re-decodes bytes into a type-erased value; null on failure. The
   *  decode a view was made with rides along with the view, so poll()
   *  can re-run exactly it. */
  using Redecode = std::function<std::shared_ptr<const void>(
      const Bytes&, const std::filesystem::path&)>;

  /** One decoded view of an entry: the value, and how to make it again
   *  from fresh bytes. */
  struct View {
    std::shared_ptr<const void> value;
    Redecode decode;
  };

  /** One cached resource. The blob and each decoded view are
   *  independent, each populated the first time its accessor asks;
   *  asking for bytes never decodes, and decoding never drops bytes
   *  already served. An image decoded with a layer or explicit size
   *  lives in its own entry (see cacheKey).
   *
   *  `uri` is the original request string. reload() and poll() use it
   *  directly — a URI is never recovered by parsing a map key, so no
   *  character a URI may contain can confuse them. `path` is where
   *  the bytes came from (for http(s) URIs, the disk-cache file) and
   *  doubles as the decode format hint.
   *
   *  `mtime` is what poll() compares. It is written when the entry is
   *  created and when poll() reloads; an accessor that fetches fresh
   *  bytes into an existing entry leaves it alone, so a file that
   *  changed between two asks shows a stale stamp and the next poll()
   *  re-decodes every populated view from one read, bringing the
   *  views back into agreement. */
  struct Entry {
    std::string uri;
    std::shared_ptr<const Bytes> blob;
    std::map<std::type_index, View> views;
    std::filesystem::path path;
    std::filesystem::file_time_type mtime;
  };

  bool reload(Entry& entry);

  /** The one decode path every typed accessor shares: the view of
   *  `type` in the entry at `key`, decoded with `decode` from cached or
   *  freshly fetched bytes. Null when `decode` is empty, when the fetch
   *  fails, or when the decode does. */
  std::shared_ptr<const void> loadView(const std::string& key,
                                       std::string_view uri,
                                       std::type_index type,
                                       const Redecode& decode);

  /** The registered-decoder path. A populated view returns under one cache
   *  lock; only a miss copies the decoder and enters loadView(). */
  std::shared_ptr<const void> loadRegisteredView(const std::string& key,
                                                 std::string_view uri,
                                                 std::type_index type);

  /** The decoder registered for `type`, or an empty function. */
  Redecode registeredDecoder(std::type_index type) const;
  void setDecoder(std::type_index type, Redecode decode);

  std::vector<std::pair<std::string, std::filesystem::path>>
  mountedDirectories() const;
  std::shared_ptr<detail::Residency> residency();

  /** The map key for an ask: the URI alone for blob()/text()/
   *  channels() and default-options image(); with a layer or size
   *  set, the URI plus each option behind a '\0' separator — a byte
   *  no URI that names a real resource can contain, so option
   *  suffixes never collide with URI content. Keys are write-only:
   *  nothing parses one back (entries carry their own uri). */
  static std::string cacheKey(std::string_view uri,
                              const image::DecodeOptions* options);

  std::unique_ptr<detail::Synchronization> m_synchronization;
  std::vector<std::pair<std::string, std::filesystem::path>> m_mounts;
  std::map<std::string, Entry, std::less<>> m_entries;
  std::map<std::type_index, Redecode> m_decoders;
  std::filesystem::path m_netCacheDir;  // empty = the default temp dir
  NetworkPolicy m_netPolicy = NetworkPolicy::CacheFirst;
  std::shared_ptr<detail::Residency> m_residency;
};

}  // namespace sigil::io
