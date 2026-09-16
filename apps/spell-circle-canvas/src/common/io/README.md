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

`SigilIOTransport` registers nine transports under eleven schemes, two of
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
rather than out of the bytes. `artnet://` is that same socket once more,
for a port whose datagrams are a lighting desk's universes of dimmers —
`artnet://:6454` listens for them and `artnet://HOST:6454` is a desk to
send them to. Every socket runs on one thread of its own, private to the
transport.

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

`registerSharedMemory()` takes `shm://`, and it is the one transport
here with no network under it. `shm://NAME` maps the shared memory
object of that name and answers what the ONE writer of that region put
there, so a scene crosses from another process on this machine through
memory both of them have mapped, with no socket beneath it and no kernel
on the way. A region opens with a fixed header of sixty-four bytes — the
sixteen bytes `sigil-shared-1` and its padding, then the capacity the
region was made for, a field left spare, a count of the messages written
into it, the size of the one standing now, and the wall-clock nanosecond
it was written at — and the payload begins where that header ends. Every
number stands at a fixed offset in the byte order of the machine both
ends run on, so a writer in another language is a structure definition
and not a library.

THE COUNT IS THE WHOLE PROTOCOL. The writer raises it to an odd number
before it touches the payload and to the next even one once the message
is whole; a reader copies the payload between two reads of that count
and keeps what it copied only when both reads are the same even number.
So a reader never takes half of one message and half of the next, and a
writer is never held up by a reader that is mid-copy: the two ends share
no lock and neither of them makes a system call to speak. What says a
message is NEW is the count and not what the message says, so the same
bytes written twice are two arrivals.

NOTHING PUSHES, SO THE FEED LOOKS. `shm://NAME?rate=HERTZ` reads the
region a whole number of times a second, 120 times where the URI names
no rate, and every message left standing between two looks is one
arrival — while a message written and written over between them is one
the reader missed rather than one it queued, a region holding the
message that stands NOW and no backlog. Every arrival names the region
as its sender, spelled `shm://NAME` exactly as the `address()` the feed
reports is, the rate being the reader's own arrangement and no part of
what the region is called, as a listener's pages are no part of the path
its peers reach. The mapping is read-only and there is no way back
through it, so `send()` and `Feed::sendTo()` are both false on such a
feed: a scene that must answer holds another door for that. A region no
writer has made, one smaller than its own header, one whose first bytes
are not this layout's, and one claiming more payload than it has room
for, open nothing and leave the reason on the feed.

`SharedMemoryWriter` is the other end, for a tool or a test written in
this language — a writer in any other needs the layout and nothing else.
Constructing one takes a region's name and a capacity and makes that
object, replacing whatever stood under the name, sized to hold that many
payload bytes;
`SharedMemoryWriter::write` puts one message in it and is false for a
message larger than that capacity, a message being written whole or not
at all; `SharedMemoryWriter::open` says whether the region stands; and
destruction unmaps the region and takes the name back, after which a
reader opening that name finds nothing while one that already mapped it
goes on reading the message it holds. ONE WRITER PER REGION, a region
carrying one count and not one per writer. And a writer stands BEFORE
its readers: a feed maps what is there when it opens, so a region made
afterwards is a region that feed never sees.

`registerMidi()` takes `midi://`, and what stands behind it is not a
network either: it is the controller on the desk beside the screen, its
pads and knobs coming in and its lights going out. `midi://in/NAME`
opens the first INPUT port whose own name holds NAME, letter for letter
with case left out of it, and `midi://out/NAME` the first output port
that does; NAME left out altogether takes the first port there is,
which is the one controller on a desk that has one. A PORT IS NAMED AND
NEVER NUMBERED, because which port a machine calls its second depends on
what else was plugged in this morning, and a name nobody answers to
opens nothing and leaves on the feed both the name that was looked for
and the ports that do exist — so a name typed from memory is corrected
by reading the sentence. `midi://in/virtual:NAME` and
`midi://out/virtual:NAME` MAKE a port of that name rather than looking
for one, which is how other software on this machine reaches a scene as
it reaches a controller, and a system that does not offer such a port
opens nothing and says so.

An input delivers EVERY MESSAGE THE WIRE CARRIES, one arrival per
message, the bytes exactly as they arrived with the status byte first —
system exclusive included, that being what a controller answers a
question with. The two a scene cannot use are left out: a clock beats
twenty-four times a quarter note and a sensing byte arrives several
times a second whether or not anybody played anything, so a feed taking
both would be a feed of heartbeat with the performance somewhere inside
it. Every arrival names the port as its sender, spelled `midi://in/NAME`
with the port's WHOLE name and not the piece the URI asked for, exactly
as the `address()` such a feed reports is; an output reports
`midi://out/NAME` the same way. An input is ONE WAY — what comes back
down a cable is the other cable, which is a door of its own — so
`send()` and `Feed::sendTo()` are both false on one, while an output's
`send()` writes the bytes as one message and is false where the driver
refused it. No thread is started for any of this: the driver runs a
callback of its own on every arriving message, which is the thread an
arrival is delivered from, and an output is written on the thread that
asked.

