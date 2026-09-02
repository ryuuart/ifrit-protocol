# SigilLoader

A runtime resource hub. Application code asks for a resource by URI —
`res://ui/logo.png` — instead of by filesystem path. URI prefixes mount
onto directories, results are cached per resource, and `poll()` re-stats
what has been loaded so edited files reload without a restart. `write()`
stores bytes back through the same mounts. `http://`
and `https://` URIs fetch over libcurl behind an on-disk cache with a
selectable policy; `file://` strips to a plain local path. What a byte
MEANS is not its job in either direction: it hands bytes to registered
decoders, SigilImage's by default, and takes already-encoded bytes back.

Namespace `sigil::loader`. One feature library per directory, linked by
what a consumer uses; every public header lives under
`include/sigilloader/<feature>/` and is spelled `<sigilloader/<feature>/X.h>`:

| target | headers | holds |
|--------|---------|-------|
| `SigilLoaderSource` | `source/Source.h`, `source/Sink.h` | header only, standard library only: `Bytes`, the `ByteSource`, `ResolvingByteSource` and `Decoder` concepts, `AnyByteSource` (the type-erased source value), and the other direction — the `ByteSink` concept and `writeBytes()`, the one place a path and a run of bytes become a file |
| `SigilLoaderHub`    | `hub/Hub.h`, `hub/Network.h` | the `Hub` and `ResourceInfo`; `NetworkPolicy` and `networkCacheKey()` |

`SigilLoader` is the umbrella target over both, and
`<sigilloader/Loader.h>` the umbrella header. The hub is a `ByteSource`;
anything that consumes bytes by URI can be written against the concept
and handed a hub, a fixture, or an `AnyByteSource` holding either.

## Using it

```cpp
#include <sigilloader/hub/Hub.h>

sigil::loader::Hub hub;
hub.mount("res://", "/opt/myapp/assets");

auto shader = hub.text("res://shaders/glow.sksl");   // std::optional<std::string>
auto table  = hub.blob("res://data/table.bin");      // shared_ptr<const Blob>
auto logo   = hub.image("res://ui/logo.png");        // stills and animations
auto icon   = hub.image("res://ui/mark.svg", {.width = 256});
auto layer  = hub.image("res://light/probe.exr", {.layer = "diffuse"});
auto planes = hub.channels("res://light/probe.exr"); // every raw channel

if (auto info = hub.probe("res://light/probe.exr"))
  useDimensions(info->image.width, info->image.height);

// Any type, once its decoder is registered: a Decoder<T> object or a
// function from bytes (and the resource's name as a hint) to optional<T>.
hub.registerDecoder<Mesh>(ObjParser{});
auto crate = hub.load<Mesh>("res://props/crate.obj");   // shared_ptr<const Mesh>

// The hub as a ByteSource, for code that only wants bytes by URI.
sigil::loader::AnyByteSource source(hub);
auto raw = source.fetch("res://data/table.bin");

// Network resources need no mount; point the disk cache somewhere durable
// if downloads should survive a reboot.
hub.setNetworkCacheDir("/opt/myapp/assets/.netcache");
hub.setNetworkPolicy(sigil::loader::NetworkPolicy::Offline);
auto remote = hub.image("https://example.com/tex.png");

// Bytes back out, through the same mount table they are read by. What
// they are is the caller's business: an image is encoded first.
sk_sp<SkData> png = sigil::image::encodeImage(*rendered,
                                              sigil::image::Format::Png);
hub.write("res://out/plate.png", png->data(), png->size());

// A plain path, with no hub in reach — the sink half on its own.
sigil::loader::writeBytes(outDir / "plate.png", png->data(), png->size());

// Once per frame, or on a file-watch event.
if (hub.poll())
  redraw();
```

## Mental model

A `Hub` holds two things: a mount list and a cache.

Mounts map a URI prefix onto a directory, and the **longest matching
prefix wins**, so `res://deep/` can point somewhere other than `res://`.
Re-mounting a prefix replaces it. A URI that matches no mount is tried as
a plain path.

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

Every decode is a registered decoder. The constructor registers
SigilImage's two — `ImageAsset` and `ChannelData` — and
`registerDecoder<T>()` adds any other; registering a type again replaces
its decoder, and views already decoded keep their values until `poll()`
reloads them. `load<T>()` with no decoder registered for `T` answers null
without fetching. The hub never inspects bytes.

## Gotchas

