# SigilLoader

A runtime resource hub. Application code asks for a resource by URI —
`res://ui/logo.png` — instead of by filesystem path. URI prefixes mount
onto directories, results are cached per resource, and `poll()` re-stats
what has been loaded so edited files reload without a restart. `http://`
and `https://` URIs fetch over libcurl behind an on-disk cache with a
selectable policy; `file://` strips to a plain local path. Decoding is not
its job: it hands bytes to SigilImage.

Namespace `sigil::loader`. Header `Loader.h`. One class, `Hub`.

## Using it

```cpp
#include <sigilloader/Loader.h>

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

// Network resources need no mount; point the disk cache somewhere durable
// if downloads should survive a reboot.
hub.setNetworkCacheDir("/opt/myapp/assets/.netcache");
hub.setNetworkPolicy(sigil::loader::NetworkPolicy::Offline);
auto remote = hub.image("https://example.com/tex.png");

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

The cache holds one entry per URI. An entry carries the blob, the decoded
image, and the channel data as three independent views, each populated the
first time its accessor is asked — asking for bytes never decodes, and a
later `image()` or `channels()` ask on the same URI decodes the bytes the
entry already holds instead of reading the source again. An `image()` ask
with a layer or an explicit size is a different decode, so it gets its own
entry, keyed by the URI plus the options behind a separator byte no URI
can contain. Every entry also remembers the URI it was asked by, which is
what reloading goes back to — a URI is never re-derived from a key
string, so no character a URI may contain is special.

Network URIs bypass the mount list entirely. A fetch goes through the disk
cache directory, and the entry carries a sentinel timestamp so `poll()`
knows to leave it alone.

Every decode delegates to SigilImage. The hub never inspects bytes.

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

Dependencies: `SigilImage` publicly, `CURL::libcurl` privately — private
because it is pure transport, but a hard requirement to configure.

SigilLoader owns **access**: URIs, mounts, caching, hot reload, network
fetch and the disk cache. SigilImage owns **meaning**: format sniffing,
decode backends, probing, layer and channel semantics. The hub adds zero
format knowledge of its own — `ImageOptions` is an alias for SigilImage's
`DecodeOptions`, `ResourceInfo` carries SigilImage's `ImageProbe`, and
every decode is a delegation. The dependency runs one way only: SigilImage
does not know the hub exists.

## Build and test

From `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Debug
cmake --build build --config Debug --target loader_test
ctest --test-dir build -C Debug -R loader_test --output-on-failure
```

Targets: `SigilLoader` (static library) and `loader_test`.

Two parts of the test suite are conditional. The EXR cases compile only
when OpenImageIO is found at configure time — the test uses it to *write*
its fixtures, while the library itself never calls it. The live-network
cases skip unless `SIGILLOADER_NET_TESTS=1` is set in the environment, so
the default run needs no connectivity.
