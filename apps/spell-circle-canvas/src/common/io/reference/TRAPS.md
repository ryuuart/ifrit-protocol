# SigilIO — gotchas

The chapter on what this library does that a caller would otherwise
have to discover: what is synchronised and what is not, what a poll
costs and what it cannot see, which answers are shared and must not be
mutated, and what each door does when it is asked for something that is
not there. `README.md` beside the library is the front page.

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