`registerSerial()` takes `serial://`, and what stands behind it is the
oldest wire there is: a board on a cable, printing a line whenever it
has something to say. `serial://DEVICE?baud=RATE` opens the device file
of that path — whole and absolute, as in
`serial:///dev/tty.usbmodem1101?baud=115200` — at that rate. THE RATE IS
REQUIRED and there is none to fall back on, because two ends that
disagree about it read each other as noise, so a URI carrying none opens
nothing and says so. The rest of the wire stands in the same query and
has the defaults a board is wired for: `bits` is 5, 6, 7 or 8 and is 8,
`parity` is none, odd or even and is none, `stop` is 1 or 2 and is 1,
`flow` is none, software or hardware and is none. A port that will not
take one of those settings is a door that does not open, since a port
read at a setting nobody asked for answers bytes that are not the ones
on the wire.

A MESSAGE IS A LINE, AND SO IS A REPLY. What reaches a serial port is a
run of bytes with no message boundary anywhere in it, so the boundary is
the one the sender writes: an arrival is the bytes up to a newline, with
a carriage return before it left off and a blank line delivered to
nobody, and half a line is no arrival at all until the rest of it comes
— a reading with its numbers cut in two being worse than a reading that
has not arrived yet. A FEED THAT OPENS ONTO A WIRE ALREADY IN MID-LINE
takes the tail of that line as its first arrival: nothing in the bytes
says where a line began, so a reader that attaches to a board already
talking pays one fragment for it and reads whole lines from then on. A
run of 64 KiB with no newline among them is
handed over as the line it stands as and the reading begins again, so a
sender that frames nothing still reaches a reader instead of growing one
arrival without end. `send()` writes the bytes and a newline after them,
which is where the reader on the board stops.

A CABLE HOLDS ONE PEER. What is at the other end is the only thing
there, so `send()` reaches it and there is no sender to pick out by
name: `Feed::sendTo()` is false on such a feed. Every arrival names the
port as its sender, spelled `serial://DEVICE` with the settings left off
— what the board IS, and not how this end was told to read it — exactly
as the `address()` such a feed reports is. The ports every registration
opens share ONE thread, made when the first of them opens, so a hub
taught the scheme and never asked for a port starts nothing.

`registerGrpc()` takes `grpc://`, and it is one scheme at both ends: the
SHAPE of the URI is what says which end a feed is, as it is for the two
websocket ones, but here one registration takes both. `grpc://:PORT/
Service/Method` holds that port and serves that one method — PORT 0
meaning any free port — and `grpc://HOST:PORT/Service/Method` calls that
method there. BOTH ENDS NAME THE METHOD, a method being a leading slash,
a service and a method with nothing after, so a URI carrying a service
and nothing behind it is half an address and opens neither end.

THE METHOD CARRIES BYTES AND PARSES NOTHING. It is generic at both ends,
which is gRPC's own word for a method with no generated stub standing
over it, so a feed's buffers, its JSON and whatever else a sender writes
cross it exactly as they cross a datagram — and what a message MEANS is
the business of the library that owns the format, as it is on every
other door here. A scene that wants a wire format across gRPC writes
that format and this transport never learns it.

A SERVER'S PEER IS A CALL AND NOT A CALLER. Every call that arrives is a
bidirectional stream of its own and one caller may hold several at once,
so the address alone would not tell two of them apart: an arrival is
named `grpc://ADDRESS#NUMBER`, the number counting the calls that feed
has taken, and `Feed::sendTo()` writes on the call it names. `send()`
writes on every call standing and is false where none is — a door that
holds its own callers can say that a broadcast reached nobody, which a
door that posts to a loop cannot — and a caller that ends its half of
the stream ends that peer, its name answering nothing from then on. The
`address()` such a feed reports is
`grpc://[::]:PORT/Service/Method`, every interface of both families
being one dual-stack listener, and closing the feed gives the port back
and cancels the calls standing on it rather than waiting for a caller to
stop talking. A call asking for any other method is told nothing here
answers to it, a generic service being handed every method a caller
names.

