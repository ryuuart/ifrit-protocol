---
kind: type
library: SigilIO
name: Hub
qualified: sigil::io::Hub
group: The hub
status: stable
---

# Hub

## Description

The resource hub: game-engine-style mounted URIs over pluggable decode
backends.

A `Hub` maps URI prefixes onto directories, so application code asks for
"res://ui/logo.png" and never touches the filesystem again. Resources are
cached, hot-reloadable (`sigil::io::poll` re-checks everything previously
loaded), and typed:

```cpp
sigil::io::Hub hub({.mounts = {{"res://", assetsDir}}});
sigil::image::registerDecoders(hub);                    // image meaning
auto bytes = hub.read("res://data/table.bin");
auto text  = hub.text("res://shaders/glow.sksl");
auto img   = hub.load<sigil::image::ImageAsset>(        // stills+anim
    "res://ui/logo.png");
auto hdr   = hub.load<sigil::image::ImageAsset>(        // OIIO: EXR,
    "res://light/probe.exr", {.layer = "diffuse"});     //  PSD, TIFF…
auto info  = sigil::io::probe<sigil::io::ResourceInfo>(hub,        // size, path
    "res://light/probe.exr");
auto meta  = sigil::io::probe<sigil::image::ImageProbe>(hub,        // meaning,
    "res://light/probe.exr");                           //  from image
sigil::io::registerDecoder<Mesh>(hub, parseMesh);                   // any T
auto mesh  = hub.load<Mesh>("res://props/crate.obj");
```

### One entry per URI, one view per decoded type

Each URI is cached as one entry whose bytes and decoded views are
independent: each populates the first time its accessor is asked, and
asking for one never affects another. A view is one decoded type — each
type `Hub::load` is asked for populates its own — and a `Hub::load` ask
with options other than the type's defaults, an image with a layer or an
explicit size, is a different decode that gets its own entry. `sigil::io::poll` re-stats every previously
requested resource and reloads the changed ones, returning true so hosts
can re-render (holders of old shared_ptrs keep the old data — swap by
re-asking). Failed lookups are NOT cached: a missing file loads as soon
as it appears.

Calls on one `Hub` may overlap: mount, decoder, cache and retention state
are synchronized internally. A `Hub` satisfies `sigil::io::ByteSource` and
`sigil::io::ResolvingByteSource`.

### The network half

http:// and https:// URIs bypass mounts and fetch over the network
(libcurl: redirects followed, 20s timeout, HTTP errors fail). Successful
fetches persist in an on-disk cache (the platform cache location /
"SigilIO/network" — ~/Library/Caches on macOS, $XDG_CACHE_HOME or
~/.cache elsewhere — so a fetch outlives the temp directory's eviction
policy; override with `NetworkOptions::cacheDirectory` in `HubOptions::network` — point it at an
asset dir to keep downloads beside the assets). Under the default
`NetworkPolicy::CacheFirst` policy a cache hit never touches the network,
so offline runs keep working with no flag to set; `NetworkOptions::policy`
picks `NetworkPolicy::Refresh` (network first, cache as the fallback) or
`NetworkPolicy::Offline` (cache only) when a host wants the explicit
behavior. `sigil::io::setNetworkPolicy` and its two siblings in
`<sigilio/advanced/Network.h>` change a standing hub. file:// URIs strip to plain local paths. `sigil::io::poll` skips
network entries: they carry no mtime to watch.

### Frames shared with other applications

`Hub::publish` offers drawn frames under the name a `syphon://` (Metal,
macOS) or `spout://` (Direct3D11) URI carries, and `Hub::subscribe`
receives another application's; both are defined by `SigilIOFrames`,
which a caller links to reach them, and both answer an empty handle for
anything they cannot carry.

```cpp
auto out = hub.publish("syphon://SpellCircle");
out.send({.texture = texture, .commandBuffer = commands, .width = 1920, .height = 1080});
auto in = hub.subscribe("syphon://Resolume", {.application = "Arena"});
if (auto frame = in.latest()) drawTexture(frame->texture, frame->width, frame->height);
```

### A resource that keeps arriving

