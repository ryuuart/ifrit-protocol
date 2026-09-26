#pragma once

/** @file
 * @ingroup io-hub
 * The resource hub: one object that answers a URI. It reads and caches a
 * resource's bytes, decodes them into what they mean through the decoder
 * the owning library registered, writes bytes back where it mounts,
 * listens on a door for a resource that keeps ARRIVING, and shares
 * drawn frames with other applications. The control a host needs —
 * advancing time, late mounts, residency, decoders, transports — is
 * declared in `sigilio/advanced/`, as free functions over the hub.
 */

#include <sigilcore/callable/Callable.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <typeindex>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#include "sigilio/frames/Frame.h"
#include "sigilio/hub/Feed.h"
#include "sigilio/hub/Network.h"
#include "sigilio/source/Sink.h"
#include "sigilio/source/Source.h"

namespace sigil::io {

class Lease;
class ResourceLease;
struct ResourceInfo;

namespace frames {
class Publisher;
class Subscription;
}  // namespace frames

namespace detail {
struct Residency;
/** The one door the control in `sigilio/advanced/` reaches the hub by. */
struct HubAccess;
}  // namespace detail

/** WHAT A HUB IS MADE WITH, passed once to its constructor. */
struct HubOptions {
  /** URI prefix → directory: `{"res://", "assets"}` answers
   *  "res://ui/logo.png" from assets/ui/logo.png. The longest matching
   *  prefix wins; none by default. */
  std::map<std::string, std::filesystem::path> mounts;
  /** The schemes of the linked transports the first listen() nothing
   *  answers registers — `{"udp"}` for a program that speaks nothing
   *  else; empty registers every one the program links. */
  std::vector<std::string> transports;
  /** How http(s):// resources are fetched and cached. */
  NetworkOptions network;
};

/**
 * The resource hub: ask for resources by URI.
 *
 * Each URI is cached as one entry whose bytes and decoded views are
 * independent, each populated the first time its accessor is asked, and
 * a load<T>() ask with options other than T's defaults is a different
 * decode in an entry of its own. A failed lookup is NOT cached: a missing file
 * loads as soon as it appears. Calls on one Hub may overlap — mount,
 * decoder, cache and retention state are synchronized internally — and
 * a Hub satisfies ByteSource and, with `sigilio/advanced/Places.h`,
 * ResolvingByteSource.
 */
class Hub {
 public:
  /** A hub made with @p options. Nothing is decoded until the library
   *  that owns a meaning registers its decoder on it, so bytes are all a
   *  new hub answers. */
  explicit Hub(HubOptions options = {});
  ~Hub();

  Hub(const Hub&) = delete;
  Hub& operator=(const Hub&) = delete;
  Hub(Hub&&) = delete;
  Hub& operator=(Hub&&) = delete;

  /** The resource's bytes, cached per URI: a mounted `res://` URI, a
   *  network `https://` URL behind the disk cache, or a plain path. Null
   *  when unresolvable or unreadable. Never decodes: bytes load and
   *  cache whether or not any decoder accepts them. */
  std::shared_ptr<const Bytes> read(std::string_view uri);

  /** The same, as UTF-8 text. */
  std::optional<std::string> text(std::string_view uri);

  /** The resource decoded as a T through the decoder registered for T;
   *  null on failure, and null (with no read) when no decoder is
   *  registered for T. Decodes on the first ask, from bytes a prior
   *  read() already cached when they are present, and caches the
   *  result as one view of the URI's entry. */
  template <typename T>
  std::shared_ptr<const T> load(std::string_view uri) {
    return std::static_pointer_cast<const T>(loadRegisteredView(
        std::string(uri), uri, std::type_index(typeid(T))));
  }