`Hub` has no synchronization of any kind. It is not safe for concurrent
use — and that includes concurrent reads, since a lookup that misses
inserts into the cache.

`probe()` is `const` but is not cheap and is not side-effect-free: it
performs a full fetch on every call and caches nothing. For a network URI
it may hit the network and write into the cache directory.

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

`write()` refuses a network URI. A hub writes where it mounts; the disk
cache under an `http(s)://` entry belongs to the fetch, and writing into
it would invent a resource the server never served.

`writeBytes()` is true only when every byte reached the file and the
stream closed clean, so a half-written file reads as a failure rather than
as a shorter resource. A zero-length write still creates the file:
emptiness is a value a resource may have.

`networkCacheKey` builds its filename from `std::hash<std::string_view>`,
which is implementation-defined. Cache directories are therefore not
portable across standard library implementations — treat them as local
scratch, not as a shippable artifact.

Network fetches follow redirects, time out after 20 seconds, fail on any
HTTP status of 400 or above, and buffer the whole body in memory.
Persisting to the cache is best-effort: a fetch that cannot be written to
disk still returns its bytes. `curl_global_init` runs lazily on the first
fetch and `curl_global_cleanup` is never called.

The policies differ in their failure behaviour, which is the part that
matters. `CacheFirst` (the default) serves a present cache file without
touching the network at all, so a run goes offline for free once a
resource has been seen. `Refresh` asks the network first and falls back to
the cached copy when the fetch fails, so a flaky network degrades to
`CacheFirst` rather than erroring. `Offline` never touches the network:
cache hit or failure.

## Boundary

Dependencies: `SigilLoaderHub` links `SigilLoaderSource` and
`SigilImageDecode` publicly and `CURL::libcurl` privately — private
because it is pure transport, but a hard requirement to configure. `SigilLoaderSource` itself depends on
nothing beyond the standard library, so a decoder or an encoder library
can speak the byte vocabulary without inheriting the hub, libcurl or any
codec.

SigilLoader owns **access**: URIs, mounts, caching, hot reload, network
fetch, the disk cache, and the file write. SigilImage owns **meaning**:
format sniffing, decode and encode backends, probing, layer and channel
semantics. The hub adds zero format knowledge of its own —
every image ask takes SigilImage's own `DecodeOptions`,
`ResourceInfo` carries SigilImage's `ImageProbe`, every decode is a
delegation, and `write()` takes bytes somebody else encoded. The
dependency runs one way only: SigilImage does not know the hub exists,
and does not open a file in either direction.

## Build and test

From `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Debug
cmake --build build --config Debug --target loader_source_test loader_hub_test
ctest --test-dir build -C Debug -R loader_ --output-on-failure
```

Targets: `SigilLoaderSource` (header only, `source/`) with
`loader_source_test`, which checks the concepts against a fixture source,
a fixture decoder and a fixture sink with no hub in the binary, and
`writeBytes` against a real scratch directory; `SigilLoaderHub` (static
library, `hub/` — mounts, cache, network and the decoder registry one
translation unit each behind the private `hub/Fetch.h`) with
`loader_hub_test` and `loader_hub_bench` (Google Benchmark, built by the
`benches` target and run from a Release build through
`scripts/bench_ledger.py`: `Hub::blob` on a cache hit and `load<T>` on a
decoded view per call, `resolve` per URI against the mount table, and
`networkCacheKey` per URL — the disk kept out of every timed loop); and
`SigilLoader`, the umbrella.

Both binaries take their scratch directory from `src/test/ScratchDir.h`,
the repository-level test support header: a directory named after the
case and the process, emptied on the way in and removed on the way out.
`loader_hub_test` opens most of its cases from a `MountedHub` fixture —
one such directory mounted at `res://`, which is the whole of what a hub
needs before it can be asked anything — and forces a distinct mtime
through one `touchForward()` helper rather than by sleeping, since a
filesystem's timestamp granularity is not this test's running time.

Two parts of `loader_hub_test` are conditional. The EXR cases compile only
when OpenImageIO is found at configure time — the test uses it to *write*
its fixtures, while the library itself never calls it. The live-network
case fetches a pinned immutable URL once and reads it back through a
fresh hub locked `Offline`, and it skips unless `SIGILLOADER_NET_TESTS=1`
is set in the environment. Every other network case is a pre-seeded disk
cache, so the fetch path itself is untested by default and the default
run needs no connectivity at all.