A resource that keeps ARRIVING is a feed rather than a fetch, and the hub
is the door on it too:

```cpp
sigil::io::registerTransport(hub, "udp", openUdpFeed);     // one per scheme
auto scene = hub.listen("udp://:27020");        // the same feed per URI
auto lease = sigil::io::onAdvance(hub, readTheScene);    // driven by that same call
sigil::io::advance(hub);                               // once per frame
if (auto newest = scene.latest()) draw(*newest->payload);
```

`Hub::listen` hands back the one feed a URI names for as long as anybody
holds it. A URI named to `Hub::replay` is played back from its recording
as `sigil::io::advance` moves time forward, so the same code reads a live
sender and a recorded session; anything else opens
through the transport registered for its scheme. That one call also runs
every callback registered through `sigil::io::onAdvance`, so something that
reads feeds on the frame is driven by the call a host already makes and
no host code has to name it.

Any other URI opens through the transport registered for its scheme — the
part before "://" — called outside the hub's lock. No scheme, no
transport, or an unreadable recording: the feed exists and its
`FeedState::error` says why.

`sigil::io::onAdvance` runs its callback on every advance for as long as the
lease lives. It is given the time that advance was given, and runs
after every replayed recording has been advanced to them, on the
advancing thread, in the order the callbacks were registered — so a
callback sees what this same advance delivered. That is how something
reading feeds on the frame is driven by the call a host already makes,
with no host code naming it. A callback registered from inside an advance
runs from the next one.

### Access, not meaning

SigilIO owns ACCESS: where bytes come from, caching, reload. A `Hub` is a
`sigil::io::ByteSource`: `Hub::read` answers a URI with bytes, and every
typed view is a registered `sigil::io::Decoder` run over those bytes.
What pixels mean is SigilImage's concern — the Skia codecs plus, when
built in, the OpenImageIO backend (EXR with layer selection, PSD, TIFF,
HDR; float sources land as RGBA_F32) — and SigilImage registers those
decoders on a hub itself, through `sigil::image::registerDecoders`; a hub
registers none of its own.

`Hub::write` stores bytes under a URI through the same mount table a read
resolves by, creating the directories above the file. What the bytes MEAN
is nobody's business here: a caller with an image encodes it first and
hands the result over. Every cached view of that URI is dropped, so the
next ask reads the file back rather than serving what was there before
the write.

`sigil::io::registerDecoder` replaces the decoder later asks use; a view
already decoded keeps its value and the decoder that made it, which is
what `sigil::io::poll` re-runs for it. The hint is OFFERED: a decoder that
reads the bytes alone takes `[](const Bytes& bytes) {…}`.

### Selecting a set

`sigil::io::select` answers the regular-file URIs a selector names, in lexical
order. An exact file selects itself. A directory URI selects every
regular file below it recursively. In a glob, `*` matches within one path
segment, `?` matches one non-separator character, and `**` crosses `/`; a
backslash quotes the next character. Selection enumerates local
filesystem resources (mounted URIs, file:// URLs and plain paths) without
reading file contents. A network selector without a star is one exact URL
and selects itself without a fetch; network globs cannot be enumerated.

### Probing

`sigil::io::probe` answers WHAT THE BYTES ARE, WITHOUT DECODING THEM. Asked
for a `sigil::io::ResourceInfo` it answers HOW MANY BYTES, AND WHERE: the
size of the resource and the file it was read from, which the hub itself
declares probeable. Asked for another T it answers what the bytes mean —
dimensions and layers for an image, and whatever the next kind of meaning
turns out to need. That answer comes from T's own library through the
`sigil::io::Probable` seam, so this hub carries no opinion about any
format — `sigil::io::probe<sigil::image::ImageProbe>(hub, uri)` reads SigilImage's
prober, and a kind of meaning added tomorrow is one free function in the
library that owns it, with nothing to change here.

Every probe is const but neither cheap nor side-effect-free: every call
performs a full fetch of the resource and caches nothing in the hub. For
a network URI that can mean a network round trip and a write into the
disk cache directory.

## See also

`sigil::io::Feed`, `sigil::io::ResourceLease`, `sigil::io::Lease`.
