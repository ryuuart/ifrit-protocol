#pragma once

/** @file
 * @ingroup io-hub
 * The resource hub: game-engine-style mounted URIs over pluggable
 * decode backends. A Hub maps URI prefixes onto directories, so
 * application code asks for "res://ui/logo.png" and never touches the
 * filesystem again; results are cached per resource, poll() re-stats
 * what has been loaded, and every typed view is a registered decoder
 * run over the bytes it fetched. http(s):// bypasses the mounts and
 * fetches over the network behind an on-disk cache. A resource that
 * keeps ARRIVING is a feed rather than a fetch, and the hub is the door
 * on that too.
 */

#include <sigilcore/callable/Callable.h>
#include <sigilimage/decode/Decode.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <typeindex>
#include <utility>
#include <vector>

#include "sigilio/hub/Feed.h"
#include "sigilio/hub/Network.h"
#include "sigilio/source/Sink.h"
#include "sigilio/source/Source.h"

namespace sigil::io {

namespace detail {
struct Residency;
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

  /** Takes over @p other's claim, leaving it holding nothing. */
  ResourceLease(ResourceLease&& other) noexcept;
  /** Takes over @p other's claim, releasing this one's. */
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

/** A movable lease that keeps a callback on the hub's dispatch: the
 *  lease holds the callback, and releasing it or destroying it takes the
 *  callback off the hub. A lease may outlive its Hub, having then
 *  nothing left to unregister from. */
class DispatchLease {
 public:
  /** What a dispatch hands a callback: the seconds it was given, which
   *  are the seconds every replayed recording was just advanced to. */
  using Callback = std::function<void(double seconds)>;

  DispatchLease() = default;
  /** Takes over the moved-from lease's registration. */
  DispatchLease(DispatchLease&&) noexcept = default;
  /** Takes over the moved-from lease's registration, dropping this
   *  one's. */
  DispatchLease& operator=(DispatchLease&&) noexcept = default;
  DispatchLease(const DispatchLease&) = delete;
  DispatchLease& operator=(const DispatchLease&) = delete;

  /** Takes the callback off the hub, which destroying the lease does
   *  anyway. A dispatch that is already running its callbacks runs this
   *  one out: it holds what it is running. */
  void release() { m_callback.reset(); }

  /** Whether a callback still stands on the hub through this lease. */
  bool registered() const { return m_callback != nullptr; }

 private:
  friend class Hub;
  explicit DispatchLease(std::shared_ptr<Callback> callback)
      : m_callback(std::move(callback)) {}

  /** The one owner of the callback. The hub knows it weakly, so a lease
   *  that is gone leaves nothing to run. */
  std::shared_ptr<Callback> m_callback;
};

/** WHERE A RESOURCE'S BYTES ARE AND HOW MANY OF THEM THERE ARE — the
 *  whole of what a hub can say about a resource without deciding what
 *  its bytes mean. What they mean is `probe<T>()`, answered by the
 *  library that owns T. */
struct ResourceInfo {
  std::uintmax_t byteSize = 0;
  /** The local file the bytes were read from — the cache file for a
   *  network URI — or empty when they came from no file. It is also
   *  the name a prober takes as its format hint. */
  std::filesystem::path path;
};

/**
 * The resource hub: mount prefixes, ask for resources by URI.
 *
 * Each URI is cached as one entry whose bytes and decoded views are
 * independent, each populated the first time its accessor is asked, and
 * an image() ask with a layer or an explicit size is a different decode
 * in an entry of its own. A failed lookup is NOT cached: a missing file
 * loads as soon as it appears. Calls on one Hub may overlap — mount,
 * decoder, cache and retention state are synchronized internally — and
 * a Hub satisfies ByteSource and ResolvingByteSource.
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

  /** Where network fetches persist (default: the platform cache
   *  location / "SigilIO/network", the temp directory only where the
   *  platform names no cache location). A present resource is served
   *  without touching the network. */
  void setNetworkCacheDirectory(std::filesystem::path directory);

  /** How http(s):// asks may use the network (default: CacheFirst). */
  void setNetworkPolicy(NetworkPolicy policy);

  /** What answers an http(s):// URL with its body (default: libcurl). A
   *  host with its own HTTP stack, or a test that needs a fetch to fail
   *  without touching a resolver, hands one in; an empty function
   *  restores libcurl. The disk cache and the policy stay in front of
   *  whichever transport is set. */
  void setNetworkTransport(NetworkTransport transport);

  /** Raw bytes; null when unresolvable/unreadable. Never decodes:
   *  bytes load and cache whether or not any decoder accepts them. This
   *  is the ByteSource spelling, and the hub's only one — asking for
   *  bytes has one name here and in every other source. */
  std::shared_ptr<const Bytes> fetch(std::string_view uri);

  /** Stores @p size bytes under @p uri, through the same mount table a
   *  read resolves by, creating the directories above the file. What the
   *  bytes MEAN is nobody's business here. Every cached view of that URI
   *  is dropped, so the next ask reads the file back.
   *  @trap A network URI cannot be written and answers false — a hub
   *  writes where it mounts. */
  bool write(std::string_view uri, const void* bytes, size_t size);
  /** The same, from a bytes value already in hand. */
  bool write(std::string_view uri, const Bytes& bytes) {
    return write(uri, bytes.bytes.data(), bytes.bytes.size());
  }