  /** The same, decoded with @p options — T's own, as the library that
   *  owns T names them: `load<media::Image>(uri, {.width = 124})`.
   *  Options equal to T's defaults are the ask above and share its view;
   *  any others are a decode of their own in an entry of their own, which
   *  every later ask with equal options shares and a reload re-runs with
   *  the same options. */
  template <Configurable T>
  std::shared_ptr<const T> load(std::string_view uri,
                                const LoadOptions<T>& options) {
    using Options = LoadOptions<T>;
    if (options == Options{}) return load<T>(uri);
    return std::static_pointer_cast<const T>(loadConfiguredView(
        uri, std::type_index(typeid(T)),
        std::make_shared<const Options>(options),
        [](const void* left, const void* right) {
          return *static_cast<const Options*>(left) ==
                 *static_cast<const Options*>(right);
        }));
  }

  /** Stores @p bytes under @p uri, through the same mount table a read
   *  resolves by, creating the directories above the file. Every cached
   *  view of that URI is dropped, so the next ask reads the file back.
   *  @trap A network URI cannot be written and answers false — a hub
   *  writes where it mounts. */
  bool write(std::string_view uri, std::span<const std::byte> bytes);

  /** @p value ENCODED AND STORED under @p uri: the library that owns T
   *  encodes it in the format the name's extension says — a
   *  `media::Image` to "res://out/plate.png" is a PNG — and the bytes are
   *  written as write() writes them. False when the name names nothing
   *  T's library writes, or the write fails. */
  template <Savable T>
  bool save(std::string_view uri, const T& value) {
    const std::vector<std::byte> bytes =
        encodeResource(value, std::filesystem::path(uri));
    return !bytes.empty() && write(uri, bytes);
  }

  /** The same for a value held shared, as a load answers one; false for
   *  none. */
  template <Savable T>
  bool save(std::string_view uri, const std::shared_ptr<T>& value) {
    return value && save(uri, *value);
  }

  /** THE FEED AT @p uri: a handle onto the same door for the same URI
   *  while anyone holds one. A URI replay() named plays that recording
   *  back; any other opens through the transport registered for its
   *  scheme — the part before "://". Every transport linked into the
   *  program (or those `HubOptions::transports` names) is registered on
   *  the first ask for a scheme nothing answers yet. @p options are the
   *  first ask's: a later ask for a door that stands is handed that door
   *  as it was opened.
   *  @trap No scheme, no transport, or an unreadable recording is not a
   *  failure to answer: the feed exists and `state().error` says why. */
  Feed listen(std::string_view uri, ListenOptions options = {});

  /** THE FEED AT @p uri, PLAYED FROM A RECORDING instead of a door:
   *  @p recording is a file, or a URI the mount table resolves to one,
   *  written by `Feed::record()`. A feed already standing at @p uri is
   *  closed first, and every later listen() on @p uri — for as long as
   *  this hub lives — plays the same file, so a reader written against
   *  the live wire reads the recording without knowing it. The recording
   *  starts on the first advance after the feed is made and closes the
   *  feed after its last message.
   *  @trap Naming @p uri again replaces the recording it plays; a feed
   *  still held from the earlier call is closed, not redirected. */
  Feed replay(std::string_view uri, std::string_view recording,
              ListenOptions options = {});

  /** A PUBLICATION OF DRAWN FRAMES under the name @p uri carries —
   *  `syphon://NAME` over Metal on macOS, `spout://NAME` over Direct3D11
   *  where Spout is built — on @p options' device, this machine's default
   *  Metal device when it names none. Defined by `SigilIOFrames`, which
   *  a caller links to publish.
   *  @trap AN EMPTY HANDLE IS AN ORDINARY ANSWER — another scheme, no
   *  device, an empty name, a protocol this build lacks — and means a
   *  run that does not publish, never another way to. */
  frames::Publisher publish(std::string_view uri,
                            const frames::PublishOptions& options = {});

  /** THE FRAMES ANOTHER APPLICATION PUBLISHES under the name @p uri
   *  carries, received on @p options' device; a name nothing publishes
   *  yet is waited for. Defined by `SigilIOFrames`.
   *  @trap An empty handle is an ordinary answer, as for publish(). */
  frames::Subscription subscribe(std::string_view uri,
                                 const frames::SubscribeOptions& options = {});

 private:
  friend struct detail::HubAccess;

