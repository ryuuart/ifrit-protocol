# SigilIO

A runtime resource hub. Application code asks for a resource by URI —
`res://ui/logo.png` — instead of by filesystem path. URI prefixes mount
onto directories, results are cached per resource, and `poll()` re-stats
what has been loaded so edited files reload without a restart. A whole
DIRECTORY or a glob is a resource set: one selector names it, `preload()`
fetches it concurrently, and a lease says how long the hub promises to
keep it. `write()` stores bytes back through the same mounts. `http://`
and `https://` URIs fetch over libcurl behind an on-disk cache with a
selectable policy; `file://` strips to a plain local path. What a byte
MEANS is not its job in either direction: it hands bytes to registered
decoders, SigilImage's by default, and takes already-encoded bytes back.

Namespace `sigil::io`. One feature library per directory, linked by
what a consumer uses; every public header lives under
`include/sigilio/<feature>/` and is spelled `<sigilio/<feature>/X.h>`:

| target | headers | holds |
|--------|---------|-------|
| `SigilIOSource` | `source/Source.h`, `source/Archive.h`, `source/Sink.h`, `source/Places.h` | the byte vocabulary in both directions: `Bytes`, the `ByteSource`, `ResolvingByteSource`, `Decoder` and `Probable` concepts, `AnyByteSource` (the type-erased source value), the `ByteSink` concept and `writeBytes()`, the one place a path and a run of bytes become a file; `ArchiveSource` and `ArchiveEntry`, one zip held in memory answering its files by name — and the two places only the platform can name, `executablePath()` and `scratchDirectory(label)` |
| `SigilIOHub`    | `hub/Hub.h`, `hub/Feed.h`, `hub/Recording.h`, `hub/Network.h`, `hub/TextCatalog.h` | the `Hub`, `ResourceInfo` (a resource's byte size and the file it came from), and `ResourceLease`; `NetworkPolicy`, `NetworkTransport`, `probeNetworkCache()` and `seedNetworkCache()` — inspect or populate the persistent cache by URL without constructing its filenames or contacting a server; `Feed`, `Arrival`, `OpenedFeed` and `FeedTransport` — a resource that keeps arriving, opened through the hub's `feed()` and moved forward by its `dispatch()`; `DispatchLease` and `Hub::onDispatch()` — a callback the same `dispatch()` drives, for as long as the lease lives; `RecordingWriter` and `readRecording()`, the format a feed records itself in; and `TextCatalog`, the stock value over the hub that a directory of authored shaders is |
| `SigilIOTransport` | `transport/Transport.h` | `registerUdp()`, `registerWebSocket()`, `registerWebSocketClient()` and `registerTransports()` — the UDP transport, one socket per feed on a thread of its own, answering to udp:// and, for messages that are OSC packets, to osc://; the WebSocket listener, one of them per feed on a loop of its own, answering to ws:// and, where that URI's query names a directory of pages, answering HTTP GET out of it on the same port; and the WebSocket client over libcurl, one session per feed on a thread of its own, which is what ws:// and wss:// open when the URI names a server to call rather than a port to hold; either listener fills `OpenedFeed::sendTo`, so a listening feed answers the one sender an arrival names through `Feed::sendTo()`; linked by a consumer that opens network feeds and by no other |

`SigilIO` is the umbrella target over the source and the hub, and
`<sigilio/IO.h>` the umbrella header; the transport feature stands
outside both, linked only where a network feed is opened. The hub is a `ByteSource`;
anything that consumes bytes by URI can be written against the concept
and handed a hub, a fixture, or an `AnyByteSource` holding either.

## Using it

```cpp
#include <sigilio/hub/Hub.h>

sigil::io::Hub hub;
hub.mount("res://", "/opt/myapp/assets");

auto shader = hub.text("res://shaders/glow.sksl");   // std::optional<std::string>
auto table  = hub.blob("res://data/table.bin");      // shared_ptr<const Bytes>
auto logo   = hub.image("res://ui/logo.png");        // stills and animations
auto icon   = hub.image("res://ui/mark.svg", {.width = 256});
auto layer  = hub.image("res://light/probe.exr", {.layer = "diffuse"});
auto planes = hub.channels("res://light/probe.exr"); // every raw channel

if (auto info = hub.probe("res://light/probe.exr"))   // bytes: size, path
  budgetFor(info->byteSize);
// …and what those bytes MEAN is asked of the library that owns the
// meaning, through its own probe:
if (auto probed = hub.probe<sigil::image::ImageProbe>("res://light/probe.exr"))
  useDimensions(probed->width, probed->height);

// Any type, once its decoder is registered: a Decoder<T> object or a
// function from bytes (and the resource's name as a hint) to optional<T>.
hub.registerDecoder<Mesh>(ObjParser{});
auto crate = hub.load<Mesh>("res://props/crate.obj");   // shared_ptr<const Mesh>

// The hub as a ByteSource, for code that only wants bytes by URI.
sigil::io::AnyByteSource source(hub);
auto raw = source.fetch("res://data/table.bin");

// Network resources need no mount; the disk cache already sits under the
// platform's cache location, and this points it at an asset directory
// instead. A host with its own HTTP stack hands in the function that
// answers a URL with its body.
hub.setNetworkCacheDirectory("/opt/myapp/assets/.netcache");
hub.setNetworkPolicy(sigil::io::NetworkPolicy::Offline);
hub.setNetworkTransport(myHttpClient);
auto remote = hub.image("https://example.com/tex.png");

// Inspect or seed the persistent cache by URL without fetching. The same
// directory override is supplied to these operations and the hub.
auto retainedBytes = sigil::io::probeNetworkCache(
    "https://example.com/tex.png", "/opt/myapp/assets/.netcache");
sigil::io::seedNetworkCache("https://example.com/seed.png", encodedBytes,
                             "/opt/myapp/assets/.netcache");

std::string_view shaderUris[] = {"shader://surface.slang",
                                 "shader://bloom.sksl"};
hub.mount("shader://", shaderDirectory);
hub.mount("plugin://", pluginShaderDirectory);
hub.preload(shaderUris); // concurrent fetch, bytes only
auto sksl = hub.select("shader://**/*.sksl"); // sorted URI snapshot
hub.preload("shader://**/*.sksl"); // discover, then fetch concurrently
hub.preload("shader://"); // a directory selector recursively fetches everything

// A consumer whose own shader files live in one directory needs exactly one
// prefix over it: the catalogue is that declaration, its own hub inside. A
// shader a library SHIPS is compiled into that library instead and needs no
// hub at all.
sigil::io::TextCatalog glowShaders("shader://glow/", shaderDirectory);
glowShaders.preload();
auto body = glowShaders.text("Glow.sksl");            // std::optional<std::string>

// Preloading controls when bytes arrive. A lease controls how long the Hub
// promises to retain them, and may unite any number of selectors.
auto authored = hub.retain({"shader://material/**/*.sksl",
                            "shader://compose/**/*.sksl"});
authored.include("plugin://**/*.slang");
authored.preload();
for (const std::string& uri : authored.uris())
  registerAuthoredShader(uri);

// Bytes back out, through the same mount table they are read by. What
// they are is the caller's business: an image is encoded first.
sk_sp<SkData> png = sigil::image::encodeImage(*rendered,
                                              sigil::image::Format::Png);
hub.write("res://out/plate.png", png->data(), png->size());

// A plain path, with no hub in reach — the sink half on its own.
sigil::io::writeBytes(outDir / "plate.png", png->data(), png->size());

// Once per frame, or on a file-watch event.
if (hub.poll())
  redraw();
```

A resource that keeps ARRIVING is a feed, and the hub is the door on it
too. The same URI answers the same feed while anyone holds it; a
transport registered for the scheme delivers into it from a thread of
its own, and a reader on any thread never waits.

```cpp
#include <sigilio/hub/Feed.h>
#include <sigilio/transport/Transport.h>

sigil::io::registerTransports(hub);                  // udp://, osc://, ws://, wss://
auto scene = hub.feed("udp://:27020");               // std::shared_ptr<sigil::io::Feed>
if (auto newest = scene->latest())                   // the newest message; generation() counts them
  draw(*newest);
while (auto arrival = scene->receive())              // every message since the last receive, in order
  fold(*arrival->bytes, arrival->from);              // …and the address that one came from
auto desk = hub.feed("udp://desk.local:9001");       // a peer: send() reaches it, its replies arrive
desk->send(reply);
auto control = hub.feed("osc://:9000");              // the same socket, for messages that are OSC
if (auto arrival = scene->newest())                  // a listener holds no peer of its own…
  scene->sendTo(arrival->from, reply);               // …so it answers the one sender that wrote to it
auto browsers = hub.feed("ws://:8848/scene");        // every peer that reaches that path
browsers->send(frame);                               // …and one send goes out to all of them
auto staged = hub.feed("ws://:8848/sky?pages=res://sky");  // …and GET serves that directory
auto studio = hub.feed("wss://sky.example:443/scene");  // the same scheme calling out: a server to reach
studio->send(frame);                                 // …the one peer it dialled, whose messages arrive
scene->record(outDir / "scene.feed");                // every arrival from now on, to a recording
hub.mount("udp://:27020", outDir / "scene.feed");    // the next feed() on that URI replays the file
hub.dispatch(seconds);                               // once per frame: recordings advance to this time
```

## Mental model

A `Hub` holds the mount list, decoder registry and cache. Resource leases keep
explicit claims beside that cache; the retained URI set is observable rather
than an undocumented side effect of having loaded something once.

Mounts map a URI prefix onto a directory, and the **longest matching
prefix wins**, so `res://deep/` can point somewhere other than `res://`.
Re-mounting a prefix replaces it. A URI that matches no mount is tried as
a plain path. A mount is a namespace and not a door into the filesystem
around it: what a URI names is **beneath** the mounted directory, so a
remainder that climbs out through `..` resolves to nothing — for a
fetch, for `resolve()` and for a selector alike. A URI that names a
directory rather than a file answers nothing too: the hub answers bytes.

The cache holds one entry per URI. An entry carries the blob and one
decoded view per type — the image, the channel data, and whatever
`load<T>()` has been asked for — each populated the first time its
accessor is asked. Asking for bytes never decodes, and a later `image()`,
`channels()` or `load<T>()` ask on the same URI decodes the bytes the
entry already holds instead of reading the source again. Each view
remembers the decode that made it, which is what `poll()` re-runs. An
`image()` ask with a layer or an explicit size is a different decode, so
it gets its own entry, keyed by the URI plus the options behind a
separator byte no URI can contain; at default options `image()` is the
registered `ImageAsset` decoder, so it and `load<ImageAsset>()` share one
view. Every entry also remembers the URI it was asked by, which is
what reloading goes back to — a URI is never re-derived from a key
string, so no character a URI may contain is special.

`write()` runs the read's resolution backwards: the URI resolves through
the same longest-prefix mount table, the directories above the file are
created, and every cached entry for that URI is dropped so the next ask
reads the file back rather than serving what was there before. Entries are
matched on the URI each one carries, never by parsing a key.

Network URIs bypass the mount list entirely. A fetch goes through the disk
cache directory, and the entry carries a sentinel timestamp so `poll()`
knows to leave it alone.

`select()` turns one selector into a sorted, duplicate-free URI snapshot.
An exact file selects itself, a directory selects every regular file below it
recursively, and a glob uses `*` within one path segment, `?` for one
non-separator character and `**` across directories. A backslash quotes the
next character. Mounted URIs, `file://` URLs and plain filesystem paths can be
enumerated. A network selector with no star is one exact URL, including its
query and a possible trailing slash, and selects itself without fetching;
network globs cannot be enumerated. When mounts overlap, the same longest-prefix
rule as an ordinary read decides which physical file occupies a URI.

`preload()` fetches distinct URI bytes concurrently and merges them into the
same cache ordinary reads use. Its selector overload calls `select()` first, so
one directory or glob replaces a maintained list. A fetch waits on a disk or
on a server, so the fan-out is SigilCore's blocking seam rather than the one
computations divide themselves over: a preload of a hundred URLs cannot stall
a parallel range somewhere else in the process for as long as a server takes.

Preloading and retention are separate. `preload()` eagerly fills the byte cache
but makes no residency promise. `retain()` returns a movable `ResourceLease`
whose sorted `uris()` are the promise: every cache entry for those URIs is
protected until that lease releases it. A lease may include several selectors;
their matches are one duplicate-free union, and overlapping leases retain a URI
independently. Selectors are snapshots until `refresh()` reruns them, admitting
new files and releasing vanished ones. `discardUnretained()` removes every
unprotected cache entry; values already held through a `shared_ptr` survive that
removal for their holders. Nothing in this repository evicts, so a lease is
for a host that clears a hub between scenes.

Every decode is a registered decoder. The constructor registers
SigilImage's two — `ImageAsset` and `ChannelData` — and
`registerDecoder<T>()` adds any other — an object whose `decode()`
satisfies the `Decoder` concept, or a callable, either of them reading the
bytes and the name hint or the bytes alone: SigilDrawBrush's
`format::BrushDecoder` is one such, answering a `brush::Tool` from a
native brush archive, a Photoshop `.abr` or a Procreate `.brush`, and it
lives in the brush library because a brush is that library's type; registering a type again replaces
the decoder later asks run, while a view already decoded keeps its value
and the decoder that made it, which is what `poll()` re-runs for it.
`load<T>()` with no decoder registered for `T` answers null without
fetching. The hub never inspects bytes.

A **feed** is a resource that keeps arriving. `feed()` answers one `Feed`
per URI for as long as anyone holds it, and a later ask for the same URI
while it is held is the same object, so two readers of one port share
one socket. What arrives is a byte message, delivered by a transport from
whichever thread it runs on; the feed latches the newest as `latest()`
with a `generation()` that counts every arrival, and queues each arrival
for `receive()`, which hands them out in order and never waits.
`Feed::newest()` is that same latched message WHOLE — the generation it
came in as, the second it came in at, its bytes and the sender it named —
for a reader that wants more of the newest than its bytes and is not
draining the queue to get it. The queue is bounded by the feed's
`Policy`: when it is full the oldest arrival is dropped and `dropped()`
counts it, because a reader that fell behind a state feed wants the
newest, not the backlog. `close()` takes nothing more and keeps what was
received readable. A feed's `error()` says why a door could not be
opened — no scheme, no transport for it, a port already taken — and the
feed still exists, so a program that opened the wrong URI sees the
sentence rather than a null.

A scheme opens through the `FeedTransport` registered for it, called
outside the hub's lock; the transport hands back an `OpenedFeed`: how the
feed closes it, how `send()` goes back through it when the way is two-way,
how `OpenedFeed::sendTo` answers one named sender when it can address one,
and the local `address()` it bound. Every arrival also names where it came
from: `sigil::io::Arrival::from` is the sender's address spelled the way a
URI of that scheme is, `udp://127.0.0.1:52341`, and is empty where the
transport has no way of knowing — and on a replayed recording, which holds
the messages and not who sent them.

`Feed::sendTo()` is the OTHER way back out, and the one a door that holds
no peer of its own has: it takes an address spelled the way an arrival's
`from` is and writes to that sender alone. It is false where the
transport cannot address one, where the feed is closed and where no
transport opened it — a replayed recording among them, which has no
sender to answer and no end to answer through. `send()` is unchanged by
it: a peer's feed still writes to its peer and a listening WebSocket
still broadcasts to every peer on its path.

`SigilIOTransport` registers three transports under four schemes, two of
those transports sharing a scheme.
`registerUdp()` takes the UDP ones: `udp://:PORT` listens on every
interface, IPv4 and IPv6 alike, and `udp://HOST:PORT` is a peer that
`send()` reaches and whose replies arrive. A listener has no peer to
`send()` to and answers ONE sender instead, through `Feed::sendTo()`:
the address is read the way a URI of that scheme is written and
resolved as the literal it is, so answering waits on no name lookup and
a `from` that is not literal is nobody to answer. `osc://` is that same
socket under another name, for a port whose messages are OSC packets; a
feed keeps the scheme it was opened with, in its `uri()`, in its `address()`
and in every sender it names, so a reader picks the decoding off the URI
rather than out of the bytes. Every socket runs on one thread of its own,
private to the transport.

`registerWebSocket()` takes `ws://`. `ws://:PORT/PATH` listens on every
interface for peers reaching that path — an omitted PATH being the root —
every text or binary message from any of them arrives naming that peer,
and one `send()` goes out to all of them at once while `Feed::sendTo()`
reaches the one peer it names and no other. Each listening feed holds a
loop on a thread of its own, and closing the feed gives the port back and
ends the peers still attached to it. A peer is named by the address its
own messages arrive under, and a peer that has left before the loop
reaches an answer is nobody to answer — the answer having been posted is
what `sendTo()` says, since the loop that holds the peers is not the
thread that asked. That registration LISTENS: the library underneath
carries no client and its sockets are built without TLS, so it opens a
port to hold and nothing else.

A URI'S QUERY MAY NAME WHERE ITS PAGES STAND —
`ws://:PORT/PATH?pages=URI` — and the same port then answers HTTP GET
out of that directory, so what a peer loads and the socket it opens back
are one address. `/` and `/index.html` are that directory's
`index.html`; any other path is the file of that name beneath it, typed
by its extension — html, css, js, json, png, jpg, svg and txt, and bytes
with no name otherwise. A path naming no file there, a path climbing out
through `..` exactly as a mount refuses one, a page larger than the
listener hands back, and every request to a listener whose URI named no
pages at all, are answered with a status and a plain sentence and never
with a page. The pages URI is resolved through the hub's mount table AS
THE FEED OPENS, and what the listener keeps from then on is the
directory: a feed may outlive the hub that opened it, and a request is
answered out of the filesystem and nothing else. So a pages URI that
resolves to no directory opens nothing and leaves the reason on the
feed, while a page edited on disk is the page the next reload is served,
the directory being read per request and cached nowhere. The query is
the listener's own arrangement and no part of the path peers reach: it
stands in neither the address the feed reports nor the sender an arrival
names.

`registerWebSocketClient()` is the other end, and it takes `ws://` and
`wss://` both. The two ends share a scheme, and the SHAPE of the URI is
what says which one a feed is: `ws://HOST:PORT/PATH` and
`wss://HOST:PORT/PATH` name a server to call, `ws://:PORT/PATH` names a
port to hold. The client stands in front of whatever was registered for
those schemes and splits the two in one place — a URI with a host it
calls itself, a URI without one it hands to the listener behind it — so
`registerWebSocket()` is installed first, and a scheme with nothing
behind it refuses a hostless URI with the reason. The port is spelled
rather than taken from the scheme, so what a feed reaches is what its
URI says. A call is made over libcurl, which is where the TLS `wss://`
needs comes from: each feed holds one session on a thread of its own,
every message the server sends arrives whole however many frames it was
split into, `send()` writes one whole message back as bytes, and both
the `address()` the feed reports and the sender every arrival names are
the server it dialled — the one peer a client has, so `Feed::sendTo()`
is false on it. The handshake runs on that same thread: a feed is
answered before its server has been reached, `error()` carries the
reason when it cannot be, and a `send()` before then goes nowhere and
says so. A server that ends the session closes the feed.

A **recording** is a feed written down: `record(path)` appends every
arrival from then on, with the seconds since the feed was made, in the
format `RecordingWriter` writes and `readRecording()` reads. A URI that
resolves through the mount table to a regular file is not opened through
a transport at all: the feed replays that file, and `dispatch(seconds)`
advances every replay to that time on the caller's clock, delivering each
recorded arrival at its recorded second and closing the feed after the
last. That is how a deterministic run reads what a live one heard:
`mount()` the URI onto the recording, and the code that opened the port
opens the file.

Once every replay has been advanced, that same `dispatch()` runs each
callback registered through `onDispatch()`, in registration order, on
the dispatching thread and with the same seconds — so something that
reads feeds on the frame is driven by the call a host already makes,
sees what this very dispatch delivered, and is unregistered by letting
its `DispatchLease` go.

## Gotchas

A `Hub` synchronizes its mount table, decoder registry, cache and retention
state internally. Calls on one Hub may overlap. Fetch, decode and disk-write
work happens outside the cache lock — `poll()` and `write()` included — so
a decoder may ask the same hub for another resource while it runs, and a
slow source never stalls another thread's cache hit; concurrent cold asks
may do the same source work, but only one resulting view becomes the
cached answer, and a `poll()` commits a reload only into an entry nobody
replaced meanwhile. A single `ResourceLease` is a mutable selector set and
its own `include()` and `refresh()` calls must not overlap; independent
leases coordinate their URI claims internally.

Selection is a filesystem snapshot, not a watch. A later `select()` sees files
added since the previous call, while a returned vector does not change beneath
its caller. Matching is case-sensitive and directory selection is recursive.

A resource lease protects cached versions from cache eviction. It does not make
a vanished source exist or suppress hot reload: `poll()` may remove a missing
resource or replace a changed version, and `write()` invalidates the version it
overwrites. An already returned `shared_ptr` continues to own its older value.

`probe()` answers how many bytes a resource is and which file they were
read from — nothing about what they are. `probe<T>()` answers meaning,
and the answer comes from T's own library: a type is probeable when its
namespace declares `probeResource(std::type_identity<T>,
std::span<const std::byte>, const std::filesystem::path&)`, which is the
`Probable` concept in `source/Source.h`. SigilImage declares it for
`ImageProbe`, so `hub.probe<sigil::image::ImageProbe>(uri)` reads
SigilImage's prober and the hub carries no opinion about any format. A
kind of meaning added tomorrow is one free function in the library that
owns it, with nothing to change here.

Both are `const` but neither is cheap or side-effect-free: each performs
a full fetch on every call and caches nothing. For a network URI that may
hit the network and write into the cache directory.

`poll()` reloads local paths only. It erases entries whose file has
vanished, skips `http(s)://` entries entirely, and reloads by decoding
again into a *new* `shared_ptr`. Anyone still holding the previous pointer
keeps the old data; picking up the new data means asking the hub again.

`image()` after `blob()` decodes the bytes `blob()` already read. If the
file changed on disk between the two asks, the decoded view catches up at
the next `poll()` — which re-decodes every populated view from one fresh
read — not at the ask itself.

Failed lookups are deliberately not cached. A URI that resolves to a file
which does not exist yet returns null now and loads as soon as the file
appears.

`write()` refuses a network URI. A hub writes where it mounts; a network
URI belongs to its server, and changing the local cache cannot write there.

`writeBytes()` is true only when every byte reached the file and the
stream closed clean, so a half-written file reads as a failure rather than
as a shorter resource. A zero-length write still creates the file:
emptiness is a value a resource may have.

The default disk cache is `SigilIO/network` under the platform's cache
location — `~/Library/Caches` on macOS, `$XDG_CACHE_HOME` or `~/.cache`
elsewhere, `%LOCALAPPDATA%` on Windows — and falls back to the system
temporary directory only where the platform names no cache location. The
temporary directory is not the default because the OS evicts it on its own
schedule (macOS deletes what has not been touched for three days), and a
lane that renders network-fetched assets without fetching depends on the
cache still being there. The resolver reads the environment directly:
SigilIO stands below every UI toolkit and cannot ask one where the caches
go. `setNetworkCacheDirectory()` overrides it per hub.

Cache filenames are private to the hub and implementation-dependent. Cache
directories are local scratch, not portable artifacts. To ask whether a URL
has bytes on this machine, `probeNetworkCache(url, directory)` returns its
cached byte count without reading or decoding the file. A missing entry or a
metadata error answers nothing; zero is a present, empty resource. The probe
creates no files or directories. A consumer needing actual content can reject
zero separately.

`seedNetworkCache(url, bytes, directory)` stores already-held bytes for later
cache reads, including empty bytes. It creates the directory when needed and
publishes the whole resource through the same complete-file write as a fetch;
a failed write leaves an existing resource intact. Both calls accept only
`http://` and `https://` URLs, never contact a server, and use the default disk
cache when the directory is empty. Seeding affects the disk cache; a hub's
already-loaded views keep their values until discarded.

Network fetches follow redirects, time out after 20 seconds, fail on any
HTTP status of 400 or above, and buffer the whole body in memory.
Persisting to the cache is best-effort, and whole or not at all: the body
goes through `writeBytes()` into a sibling file that takes the cache name
only once every byte is there, so a fetch that cannot be written to disk
still returns its bytes and a later run finds the whole resource or
nothing. `curl_global_init` runs lazily on the first fetch and
`curl_global_cleanup` is never called. `setNetworkTransport()` replaces
libcurl with any function from a URL to its body; the cache and the
policy stay in front of it.

The policies differ in their failure behaviour, which is the part that
matters. `CacheFirst` (the default) serves a present cache file without
touching the network at all, so a run goes offline for free once a
resource has been seen. `Refresh` asks the network first and falls back to
the cached copy when the fetch fails, so a flaky network degrades to
`CacheFirst` rather than erroring. `Offline` never touches the network:
cache hit or failure.

## Boundary

Dependencies: `SigilIOHub` links `SigilIOSource`, `SigilImageDecode` and
Boost.Container publicly and `CURL::libcurl` plus `SigilCoreSchedule`
privately — private because they are transport and where a fetch that
blocks runs, while curl remains a hard requirement to configure. `SigilIOTransport` links `SigilIOHub` publicly and Boost.Asio,
uWebSockets and `CURL::libcurl` privately — the last one for the
websocket client, which is also where the TLS a `wss://` feed is carried
over comes from, the sockets a listener stands on being built without
any: its header names a hub and a scheme and nothing of
the socket behind them, so a consumer that opens a feed inherits no
executor, no event loop and no Boost. `SigilIOSource` itself depends on
nothing beyond the standard library, so a decoder or an encoder library
can speak the byte vocabulary without inheriting the hub, libcurl or any
codec.

SigilIO owns **access**: URIs, mounts, caching, hot reload, network
fetch, the disk cache, and the file write. SigilImage owns **meaning**:
format sniffing, decode and encode backends, probing, layer and channel
semantics. The hub adds zero format knowledge of its own — every image
ask takes SigilImage's own `DecodeOptions`, every decode is a
delegation, `ResourceInfo` says only how many bytes there are and where
they came from, `probe<T>()` asks T's own library what they mean, and
`write()` takes bytes somebody else encoded. The dependency runs one way
only: SigilImage does not know the hub exists, and does not open a file
in either direction — its prober is declared against a span of bytes and
a name, which is why it costs SigilImage nothing to be askable.

## One file with files inside it

`ArchiveSource` reads a zip out of memory and answers the files inside it
by the names the archive lists them under, which makes a brush pack, a
font pack or a scene bundle the same kind of thing as a directory: names
in, bytes out. It is here rather than inside whichever decoder needed a
zip first, because reading an archive is resource ACCESS — what the files
inside mean is the decoding library's answer, exactly as it is for a file
on disk.

```cpp
if (sigil::io::ArchiveSource::isArchive(bytes)) {
  const sigil::io::ArchiveSource archive(bytes);
  for (const sigil::io::ArchiveEntry& entry : archive.entries())
    …                                        // in the archive's order
  auto described = archive.fetch("brush.json");   // or by name
}
```

Whole, not streamed: construction reads every entry, so a decoder asking
for three files out of one pack pays for one read. Directories are left
out — a name is a path, and what a reader wants is the files under it.

**An archive's directory is a CLAIM.** It states what each entry
decompresses to before a byte of the entry is read, so a two-hundred-byte
file can claim a two-gigabyte entry. An entry claiming more than
`ArchiveSource::kEntryCeiling`, or more than a thousand times the
archive's own size, is left out rather than allocated for, and the honest
entries beside it still arrive. An archive of 2 GiB or more is refused
whole, because the length the reader underneath takes is a signed 32-bit
count and truncating it would read a different file.

## Build and test

From `apps/spell-circle-canvas`:

```sh
python3 scripts/sigil.py setup --config Release
cmake --build build --config Release --target io_test
ctest --test-dir build -C Release --output-on-failure
```

Targets: `SigilIOSource` (`source/` — headers, the archive source, and
the two places only the platform can name, where the running binary
stands and where a process may leave throwaway files) with
`source/test/`, whose `SourceVocabulary`, `SinkVocabulary` and `Places`
suites check the concepts against a fixture source, a fixture decoder and
a fixture sink with no hub in the binary, `writeBytes` against a real
scratch directory, and whose `IOArchive` suite reads zips written by
hand, one of which claims an entry a thousand times the file it is in; `SigilIOHub` (static library, `hub/` — mounts, selection,
cache, retention, network and the decoder registry, split behind the
private `hub/Fetch.h` and `hub/Residency.h`) with `hub/test/`, whose
`IOHub`, `IOSource`, `IOChannels`, `IOResourceLease`, `IONetwork`,
`IOOiio`, `IOTextCatalog` and `IOFeed` suites cover it — `IOSource` being the hub
answering as a `ByteSource`, which is the seam a consumer that only
wants bytes stands on; and `io_bench` (Google Benchmark, built
by the `benches` target and run from a Release build through
`scripts/sigil.py bench`: `Hub::blob` on a cache hit and `load<T>` on a
decoded view per call and `resolve` per URI against the mount table — the
disk kept out of every timed loop); `SigilIOTransport` (static
library, `transport/` — the UDP transport and the one thread its
sockets run on, behind the private `transport/IoThread.h`, the
WebSocket listener and the loop each one holds, and the WebSocket client
and the session each of its feeds runs on a thread of its own) with
`transport/test/`, whose `IOUdp` suite binds real ports on the loopback
and sends its own datagrams through raw sockets, whose `IOWebSocket`
suite does the same with a websocket peer it writes out by hand, upgrade
request and masked frames and all, and with one plain HTTP request for
the pages a listener's query stands that same port over — which a case
may do to prove what the listener answers, while a transport stands on a
library that speaks the protocol instead — and whose `IOWebSocketClient` suite calls a
listener this same process is holding, so both ends of a session stand in
one binary; and `SigilIO`, the umbrella over the source and the hub.

There is one test binary, `io_test`, built from every feature's `test/`
directories, and ctest discovers one entry per CASE out of it, so a
suite or a case is selected by name with no target behind it —
`-R '^IOHub\.'` for the hub's cases, `-R '^Places\.'` for the platform's.

Its cases take their scratch directory from `src/test/ScratchDir.h`, the
repository-level test support header: a directory named after the
case and the process, emptied on the way in and removed on the way out.
The hub cases open most of themselves from a `MountedHub` fixture —
one such directory mounted at `res://`, which is the whole of what a hub
needs before it can be asked anything — and force a distinct mtime
through one `touchForward()` helper rather than by sleeping, since a
filesystem's timestamp granularity is not this test's running time.

Two parts of the hub's cases carry a ctest label, because each needs
something the machine may not have. The `IOOiio` suite is the EXR cases,
which compile only where OpenImageIO is found at configure time — the
test uses it to *write* its fixtures, while the library itself never
calls it — and carries the `oiio` label. The live-network case fetches a
pinned immutable URL once and reads it back through a fresh hub locked
`Offline`; it is a ctest entry of its own,
`IONetwork.LiveFetchThenOfflineRoundTrip`, labelled `network`, and it
skips itself where there is no route, so `-LE network` is how a run
leaves it out rather than how it avoids failing. Every other network
case is a pre-seeded disk cache, with a stub transport standing in for
libcurl where a fetch has to succeed or fail, so libcurl itself is
untested by default.

A case here asserts one thing a header promises and is named that
promise as a sentence. It pins only what editing this library could
falsify — a cache hit, a selection, a URI resolution, bytes in and the
same bytes out — never how many bytes a server happens to hold, nor how
long a fetch took.
