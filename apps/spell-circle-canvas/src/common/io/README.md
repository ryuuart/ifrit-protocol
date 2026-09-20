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
| `SigilIOTransport` | `transport/Transport.h` | `registerUdp()`, `registerWebSocket()`, `registerWebSocketClient()`, `registerSharedMemory()`, `registerMidi()`, `registerSerial()`, `registerGrpc()`, `registerQuic()`, `registerWebRtc()` and `registerTransports()`, with `SharedMemoryWriter` beside them — the UDP transport, one socket per feed on a thread of its own, answering to udp://, to osc:// for messages that are OSC packets and to artnet:// for the universes a lighting desk sends; the WebSocket listener, one of them per feed on a loop of its own, answering to ws:// and, where that URI's query names a directory of pages, answering HTTP GET out of it on the same port; the WebSocket client over libcurl, one session per feed on a thread of its own, which is what ws:// and wss:// open when the URI names a server to call rather than a port to hold; and the shared memory reader, answering to shm://, which is a region another process on this machine wrote and no socket at all, with the writer's end of such a region standing beside it; and the MIDI transport, answering to midi://, which is the controller standing beside the screen — its pads and knobs in at midi://in/NAME, its lights out at midi://out/NAME, and a port made rather than found under virtual:NAME — on the thread the driver itself runs its callbacks on and none of this feature's own; and the serial transport, answering to serial://, which is the board on a cable printing one line per reading — a device file and a baud rate, one arrival per line and a line out of every send — one port per feed on a thread every port of a registration shares; and the gRPC transport, answering to grpc:// at both ends of one generic method — a server holding grpc://:PORT/Service/Method and a call reaching grpc://HOST:PORT/Service/Method — which carries bytes and parses nothing, so a feed's buffers cross it with no generated stub in the transport, and which starts no thread of this feature's at all; and the QUIC transport, answering to quic:// at both ends of one encrypted connection — a port held at quic://:PORT?cert=FILE&key=FILE and a call reaching quic://HOST:PORT, ?insecure=1 on the call being what reaches the self-signed pair a machine on a stage carries — where a message is one unidirectional stream and ?datagrams=1 makes it one unreliable datagram instead, on threads of the library underneath and none of this feature's; and the WebRTC transport, answering to webrtc://, which is the door with nothing in the middle of it — `webrtc://ROOM?signal=URI` is introduced over the websocket door that signal names, a port to hold or a server to call, and every message afterwards crosses straight between the two ends, one connection per peer and one channel on each, on threads of the library underneath and none of this feature's; either listener fills `OpenedFeed::sendTo`, so a listening feed answers the one sender an arrival names through `Feed::sendTo()`; linked by a consumer that opens a feed over a wire or over a region, and by no other |

`SigilIO` is the umbrella target over the source and the hub, and
`<sigilio/IO.h>` the umbrella header; the transport feature stands
outside both, linked only where a network feed is opened. Native texture
publication is likewise an optional feature, linked as `SigilIOPublish`. The hub is a `ByteSource`;
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

