# SigilIO

A runtime resource hub. Application code asks for a resource by URI —
`res://ui/logo.png` — instead of by filesystem path. URI prefixes mount
onto directories, results are cached per resource, and a poll re-stats
what has been loaded so edited files reload without a restart. A whole
DIRECTORY or a glob is a resource set: one selector names it, a preload
reads it concurrently, and a lease says how long the hub promises to
keep it. `write()` stores bytes back through the same mounts, and `save()`
a value encoded by the library that owns it. `http://`
and `https://` URIs fetch over libcurl behind an on-disk cache with a
selectable policy; `file://` strips to a plain local path. What a byte
MEANS is not its job in either direction: it hands bytes to the decoders
the libraries that own a meaning register on it — SigilMedia's, SigilData's
— and takes already-encoded bytes back.

Namespace `sigil::io`. One feature library per directory, linked by
what a consumer uses; every public header lives under
`include/sigilio/<feature>/` and is spelled `<sigilio/<feature>/X.h>`:

| target | headers | holds |
|--------|---------|-------|
| `SigilIOSource` | `source/Source.h`, `source/Archive.h`, `source/Sink.h`, `source/Places.h`, `source/State.h` | `FeedState` and `ReadyState`, the one value a feed and a frame subscription answer for where their door stands; the byte vocabulary in both directions: `Bytes`, the `ByteSource`, `ResolvingByteSource`, `Decoder`, `Probable` and `Configurable` concepts with `LoadOptions`, `AnyByteSource` (the type-erased source value), the `ByteSink` concept and `writeBytes()`, the one place a path and a run of bytes become a file; `ArchiveSource` and `ArchiveEntry`, one zip held in memory answering its files by name — and the two places only the platform can name, `executablePath()` and `scratchDirectory(label)` |
| `SigilIOHub`    | `hub/Hub.h`, `hub/Feed.h`, `hub/Recording.h`, `hub/Network.h`, `hub/Problem.h`, `advanced/Time.h`, `advanced/Lease.h`, `advanced/Places.h`, `advanced/Problems.h`, `advanced/Residency.h`, `advanced/Decoding.h`, `advanced/Feeds.h`, `advanced/Network.h`, `advanced/Transport.h`, `testing/Testing.h` | the `Hub` and the `HubOptions` it is made with; `Problem`, what a library could not make of one resource, one per URI on the hub's `problems()`; `ResourceInfo` (a resource's byte size and the file it came from), and `ResourceLease`; `NetworkPolicy`, `NetworkTransport` and `NetworkOptions`, and `NetworkCache` — inspect or populate the persistent cache by URL without constructing its filenames or contacting a server; `Feed`, `Message`, `ListenOptions` and `SendOptions` — the reader's side of a resource that keeps arriving, a copyable handle opened through the hub's `listen()`, which registers every linked transport on its first ask nothing answers, played from a file through its `replay()` and moved forward by `advance()`, whose `Feed::peers()` names the peers a door holds attached now; `Lease` and `onAdvance()` — a callback the same `advance()` drives, for as long as the lease lives; `Recording`, the handle a feed's `record()` hands back, which records until it goes; and `RecordingWriter` and `readRecording()`, the format a feed records itself in; and, for whoever writes a transport rather than reads one, `Inlet`, `TransportEnd` and `Transport` — the producer's side a scheme is opened through and delivers into — with `testing::inletOf()`, the one way a test puts a message on a feed without a socket |
| `SigilIOTransport` | `transport/Transport.h` | `registerTransports()`, which installs on a hub the transports that answer the schemes it is handed and every one of them when it is handed none, with `SharedMemoryWriter` beside it — the UDP transport, one socket per feed on a thread of its own, answering to udp://, to osc:// for messages that are OSC packets and to artnet:// for the universes a lighting desk sends; the WebSocket listener, one of them per feed on a loop of its own, answering to ws:// and, where that URI's query names a directory of pages, answering HTTP GET out of it on the same port, where it names an interface, holding that one alone, and where it names the peers to admit, refusing every other before it becomes a peer, and where it says `frames=text`, sending every message as a text frame; the WebSocket client over libcurl, one session per feed on a thread of its own, which is what ws:// and wss:// open when the URI names a server to call rather than a port to hold; and the shared memory reader, answering to shm://, which is a region another process on this machine wrote and no socket at all, with the writer's end of such a region standing beside it; and the MIDI transport, answering to midi://, which is the controller standing beside the screen — its pads and knobs in at midi://in/NAME, its lights out at midi://out/NAME, and a port made rather than found under virtual:NAME — on the thread the driver itself runs its callbacks on and none of this feature's own; either listener fills `TransportEnd::sendTo`, so a listening feed answers the one sender an arrival names through `Feed::send` with `SendOptions::to`; linked by a consumer that opens a feed over a wire or over a region, and by no other |