  /** Registers how a T is decoded from bytes, so load<T>() can answer.
   *  `hint` is the resource's local path when it has one, and is OFFERED:
   *  a decoder reading the bytes alone takes `[](const Bytes& bytes) {…}`.
   *  ImageAsset and ChannelData are registered by the constructor.
   *  @trap Replacing a decoder leaves a view already decoded holding its
   *  value and the decoder that made it, which is what poll() re-runs. */
  template <typename T>
  void registerDecoder(
      core::Callable<std::optional<T>(const Bytes&, std::string_view hint)>
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

  /** The same, from any object satisfying the Decoder concept — which
   *  reads the hint or the bytes alone, as the callable form does. */
  template <typename T, Decoder<T> D>
  void registerDecoder(D decoder) {
    registerDecoder<T>([decoder = std::move(decoder)](const Bytes& bytes,
                                                      std::string_view hint) {
      return core::callPrefix(
          [&decoder](const Bytes& b, std::string_view h) {
            if constexpr (requires { decoder.decode(b, h); })
              return decoder.decode(b, h);
            else
              return decoder.decode(b);
          },
          bytes, hint);
    });
  }

  /** The resource decoded as a T through the decoder registered for T;
   *  null on failure, and null (with no fetch) when no decoder is
   *  registered for T. Decodes on the first ask, from bytes a prior
   *  fetch() ask already cached when they are present, and caches the
   *  result as one view of the URI's entry. load<ImageAsset>(uri) is
   *  image(uri) and shares its view. */
  template <typename T>
  std::shared_ptr<const T> load(std::string_view uri) {
    return std::static_pointer_cast<const T>(loadRegisteredView(
        cacheKey(uri, nullptr), uri, std::type_index(typeid(T))));
  }

  /** UTF-8 text convenience over fetch(). */
  std::optional<std::string> text(std::string_view uri);

  /** The regular-file URIs named by @p selector, in lexical order: an
   *  exact file, a directory URI read recursively, or a glob in which
   *  `*` matches within one path segment, `?` one non-separator
   *  character and `**` across `/`, a backslash quoting what follows it.
   *  @trap Only LOCAL resources are enumerated: a network selector
   *  without a star is one exact URL and selects itself without a fetch,
   *  and a network glob cannot be enumerated at all. */
  std::vector<std::string> select(std::string_view selector) const;

  /** Fetches the distinct @p uris concurrently into the byte cache and
   *  returns how many are ready. No decoding is performed. */
  size_t preload(std::span<const std::string_view> uris);

  /** The same, over the strings a lease answers with, so a retained set
   *  reaches this call without being copied into a second vector. */
  size_t preload(std::span<const std::string> uris);

  /** Selects @p selector and concurrently fetches the resulting files. */
  size_t preload(std::string_view selector);

  /** An empty resource-retention lease bound to this Hub. */
  ResourceLease retain();

  /** A lease retaining the current files selected by @p selector. */
  ResourceLease retain(std::string_view selector);

  /** A lease retaining the union of the current selector snapshots. */
  ResourceLease retain(std::span<const std::string_view> selectors);
  /** The same from selectors written out at the call site. */
  ResourceLease retain(std::initializer_list<std::string_view> selectors);

  /** Discards every cached entry not protected by a resource lease. Values
   *  already returned in shared_ptrs stay alive for their holders. Returns the
   *  number of cache entries discarded. */
  size_t discardUnretained();

  /** Decoded image (stills and animations); null on failure. Decodes
   *  on this first ask, from bytes a prior fetch() ask already cached
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

  /** HOW MANY BYTES, AND WHERE: the size of the resource and the file
   *  it was read from; nullopt when the URI cannot be served.
   *  @trap const but neither cheap nor side-effect-free — every call
   *  performs a full fetch and caches nothing, which for a network URI
   *  is a round trip and a write into the disk cache directory. */
  std::optional<ResourceInfo> probe(std::string_view uri) const;

  /** WHAT THE BYTES MEAN, WITHOUT DECODING THEM: dimensions and layers
   *  for an image, and whatever the next kind of meaning turns out to
   *  need. The answer comes from T's own library through the `Probable`
   *  seam, so this hub carries no opinion about any format. Fetches like
   *  `probe()` does, and caches nothing. */
  template <Probable T>
  std::optional<T> probe(std::string_view uri) const {
    ResourceInfo info;
    const std::shared_ptr<const Bytes> bytes = probeFetch(uri, info);
    if (!bytes) return std::nullopt;
    return probeResource(std::type_identity<T>{},
                         std::span<const std::byte>(bytes->bytes), info.path);
  }

  /** Re-checks every previously loaded resource; reloads changes and
   *  drops entries whose files vanished. Returns true if anything
   *  changed. */
  bool poll();

  /** The feed at @p uri: the same object for the same URI while anyone
   *  holds it. A URI that resolves through the mount table to a regular
   *  file is replayed from that recording; any other opens through the
   *  transport registered for its scheme — the part before "://" —
   *  called outside the hub's lock.
   *  @trap No scheme, no transport, or an unreadable recording is not a
   *  failure to answer: the feed exists and its error() says why. */
  std::shared_ptr<Feed> feed(std::string_view uri, Feed::Policy policy = {});

  /** Installs the transport a scheme opens through; registering a
   *  scheme again replaces it. */
  void setFeedTransport(std::string scheme, FeedTransport transport);

  /** The transport registered for @p scheme, or an empty function. A
   *  transport that stands in front of another reads the one it wraps
   *  through here before it registers itself. */
  FeedTransport feedTransport(std::string_view scheme) const;

  /** Every feed currently held by someone, in opening order. */
  std::vector<std::shared_ptr<Feed>> feeds() const;

  /** Advances every replayed recording to the steady seconds since this
   *  hub was made. A live feed is unaffected. */
  void dispatch();

  /** The same, to @p seconds on the caller's own clock. */
  void dispatch(double seconds);

  /** Runs @p callback on every dispatch for as long as the lease lives:
   *  given the seconds that dispatch was given, after every replayed
   *  recording has been advanced to them, on the dispatching thread and
   *  in the order the callbacks were registered.
   *  @trap A callback registered from inside a dispatch runs from the
   *  NEXT one. */
  DispatchLease onDispatch(DispatchLease::Callback callback);

 private:
  /** The one fetch both probes make: the bytes, uncached, with @p info
   *  filled in from them. Null when the URI cannot be served. */
  std::shared_ptr<const Bytes> probeFetch(std::string_view uri,
                                          ResourceInfo& info) const;

  /** Re-decodes bytes into a type-erased value; null on failure. The
   *  decode a view was made with rides along with the view, so poll()
   *  can re-run exactly it. */
  using Redecode = std::function<std::shared_ptr<const void>(
      const Bytes&, const std::filesystem::path&)>;

  /** THE THREE TABLES THIS HUB KEEPS — the entry cache, the decoder
   *  registry and the feed transports — with the entry and the decoded
   *  view they are made of. Declared here and defined beside the code
   *  that reads it, so nothing that asks this hub for a resource takes
   *  on the containers it is kept in. */
  struct Caches;

  /** What poll() reads under the lock before it stats and decodes
   *  outside it: enough to re-run every populated view of one entry
   *  without touching the map. */
  struct Reload {
    std::string key;
    std::string uri;
    std::filesystem::file_time_type mtime;
    bool holdsBytes = false;
    std::vector<std::pair<std::type_index, Redecode>> decodes;
  };

  /** What poll() commits for one changed entry: the fresh bytes and
   *  every view decoded from them. */
  struct Reloaded {
    std::shared_ptr<const Bytes> bytes;
    std::filesystem::path path;
    std::filesystem::file_time_type mtime;
    std::vector<std::pair<std::type_index, std::shared_ptr<const void>>> views;
  };

  std::optional<Reloaded> reload(const Reload& pending) const;

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

  /** The map key for an ask: the URI alone for fetch()/text()/
   *  channels() and default-options image(); with a layer or size
   *  set, the URI plus each option behind a '\0' separator — a byte
   *  no URI that names a real resource can contain, so option
   *  suffixes never collide with URI content. Keys are write-only:
   *  nothing parses one back (entries carry their own uri). */
  static std::string cacheKey(std::string_view uri,
                              const image::DecodeOptions* options);

  /** Guards every member below. It is never held across a fetch, a
   *  decode or a file write, so one slow source never stalls another
   *  thread's cache hit; each accessor reads under it, works outside
   *  it, and re-locks to commit. */
  mutable std::mutex m_mutex;
  std::vector<std::pair<std::string, std::filesystem::path>> m_mounts;
  const std::unique_ptr<Caches> m_caches;
  std::filesystem::path
      m_networkCacheDirectory;  // empty = platform cache directory
  NetworkPolicy m_networkPolicy = NetworkPolicy::CacheFirst;
  NetworkTransport m_networkTransport;  // empty = libcurl
  std::shared_ptr<detail::Residency> m_residency;
  /** The feeds opened through this hub, in opening order, held weakly
   *  so a feed lives exactly as long as its readers do. Every walk of
   *  the list erases the entries whose feed is gone, which is why the
   *  list is mutable: dropping the name of something that no longer
   *  exists changes no answer this hub can give. */
  mutable std::vector<std::pair<std::string, std::weak_ptr<Feed>>> m_feeds;
  /** The callbacks registered through onDispatch(), held weakly so one
   *  lives exactly as long as the lease that owns it. A dispatch copies
   *  the live ones out from under the lock — a callback reads feeds and
   *  may ask this hub for a resource — and erases the entries whose
   *  lease is gone. */
  std::vector<std::weak_ptr<DispatchLease::Callback>> m_dispatchers;
  /** When this hub was made: what dispatch() counts its seconds from. */
  const std::chrono::steady_clock::time_point m_created =
      std::chrono::steady_clock::now();
};

}  // namespace sigil::io