  // What `sigilio/advanced/` reaches through HubAccess: each is
  // documented on the free function that spells it there.
  void mount(std::string prefix, std::filesystem::path dir);
  std::filesystem::path resolve(std::string_view uri) const;
  void setNetworkCacheDirectory(std::filesystem::path directory);
  void setNetworkPolicy(NetworkPolicy policy);
  void setNetworkTransport(NetworkTransport transport);
  std::vector<std::string> select(std::string_view selector) const;
  size_t preload(std::span<const std::string_view> uris);
  size_t preload(std::span<const std::string> uris);
  size_t preload(std::string_view selector);
  ResourceLease retain();
  ResourceLease retain(std::string_view selector);
  ResourceLease retain(std::span<const std::string_view> selectors);
  size_t discardUnretained();
  bool poll();
  void setFeedTransport(std::string scheme, Transport transport);
  Transport feedTransport(std::string_view scheme) const;
  std::vector<Feed> feeds() const;
  void advance();
  void advance(std::chrono::duration<double> time);
  /** What an advance hands a callback: the time it was given. */
  using AdvanceCallback = std::function<void(std::chrono::duration<double>)>;
  Lease onAdvance(AdvanceCallback callback);

  /** The one fetch a probe makes: the bytes, uncached, with @p info
   *  filled in from them. Null when the URI cannot be served. */
  std::shared_ptr<const Bytes> probeFetch(std::string_view uri,
                                          ResourceInfo& info) const;

  /** Re-decodes bytes into a type-erased value; null on failure. The
   *  decode a view was made with rides along with the view, so poll()
   *  can re-run exactly it. */
  using Redecode = std::function<std::shared_ptr<const void>(
      const Bytes&, const std::filesystem::path&)>;

  /** THE TABLES THIS HUB KEEPS — the entry cache, the decoder
   *  registry, the feed transports and the recordings replay() put in
   *  front of them — with the entry and the decoded
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

  /** Whether two option values of one type are equal, compared through
   *  the type that load<T>() knew and this hub does not. */
  using SameOptions = bool (*)(const void* left, const void* right);

  /** The registered decoder of `type` with `options` bound into it:
   *  options equal to ones asked before share their entry. Null when no
   *  decoder that takes options is registered for `type`. */
  std::shared_ptr<const void> loadConfiguredView(
      std::string_view uri, std::type_index type,
      std::shared_ptr<const void> options, SameOptions same);

  /** A registered decoder with a load's options bound into it. */
  using Configure = std::function<Redecode(std::shared_ptr<const void>)>;

  /** The decoder registered for `type`, or an empty function. */
  Redecode registeredDecoder(std::type_index type) const;
  /** Registers `decode` for `type` at its defaults, and — for a type
   *  loaded with options — `configure`, which binds other options in. */
  void setDecoder(std::type_index type, Redecode decode, Configure configure);

  std::vector<std::pair<std::string, std::filesystem::path>>
  mountedDirectories() const;
  std::shared_ptr<detail::Residency> residency();

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
  mutable std::vector<std::pair<std::string, std::weak_ptr<detail::FeedDoor>>>
      m_feeds;
  /** Whether the transports linked into the program were registered on
   *  this hub, which happens once, on the first ask nothing answered. */
  bool m_linkedTransports = false;
  /** The linked transport schemes that registration is limited to;
   *  empty for every one. */
  std::vector<std::string> m_transportSchemes;
  /** The callbacks registered through onAdvance(), held weakly so one
   *  lives exactly as long as the lease that owns it. An advance copies
   *  the live ones out from under the lock — a callback reads feeds and
   *  may ask this hub for a resource — and erases the entries whose
   *  lease is gone. */
  std::vector<std::weak_ptr<AdvanceCallback>> m_advancers;
  /** When this hub was made: what advance() counts its time from. */
  const std::chrono::steady_clock::time_point m_created =
      std::chrono::steady_clock::now();
};

}  // namespace sigil::io

// The decoder registration a library's generic `registerDecoders(hub)`
// reaches by argument-dependent lookup, which only finds what the
// translation unit can see: wherever a hub is, so is the way to teach it.
#include "sigilio/advanced/Decoding.h"