`SigilIO` is the umbrella target over the source and the hub; the
transport feature stands outside both, linked only where a network feed is opened. Frame sharing is
likewise an optional feature, linked as `SigilIOFrames`: it defines the hub's `publish()` and
`subscribe()`, with `frames/Frame.h`, `frames/Publisher.h` and `frames/Subscription.h`. The hub is a `ByteSource`;
anything that consumes bytes by URI can be written against the concept
and handed a hub, a fixture, or an `AnyByteSource` holding either.

## Using it

Everything an application asks of the library is on the hub, and every
call names it: read a resource, listen on one that keeps arriving, share
frames with another application.

```cpp
#include <sigilio/frames/Publisher.h>
#include <sigilio/frames/Subscription.h>
#include <sigilio/hub/Hub.h>
#include <sigilmedia/advanced/Resource.h>
#include <sigilmedia/image/Encode.h>

using sigil::media::Image;

sigil::io::Hub hub({.mounts = {{"res://", "/opt/myapp/assets"}}});
// What an image MEANS is SigilMedia's: it puts its own decoders on the
// hub, once, wherever the hub is built.
sigil::media::registerDecoders(hub);

// ── READ A RESOURCE ──────────────────────────────────────────────────────
auto table  = hub.read("res://data/table.bin");      // shared_ptr<const Bytes>, cached per URI
auto shader = hub.text("res://shaders/glow.sksl");   // std::optional<std::string>
auto logo   = hub.load<Image>("res://ui/logo.png");         // what the bytes mean
auto remote = hub.load<Image>("https://example.com/tex.png"); // behind the disk cache
hub.write("res://out/plate.png", encodedPng);        // bytes back out, through the same mounts
hub.save("res://out/still.png", *logo);              // a value back out, encoded by its own library
for (const sigil::io::Problem& problem : hub.problems())  // what a reader could not make of a URI
  showBeside(problem.uri, problem.message, problem.line);

// ── LISTEN, REPLAY, ANSWER ───────────────────────────────────────────────
auto scene = hub.listen("udp://:27020");             // a sigil::io::Feed; every linked transport is ready
if (auto newest = scene.latest())                    // the newest message whole
  draw(*newest->payload);
while (auto message = scene.receive())               // every message since the last receive, in order
  fold(*message->payload, message->sender());        // …and the address that one came from
if (auto message = scene.latest())                   // a listener holds no peer of its own…
  scene.send(reply, {.to = message->sender()});      // …so it answers the one sender that wrote to it
auto take = scene.record(outDir / "scene.feed");     // every message to a file, until take goes
hub.replay("udp://:27020", (outDir / "scene.feed").string());  // that URI, and every later listen() on it, plays the file
scene.state();                                       // readiness, revision, dropped, localAddress, error

// ── SHARE FRAMES WITH OTHER APPLICATIONS ─────────────────────────────────
auto out = hub.publish("syphon://Grimoire");         // spout://NAME where Spout is built
out.send({.texture = texture, .commandBuffer = commands, .width = 1920, .height = 1080});
auto in = hub.subscribe("syphon://Resolume");        // a name nothing publishes yet is waited for
if (std::optional<sigil::io::frames::Frame> frame = in.latest())
  drawTexture(frame->texture, frame->width, frame->height);
```

A `Bytes` is an immutable run read as `data()` and `size()`, as a span —
it converts to `std::span<const std::byte>` wherever one is taken — or as
text with `asText()`, shared and never copied between every reader.
`sigil::io::readBytes()` and `sigil::io::writeBytes()` are the same two
directions for a plain path with no hub in reach.

### Options

Each verb takes at most one options struct, last, with designated
initialisers. A transport's own settings (`?pages=`, `?rate=`)
stay in its URI: they are the door's address.

