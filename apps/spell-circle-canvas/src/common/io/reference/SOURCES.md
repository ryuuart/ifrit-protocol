# SigilIO — resources, and how bytes are found

The chapter on the resource half of the hub: how a URI resolves
through the mount list, what the cache holds and when a poll reloads it,
how bytes go back out the way they came in, what a network URI does
instead, and how a selector, a preload and a lease differ — ending at
the decoder registry, which is the one place a run of bytes acquires a
meaning. `README.md` beside the library is the front page;
`TRANSPORTS.md` is the other half, the resource that keeps arriving.

Mounts map a URI prefix onto a directory, and the **longest matching
prefix wins**, so `res://deep/` can point somewhere other than `res://`.
Re-mounting a prefix replaces it. A URI that matches no mount is tried as
a plain path. A mount is a namespace and not a door into the filesystem
around it: what a URI names is **beneath** the mounted directory, so a
remainder that climbs out through `..` resolves to nothing — for a
fetch, for `resolve()` and for a selector alike. A URI that names a
directory rather than a file answers nothing too: the hub answers bytes.

The cache holds one entry per URI. An entry carries the bytes and one
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