A CLIENT HOLDS THE ONE CALL IT OPENED. `send()` writes one message on
it — and a message handed over before the server has been reached waits
on the stream rather than going nowhere, which is where this end differs
from the websocket client beside it — every message the server writes is
an arrival naming the URI that was called, which is also the `address()`
such a feed reports, and `Feed::sendTo()` is false on it, a client
having the one peer it called. The server ending the call closes the
feed, while a server that goes away mid-conversation fails it with the
sentence saying the call ENDED — which is not the sentence a call that
never reached a server gets, the two being worth telling apart by
reading the reason rather than by guessing which one happened.
A channel that has not connected within ten seconds fails the feed
with the sentence saying so, while a connection nobody takes is refused
at once and says that instead, so a host that swallows the connection
and a host that refuses it both answer rather than leaving a feed
waiting.

NO TLS AT EITHER END in this cut: a server takes callers without it and
a call is made without it, so both belong on a machine or a network
somebody already trusts. A `grpcs://` scheme carrying credentials is the
step after this one and nothing is registered for it. And NO THREAD IS
STARTED for any of this — gRPC runs threads of its own and calls back
onto them, so a server's callers and a client's stream are carried
without this library holding a loop, an executor or a completion queue.
The one thread this transport ever makes is the one a feed let go from
inside such a callback hands its shutdown to, a server being shut down
by waiting for every call's callbacks to end and a callback not being
able to wait for itself.

`registerQuic()` takes `quic://`, and it is one scheme at both ends, the
SHAPE of the URI saying which end a feed is, exactly as the gRPC one
beside it. `quic://:PORT?cert=FILE&key=FILE` holds that port — PORT 0
meaning any free port — and `quic://HOST:PORT` calls an end there.
Everything crosses ONE ENCRYPTED CONNECTION over ONE UDP PORT: the
handshake, the messages and the acknowledgements alike, which is what
lets a connection survive a laptop moving between networks and what
keeps a door down to one port to open on a stage.

A PORT NAMES THE PAIR IT ANSWERS WITH, because a QUIC connection is
encrypted or it is not a connection: there is no unencrypted form of this
door to fall back to, so `cert` and `key` are required and a URI carrying
neither, or one of the two, opens nothing and leaves the reason on the
feed. Each is a path or a URI the hub resolves to one, resolved through
the mount table as the feed opens — so a scene's own certificate stands
beside it at `res://sky/cert.pem` and is named that way — and what the
listener keeps afterwards is what the library read out of the two files,
a feed being able to outlive the hub that opened it. A CALL EITHER TRUSTS
WHAT IT IS SHOWN OR SAYS PLAINLY THAT IT WILL NOT LOOK:
`quic://HOST:PORT?insecure=1` checks no certificate, which is what
reaches a SELF-SIGNED pair — the ordinary case for a machine on a stage,
where nobody signs for anybody. What that gives up is the certainty that
the machine answering is the one the URI named; what stays is that
everything crossing the connection is encrypted all the same. Both ends
name the protocol `sigil-feed/1` in the handshake, so software speaking
something else over that port is refused there rather than left to send
bytes nobody reads.

A MESSAGE IS ONE UNIDIRECTIONAL STREAM. A send opens a stream, writes the
bytes and ends it, and the arrival is delivered when the end of that
stream arrives — so a message keeps its boundary, which a byte stream has
none of, and no message waits behind another, a stream that lost a packet
holding up itself alone. The price is that two messages sent one after
the other may land in the other order, each stream being carried on its
own. A stream that grows past 16 MiB is abandoned rather than held, a
message being one whole thing. `?datagrams=1` on either form sends every
message as a QUIC DATAGRAM instead: unreliable, unordered, and no larger
than one packet on the path carries, so a send of more than that is
false, as is one made before the path has said how large that is.
Datagrams are always TAKEN whatever a door sends, so a peer that writes
one reaches a door whose own URI asked for nothing — which is what lets
one end send state it can afford to lose while the other answers in
streams.

A PORT'S PEER IS A CONNECTION. Every connection that reaches a listening
feed is a peer named `quic://ADDRESS#NUMBER`, the number counting the
connections that feed has taken; `Feed::sendTo()` writes on the
connection it names, `send()` writes on every connection standing and is
false where none is, and a connection that ends ends that peer, its name
answering nothing from then on. The `address()` such a feed reports is
`quic://[::]:PORT`, every interface of both families being one dual-stack
socket, and the query is no part of it: the pair a port answers with is
the door's own arrangement and nothing anybody reaches.

A CALL HOLDS THE ONE CONNECTION IT OPENED. `send()` writes on it, every
message the other end writes is an arrival naming `quic://HOST:PORT` —
which is also the `address()` such a feed reports, the query again being
the door's own arrangement — and `Feed::sendTo()` is false on it, a call
having the one end it reached. The other end ending the connection closes
the feed; a connection that fails after it was reached fails the feed
with the sentence saying it ENDED, which is not the sentence a call that
never reached anything gets, the two being worth telling apart by reading
the reason rather than by guessing which one happened. A call that has
not been answered within ten seconds fails the feed that way too, while a
machine that says at once that nothing is listening on that port ends the
call there and then and says THAT — so a host that swallows the
connection and one that turns it away both answer, rather than either
leaving a feed waiting.