```cpp
sigil::io::Hub hub({
    .mounts = {{"res://", assetDirectory}, {"shader://", shaderDirectory}},
    .transports = {"udp"},                            // only these linked transports; none named is every one
    .network = {.policy = sigil::io::NetworkPolicy::Offline,
                .cacheDirectory = "/opt/myapp/assets/.netcache",
                .transport = myHttpClient},           // a host's own HTTP stack instead of libcurl
});
auto icon  = hub.load<Image>("res://ui/mark.svg", {.width = 256});      // the decoder's own options
auto layer = hub.load<Image>("res://light/probe.exr", {.layer = "diffuse"});
auto small = hub.listen("udp://:27021", {.capacity = 16});                   // sigil::io::ListenOptions
auto desk  = hub.listen("osc://:9000", {.peer = "udp://desk.local:9001"});   // where send() goes
auto out   = hub.publish("syphon://Grimoire", {.device = {.handle = metalDevice}});
auto in    = hub.subscribe("syphon://Resolume", {.application = "Arena"});
```

`sigil::io::HubOptions`, `sigil::io::NetworkOptions`,
`sigil::io::ListenOptions`, `sigil::io::SendOptions`,
`sigil::io::frames::PublishOptions` and
`sigil::io::frames::SubscribeOptions` are the whole of it; the options of
`load<T>()` are T's own library's.

Every scheme a feed opens over, one line each:

```cpp
auto browsers = hub.listen("ws://:8848/scene");        // every peer that reaches that path
browsers.send(frame);                               // …and one send goes out to all of them
auto staged = hub.listen("ws://:8848/sky?pages=res://sky");  // …and GET serves that directory
auto studio = hub.listen("wss://sky.example:443/scene");  // the same scheme calling out: a server to reach
auto handed = hub.listen("shm://scene");               // a region another process on this machine wrote
auto watched = hub.listen("shm://scene?rate=240");     // …read that many times a second, 120 by default
auto pads = hub.listen("midi://in/Launchpad");         // the controller beside the screen: every message it sends
auto lights = hub.listen("midi://out/Launchpad");      // …and its lights, which send() writes to
auto made = hub.listen("midi://in/virtual:sigil");     // a port other software reaches instead of a controller
```

### Advanced

What a host, a tool or a transport author needs, and a reader of
resources never does, is declared apart, as free functions over the hub:

* `advanced/Time.h` — `advance`, `onAdvance`: the host's clock, which moves every replayed recording and runs what stands on the hub's advance
* `advanced/Lease.h` — `Lease`: the handle a callback stands on the advance by
* `advanced/Places.h` — `mount`, `resolve`, `select`, `poll`: a late mount, the file a URI stands for, the files a selector names, and hot reload
* `advanced/Problems.h` — `reportProblem`, `clearProblem`, `clearProblems`: how a library that reads through the hub says what it could not make of a URI, and how a host clears the list before its program asks again
* `advanced/Residency.h` — `ResourceLease`, `retain`, `preload`, `discardUnretained`, `discardDecoded`: what the hub keeps in memory and reads ahead, and the views one meaning decoded, let go so the next ask decodes again
* `advanced/Decoding.h` — `ResourceInfo`, `registerDecoder`, `probe`: how a library teaches `load<T>()`, under the name T's meaning declares, and what a resource is without decoding it
* `advanced/Feeds.h` — `feeds`: every feed the hub has open
* `advanced/Network.h` — `NetworkCache`, `setNetworkCacheDirectory`, `setNetworkPolicy`, `setNetworkTransport`: the disk cache by URL, and a standing hub's network settings
* `advanced/Transport.h` — `Transport`, `Inlet`, `TransportEnd`, `registerTransport`, `transport`: the producer's side a scheme is opened through