sigil::io::registerTransports(hub);                  // udp://, osc://, artnet://, ws://, wss://, shm://, midi://, serial://, grpc://, quic://, webrtc://
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
auto handed = hub.feed("shm://scene");               // a region another process on this machine wrote
auto watched = hub.feed("shm://scene?rate=240");     // …read that many times a second, 120 by default
auto pads = hub.feed("midi://in/Launchpad");         // the controller beside the screen: every message it sends
auto lights = hub.feed("midi://out/Launchpad");      // …and its lights, which send() writes to
lights->send(noteOn);
auto made = hub.feed("midi://in/virtual:sigil");     // a port other software reaches instead of a controller
auto board = hub.feed("serial:///dev/tty.usbmodem1101?baud=115200");  // a board on a cable: one arrival per line
board->send(command);                                // …and a line back down the same cable
auto watchers = hub.feed("grpc://:27090/Sky/Watch"); // one generic method: every call on it is a caller
watchers->send(frame);                               // …and one send writes on every call standing
auto watching = hub.feed("grpc://sky.local:27090/Sky/Watch");  // the other end: the one call it opened
auto stage = hub.feed("quic://:27100?cert=res://sky/cert.pem&key=res://sky/key.pem");  // one encrypted port
stage->send(frame);                                  // …one stream per message, on every connection standing
auto dialling = hub.feed("quic://sky.local:27100?insecure=1");  // the other end: a self-signed pair, reached anyway
auto loose = hub.feed("quic://sky.local:27100?insecure=1&datagrams=1");  // …unreliable, one packet each
auto room = hub.feed("webrtc://sky?signal=ws://:8849/signal");  // a conversation, introduced over that door
room->send(frame);                                   // …straight to every phone that took it up
auto waiting = hub.feed("webrtc://sky?signal=ws://:0/signal");  // …on a port of the kernel's giving
waiting->address();  // webrtc://sky?signal=ws://[::]:52341/signal — the port it got, and what a phone dials
auto joining = hub.feed("webrtc://sky?signal=ws://sky.local:8849/signal");  // the other end: taking a room up
scene->record(outDir / "scene.feed");                // every arrival from now on, to a recording
hub.mount("udp://:27020", outDir / "scene.feed");    // the next feed() on that URI replays the file
hub.dispatch(seconds);                               // once per frame: recordings advance to this time
```

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

Dependencies: `SigilIOHub` links `SigilIOSource`, `SigilImageDecode` and
Boost.Container publicly and `CURL::libcurl` plus `SigilCoreSchedule`
privately — private because they are transport and where a fetch that
blocks runs, while curl remains a hard requirement to configure. `SigilIOTransport` links `SigilIOHub` publicly and Boost.Asio,
uWebSockets and `CURL::libcurl` privately — the last one for the
websocket client, which is also where the TLS a `wss://` feed is carried
over comes from, the sockets a listener stands on being built without
any. The shared memory reader adds nothing to that line: a region is
what the platform itself names, and the looks at one run on a thread of
the same private kind the datagram sockets stand on — one for every
region a registration opens, made when the first of them opens. The MIDI
transport adds RtMidi, privately, which is the platform's own MIDI stack
behind one class and starts no thread of this feature's at all. The
serial transport adds nothing to the line either: a device file is what
the platform itself names, and the ports opened on one run on a thread
of that same private kind — one for every registration, made when the
first port of it opens. The gRPC transport adds gRPC, privately, which
carries its own transport, its own threads and the protobuf, abseil and
OpenSSL beneath them, and starts no thread of this feature's at all. The
QUIC transport adds msquic, privately, which carries the protocol and
the TLS the protocol is made of inside its own shared library — so there
is no OpenSSL to find beside it — and runs its own workers, calling back
onto them and starting no thread of this feature's either; the one test
binary is where OpenSSL IS found, because a port that has to answer for
itself needs a certificate and a key, and the cases write their own. The
WebRTC transport adds libdatachannel, privately, which carries what a
connection through a router is made of — the routes each end offers, the
encryption they agree on and the stream they open — on threads of its
own and none of this feature's; the door its introductions cross is a
websocket feed of the hub the transport was registered on, so no
signalling server stands beside this library. Its
header names a hub, a scheme
and a region's own writer, and nothing of the socket, the mapping or the
cable behind them, so a consumer that opens a feed inherits no executor,
no event loop, no Boost, no driver and no stub. `SigilIOSource` itself depends on
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
disk kept out of every timed loop — and THE WIRES' OWN ARMS beside them,
one per door with both ends of that door standing in the binary, a batch
of messages sent through one feed and counted arrived on the feed at the
other, so what such a row says is the messages a second that door
carries end to end, the WebRTC arm among them with both ends of one
conversation standing in the one process, which the library under that
door survives now that it comes from the registry with its throws
guarded); `SigilIOTransport` (static
library, `transport/` — the UDP transport and the one thread its
sockets run on, behind the private `transport/IoThread.h`, the
WebSocket listener and the loop each one holds, the WebSocket client
and the session each of its feeds runs on a thread of its own, the
shared memory reader, whose looks run on a thread of that same private
kind, the MIDI transport, which starts no thread at all, the serial
transport, whose ports run on a thread of that same private kind, the
gRPC transport, which starts none either, gRPC calling back onto
threads of its own, the QUIC transport, which starts none either, msquic
running workers of its own, and the WebRTC transport, which starts none either
and whose wire format — the introductions two ends make — stands beside
it behind the private `transport/Introduction.h`) with
`transport/test/`, whose `IOUdp` suite binds real ports on the loopback
and sends its own datagrams through raw sockets, whose `IOWebSocket`
suite does the same with a websocket peer it writes out by hand, upgrade
request and masked frames and all, and with one plain HTTP request for
the pages a listener's query stands that same port over — which a case
may do to prove what the listener answers, while a transport stands on a
library that speaks the protocol instead — whose `IOWebSocketClient` suite calls a
listener this same process is holding, so both ends of a session stand in
one binary, and whose `IOSharedMemory` suite writes the regions it
reads through `SharedMemoryWriter`, so both ends of a region do too —
one of its cases writing without pause from a thread of its own while
another reads, which is the only way the count that brackets a message
can be shown to work — and whose `IOMidi` suite MAKES the port it then
opens back by name, so both ends of a cable stand in one binary with no
controller plugged in, and skips with the reason where the system
offers no port made rather than found, and whose `IOSerial` suite MAKES
the port it then opens, a pseudo-terminal pair whose slave the feed
opens by the path the system named it while the case writes the
readings into the master — a port with a path being the whole of what
that transport asks of a board — and skips with the reason where the
system hands over no such pair, and whose `IOGrpc` suite calls a server
this same process is holding, so both ends of a method stand in one
binary with no stub generated for either, and whose `IOQuic` suite calls
a port this same process is holding, so both ends of a connection stand
in one binary — that port has to answer for itself, so each case WRITES
a self-signed certificate and the key that goes with it into its own
scratch directory with OpenSSL and the call reaches the pair the way a
machine on a stage is reached, and a case that could make no pair says so
rather than passing — and whose `IOWebRtc` suite
STARTS A PEER IN A PROCESS OF ITS OWN — this same binary, run again on
the one case of `IOWebRtcPeer`, which takes the room up, SAYS ON ITS OWN
OUTPUT THAT IT IS UP, keeps saying what its environment told it to say
and echoes back whatever it hears — because a door with nothing in the
middle of it has its two ends on two machines. That line is read back
off the process before anything is judged: a process starting is the
machine's to answer for and the pairing after it is this transport's, so
the two are waited on apart, generously and with no verdict on the
first, and a peer that never came up stands its case down naming the
machine while a pairing that does not come once both ends are up fails
saying the transport did not make it. One case holds both ends in this
process on purpose, a dozen pairs over, since two ends inside one
process share the one thread every end's routes are found on, and what
that case asserts is the process still standing and the pairs carrying a
message at all. Every door in it opens on the port zero names and is
dialled at the port it answers, so nothing here is guessed, and it
dispatches on every look, an introduction crossing on the frame. Its
peer case is skipped where no room was named for it, which is what a
sweep of the whole binary does with it; and `SigilIO`, the umbrella over
the source and the hub.

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
