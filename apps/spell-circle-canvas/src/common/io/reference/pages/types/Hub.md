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
cached, hot-reloadable (`Hub::poll` re-checks everything previously
loaded), and typed:

```cpp
hub.mount("res://", assetsDir);
auto bytes = hub.fetch("res://data/table.bin");
auto text  = hub.text("res://shaders/glow.sksl");
auto img   = hub.image("res://ui/logo.png");            // stills+anim
auto hdr   = hub.image("res://light/probe.exr",         // OIIO: EXR,
                       {.layer = "diffuse"});           //  PSD, TIFF…
auto info  = hub.probe("res://light/probe.exr");        // size, path
auto meta  = hub.probe<sigil::image::ImageProbe>(       // meaning,
    "res://light/probe.exr");                           //  from image
hub.registerDecoder<Mesh>(parseMesh);                   // any T
auto mesh  = hub.load<Mesh>("res://props/crate.obj");
```

### One entry per URI, one view per decoded type

Each URI is cached as one entry whose bytes and decoded views are
independent: each populates the first time its accessor is asked, and
asking for one never affects another. A view is one decoded type —
`Hub::image`, `Hub::channels` and `Hub::load` each populate their own —
and a `Hub::image` ask with a layer or an explicit size is a different
decode that gets its own entry. `Hub::poll` re-stats every previously
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
policy; override via `Hub::setNetworkCacheDirectory` — point it at an
asset dir to keep downloads beside the assets). Under the default
`NetworkPolicy::CacheFirst` policy a cache hit never touches the network,
so offline runs keep working with no flag to set; `Hub::setNetworkPolicy`
picks `NetworkPolicy::Refresh` (network first, cache as the fallback) or
`NetworkPolicy::Offline` (cache only) when a host wants the explicit
behavior. file:// URIs strip to plain local paths. `Hub::poll` skips
network entries: they carry no mtime to watch.

### A resource that keeps arriving

A resource that keeps ARRIVING is a feed rather than a fetch, and the hub
is the door on it too:

```cpp
hub.setFeedTransport("udp", openUdpFeed);     // one per scheme
auto scene = hub.feed("udp://:27020");        // the same feed per URI
auto lease = hub.onDispatch(readTheScene);    // driven by that same call
hub.dispatch();                               // once per frame
if (auto bytes = scene->latest()) draw(*bytes);
```

`Hub::feed` hands back the one feed a URI names for as long as anybody
holds it. A URI that resolves through the mount table to a file is played
back from that recording as `Hub::dispatch` moves time forward, so the
same code reads a live sender and a recorded session; anything else opens
through the transport registered for its scheme. That one call also runs
every callback registered through `Hub::onDispatch`, so something that
reads feeds on the frame is driven by the call a host already makes and
no host code has to name it.

A mount whose remainder is empty resolves with a trailing separator; it
is stripped, so mounting a URI directly onto a recording file works. Any
other URI opens through the transport registered for its scheme — the
part before "://" — called outside the hub's lock. No scheme, no
transport, or an unreadable recording: the feed exists and its
`Feed::error` says why.

`Hub::onDispatch` runs its callback on every dispatch for as long as the
lease lives. It is given the seconds that dispatch was given, and runs
after every replayed recording has been advanced to them, on the
dispatching thread, in the order the callbacks were registered — so a
callback sees what this same dispatch delivered. That is how something
reading feeds on the frame is driven by the call a host already makes,
with no host code naming it. A callback registered from inside a dispatch
runs from the next one.

### Access, not meaning

SigilIO owns ACCESS: where bytes come from, caching, reload. A `Hub` is a
`sigil::io::ByteSource`: `Hub::fetch` answers a URI with bytes, and every
typed view is a registered `sigil::io::Decoder` run over those bytes.
What pixels mean is SigilImage's concern — the Skia codecs plus, when
built in, the OpenImageIO backend (EXR with layer selection, PSD, TIFF,
HDR; float sources land as RGBA_F32) — and the hub registers those
decoders by default.

`Hub::write` stores bytes under a URI through the same mount table a read
resolves by, creating the directories above the file. What the bytes MEAN
is nobody's business here: a caller with an image encodes it first and
hands the result over. Every cached view of that URI is dropped, so the
next ask reads the file back rather than serving what was there before
the write.

`Hub::registerDecoder` replaces the decoder later asks use; a view
already decoded keeps its value and the decoder that made it, which is
what `Hub::poll` re-runs for it. The hint is OFFERED: a decoder that
reads the bytes alone takes `[](const Bytes& bytes) {…}`.

### Selecting a set

`Hub::select` answers the regular-file URIs a selector names, in lexical
order. An exact file selects itself. A directory URI selects every
regular file below it recursively. In a glob, `*` matches within one path
segment, `?` matches one non-separator character, and `**` crosses `/`; a
backslash quotes the next character. Selection enumerates local
filesystem resources (mounted URIs, file:// URLs and plain paths) without
reading file contents. A network selector without a star is one exact URL
and selects itself without a fetch; network globs cannot be enumerated.

### Probing

`Hub::probe` answers HOW MANY BYTES, AND WHERE: the size of the resource
and the file it was read from. It is const but neither cheap nor
side-effect-free: every call performs a full fetch of the resource and
caches nothing in the hub. For a network URI that can mean a network
round trip and a write into the disk cache directory.

The template `Hub::probe` answers WHAT THE BYTES MEAN, WITHOUT DECODING
THEM: dimensions and layers for an image, and whatever the next kind of
meaning turns out to need. The answer comes from T's own library through
the `sigil::io::Probable` seam, so this hub carries no opinion about any
format — `hub.probe<sigil::image::ImageProbe>(uri)` reads SigilImage's
prober, and a kind of meaning added tomorrow is one free function in the
library that owns it, with nothing to change here. It fetches as the
untyped probe does, and caches nothing.

## See also

`sigil::io::Feed`, `sigil::io::ResourceLease`, `sigil::io::DispatchLease`.