```cpp
#include <sigilio/advanced/Decoding.h>
#include <sigilio/advanced/Places.h>
#include <sigilio/advanced/Problems.h>
#include <sigilio/advanced/Residency.h>
#include <sigilio/advanced/Time.h>
#include <sigilio/advanced/Transport.h>
#include <sigilio/testing/Testing.h>

sigil::io::advance(hub, time);                        // once per frame: recordings advance to this time
sigil::io::Lease reading = sigil::io::onAdvance(hub, [&](auto time) { drain(); });
if (sigil::io::poll(hub)) redraw();                   // once per frame, or on a file-watch event
sigil::io::mount(hub, "plugin://", pluginDirectory);  // a mount made after the hub
sigil::io::reportProblem(hub, {uri, message, line});  // a reader says what it could not make of a URI
sigil::io::reportProblem(hub, uri, error);            // …or files the core::Error it refused with
sigil::io::clearProblems(hub);                        // a host, before its program asks again
auto sksl = sigil::io::select(hub, "shader://**/*.sksl");   // sorted URI snapshot
sigil::io::preload(hub, "shader://");                 // a directory selector reads everything under it
auto authored = sigil::io::retain(hub, {"shader://material/**/*.sksl",
                                        "shader://compose/**/*.sksl"});
authored.include("plugin://**/*.slang");
sigil::io::registerDecoder<Mesh>(hub, ObjParser{});   // any named type: a Decoder<T> object or a function
auto crate = hub.load<Mesh>("res://props/crate.obj");
if (auto info = sigil::io::probe<sigil::io::ResourceInfo>(hub, "res://light/probe.exr"))
  budgetFor(info->byteSize);
if (auto probed = sigil::io::probe<sigil::media::Metadata>(hub, "res://light/probe.exr"))
  useDimensions(probed->width, probed->height);
auto retainedBytes = sigil::io::NetworkCache("/opt/myapp/assets/.netcache")
                         .byteSize("https://example.com/tex.png");
sigil::io::registerTransport(hub, "pigeon", [](std::string_view uri, sigil::io::Inlet inlet) {
  inlet.deliver(bytesOf("hello"), "pigeon://the.roof");  // any thread; nothing once the feed is gone
  return sigil::io::TransportEnd{.localAddress = std::string(uri)};
});
sigil::io::testing::inletOf(scene).deliver(bytesOf("injected"));  // a test's one way in
```

`reference/pages/types/Inlet.md` is the transport author's page: failing
without closing, opening late, and closing from the far side.

## Mental model

A `Hub` holds the mount list, decoder registry and cache. Resource leases keep
explicit claims beside that cache; the retained URI set is observable rather
than an undocumented side effect of having loaded something once.

The two halves are chapters beside this one:
**[reference/SOURCES.md](reference/SOURCES.md)** for the resource that
is fetched once — the mounts, the cache and the poll, writing back,
network URIs, selectors, preloading, leases and the decoder registry —
and **[reference/TRANSPORTS.md](reference/TRANSPORTS.md)** for the
resource that keeps arriving, scheme by scheme, with the recording a
feed is written down as.

## Gotchas

What this library does that a caller would otherwise have to discover
is its own chapter: **[reference/TRAPS.md](reference/TRAPS.md)** — what
is synchronised and what is not, what a poll costs and what it cannot
see, which answers are shared, and what each door does when what it was
asked for is not there.

## Boundary

Dependencies: `SigilIOHub` links `SigilIOSource` and `SigilCoreCallable`
publicly — no image library, and no library of any other meaning — and
`CURL::libcurl`, `SigilCoreSchedule` and Boost.Container privately —
private because they are transport, where a fetch that blocks runs and
the tables no public header shows, while curl remains a hard requirement
to configure. `SigilIOTransport` links `SigilIOHub` publicly and Boost.Asio,
uWebSockets and `CURL::libcurl` privately — the last one for the
websocket client, which is also where the TLS a `wss://` feed is carried
over comes from, the sockets a listener stands on being built without
any. The shared memory reader adds nothing to that line: a region is
what the platform itself names, and the looks at one run on a thread of
the same private kind the datagram sockets stand on — one for every
region a registration opens, made when the first of them opens. The MIDI
transport adds RtMidi, privately, which is the platform's own MIDI stack
behind one class and starts no thread of this feature's at all. Its
header names a hub, a scheme
and a region's own writer, and nothing of the socket, the mapping or the
cable behind them, so a consumer that opens a feed inherits no executor,
no event loop, no Boost, no driver and no stub. `SigilIOSource` itself depends on
nothing beyond the standard library, so a decoder or an encoder library
can speak the byte vocabulary without inheriting the hub, libcurl or any
codec.

SigilIO owns **access**: URIs, mounts, caching, hot reload, network
fetch, the disk cache, and the file write. SigilMedia owns **meaning**:
format sniffing, decode and encode backends, probing, layer and channel
semantics. The hub adds zero format knowledge of its own and names no
image type: a hub starts with no decoder at all, `sigil::media::registerDecoders(hub)`
is SigilMedia putting its own on it, `load<T>(uri, options)` takes the
options T's library declares through `loadOptions()` — SigilMedia's
`ImageOptions` for an image — and finds its decoder by the name T's
library declares through `meaningName()` — "media.Image" — so a program
made of several images, a host and a sketch it compiled and loaded
while running, asks one door whichever image holds the type; every
decode is a delegation,
`ResourceInfo` says only how many bytes there are and where they came
from, `sigil::io::probe<T>()` asks T's own library what they mean, and `write()`
takes bytes somebody else encoded. Neither library links the other:
SigilMedia's registration is a template over whatever hub it is handed,
and its prober and its options are declared against the standard
library alone, which is why it costs SigilMedia nothing to be askable and
this library nothing to ask.