NO THREAD IS STARTED for any of this: the library underneath runs workers
of its own and calls back onto them, so the packets, the encryption, the
streams and the timers are carried without this library holding a loop,
an executor or a socket. The one thread this transport ever makes is the
one a feed let go from inside such a callback hands its shutdown to, a
listener being given back by waiting for it to stop calling back and a
callback not being able to wait for itself. The library itself and the
registration its workers run under are made on the first `quic://` feed
and stand for the life of the process, giving either back being the same
wait.

`registerWebRtc()` takes `webrtc://`, and what it opens is the only door
here with NOTHING IN THE MIDDLE OF IT. A phone on a mobile network and a
scene behind a router have no address for each other, so neither can be
dialled; what they can do is say what addresses they might be reachable
at, hear the other's, and try every pair until one answers — and from
then on every message goes straight between them, over no server at all.
`webrtc://ROOM?signal=URI` is that conversation: ROOM is what it is
called, and the signal is the door the saying crosses.

THE SIGNAL IS A FEED OF THIS SAME HUB, and the SHAPE of its URI is what
says which end this one is, exactly as it is for the two websocket ones.
`?signal=ws://:PORT/PATH` is a port to hold, so the feed WAITS to be
taken up and answers whoever offers — and that door may stand its own
pages, `?signal=ws://:PORT/PATH?pages=URI`, so the page a phone loads,
the socket it opens back and the introduction it makes are one address.
The PORT may be written `0`, which is the kernel's to fill, and the
address a feed reports is how the one it gave is read back.
`?signal=ws://HOST:PORT/PATH` is a server to call, so the feed TAKES A
ROOM UP and offers into it. What crosses that door is JSON text —
`{"kind":"offer","room":…,"sdp":…}`, the same with `"answer"`, and
`{"kind":"candidate","room":…,"candidate":…,"mid":…}` — every one of
them naming its room, so ONE SOCKET CARRIES AS MANY CONVERSATIONS AS
THERE ARE ROOMS ON IT and each door reads only its own. The doors of one
signalling URI share one reading of that feed, draining being taking;
what they do not share it with is anybody else, so a reader that drains
that same door takes introductions away from them.
`?ice=stun:HOST:PORT` names a server asked what address this machine has
to the world, may be written more than once, and is what two ends on
different networks need and two on one network do not.

THE FRAME CARRIES THE INTRODUCTION. `dispatch()` is what reads the
signalling door and answers it, so a handshake takes a few frames rather
than a few microseconds and a host that never dispatches never finishes
one — while what arrives on a channel is not frame-paced at all, being
delivered the moment it lands. An introduction written before its door
can take it waits for the next frame rather than being lost, which is
how a caller's offer stands until its socket has finished its own
handshake.

ONE PEER PER CONNECTION, on a channel named `feed`. An arrival is named
`webrtc://ROOM#NUMBER`, the number counting the peers that feed has
taken and never handed out twice; `Feed::sendTo()` writes on the one it
names, and `send()` writes on every channel standing open, which is what
makes an audience of phones see one sky rather than each its own. A
message larger than what a peer's channel takes is not written to that
peer, a channel carrying whole messages and cutting none in half.

A feed that WAITS reports `webrtc://ROOM?signal=URI` as its
`address()`, the signal spelled as that door BOUND it —
`webrtc://sky?signal=ws://[::]:52341/signal`, the room and the path
standing where the URI had them and the port between them being the one
taken. So `?signal=ws://:0/PATH` is a first-class way to wait: what a
phone has to be pointed at is read off the end holding the door rather
than agreed on beforehand. A feed that TOOK A ROOM UP reports
`webrtc://ROOM`, holding no door anybody dials. The ice servers stand in
neither, being what an end asks its own address of, as a listener's
pages are no part of the path its peers reach.

A CHANNEL THAT CLOSES ENDS ITS PEER, and a peer whose connection found
no route at all ends the same way — on the frame, a connection torn down
inside its own callback being one that waits for itself. AN END THAT
TOOK A ROOM UP TAKES IT UP AGAIN where the conversation it had is over
and it holds no other: it offers once when it opens, so a route that
never came good would otherwise leave that feed standing with nobody at
the other end of it, and the end waiting in the room answers the new
offer as it answers a first one, retiring what it held for that caller.
A SIGNAL THAT ENDS ENDS NO CONVERSATION, though: two ends that have found each other
speak through nothing else, so a door whose signalling socket closed
keeps every peer it has and only takes no new one. Closing the feed ends
every connection and lets the signalling door go with it, and the last
conversation crossing one is what gives that port back. NO THREAD IS
STARTED for any of this: the library underneath runs its own and calls
back onto them.

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