A decoder answers `std::expected<T, sigil::core::Error>`: the T, or the
error that says why the bytes are not one. `load<T>()` stays the forgiving
door — null for a resource it could not make into a T — and lists the
decoder's error among `problems()` under the URI, with the line it names,
as `sigil::io::Problem::error()` reads back; a resource that is simply not
there is no problem. A poll that finds a file changed and its decoder
refusing keeps the views it had, lists the refusal once for that change,
and takes it back when a later change decodes. A decoder answering
`std::optional<T>` is taken too, and its refusal is listed as bytes that do
not read as T's meaning. `readRecording()` answers the same shape.

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
  auto described = archive.read("brush.json");    // or by name
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

[docs/overview/testing.md](../../../docs/overview/testing.md) is the
contract every library here is built, tested and measured under: one
`io_test` over every feature's `test/` and one `io_bench`, ctest one
entry per CASE, what a case may pin, and what a label promises. What is
only true of SigilIO:

Targets: `SigilIOSource` (`source/` — headers, the archive source, and
the two places only the platform can name, where the running binary
stands and where a process may leave throwaway files); `SigilIOHub`
(`hub/` — mounts, selection, cache, retention, network and the decoder
registry, split behind the private `hub/Fetch.h` and
`hub/Residency.h`); `SigilIOTransport` (`transport/` — the wires, with
the one thread kind their sockets and looks run on behind the private
`transport/IoThread.h`); and `SigilIO`, the umbrella
over the source and the hub.

| Suites | What they stand on |
|---|---|
| `SourceVocabulary`, `SinkVocabulary`, `Places` | the concepts against a fixture source, a fixture decoder and a fixture sink with no hub in the binary, and `writeBytes` against a real scratch directory |
| `IOArchive` | zips written by hand, one of which claims an entry a thousand times the file it is in |
| `IOHub`, `IOSource`, `IOChannels`, `IOResourceLease`, `IONetwork`, `IOOiio`, `IOFeed` | the hub, `IOSource` being the hub answering as a `ByteSource` — the seam a consumer that only wants bytes stands on |
| `IOUdp` | real ports on the loopback, with the datagrams sent through raw sockets |
| `IOWebSocket` | a websocket peer written out by hand, upgrade request and masked frames and all, plus one plain HTTP request for the pages a listener's query stands that same port over |
| `IOWebSocketClient` | a listener this same process is holding, so both ends stand in one binary |
| `IOSharedMemory` | regions this binary writes through `SharedMemoryWriter` and reads back, one case writing without pause from a thread of its own while another reads — the only way the count that brackets a message can be shown to work |
| `IOMidi` | a port this binary MAKES and then opens back: a virtual cable by name. Each case skips, with the reason, where the system offers no such port |
| `FramesDelivery` | a `syphon://` publication this binary makes and subscribes to twice, the run loop turned by the case itself, as a command-line receiver turns it |

The hub cases open most of themselves from a `MountedHub` fixture — one
scratch directory mounted at `res://`, which is the whole of what a hub
needs before it can be asked anything — and force a distinct mtime
through one `touchForward()` helper rather than by sleeping, since a
filesystem's timestamp granularity is not this test's running time.

| Label | On | Means |
|---|---|---|
| `oiio` | `IOOiio` | the EXR cases, compiled only where OpenImageIO is found at configure time. The test uses it to *write* its fixtures; the library itself never calls it |
| `gpu` | `FramesDelivery`, `ReceivedFrame` | a Metal device, built on macOS only; each case skips where there is none |
| `network` | `IONetwork.LiveFetchThenOfflineRoundTrip` | a route: the case fetches a pinned immutable URL once and reads it back through a fresh hub locked `Offline`. It skips itself where there is none, so `-LE network` is how a run leaves it out rather than how it avoids failing |

Every other network case is a pre-seeded disk cache, with a stub
transport standing in for libcurl where a fetch has to succeed or fail,
so libcurl itself is untested by default.

`io_bench` times `Hub::read` on a cache hit, `load<T>` on a decoded view
per call and `resolve` per URI against the mount table — the disk kept
out of every timed loop — and THE WIRES' OWN ARMS beside them, one per
door with both ends of that door standing in the binary: a batch of
messages sent through one feed and counted arrived on the feed at the
other, so what such a row says is the messages a second that door
carries end to end.
