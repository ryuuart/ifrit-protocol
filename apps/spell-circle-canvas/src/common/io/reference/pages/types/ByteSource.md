---
kind: type
library: SigilIO
name: ByteSource
qualified: sigil::io::ByteSource
group: The byte vocabulary
status: stable
---

# ByteSource

## Description

The BYTE SOURCE foundation: the one vocabulary every resource path in
this library and its consumers speaks. A source answers a URI with bytes;
a decoder turns bytes into a value. Nothing here knows what a URI
resolves to or what a byte means — that is what lets a decoder be written
against a test fixture and run unchanged behind a hub, and a hub be
replaced by a pack file or an in-memory table without any decoder
noticing.

`sigil::io::readBytes` is the local-filesystem end of it, the one place
in this tree where a file becomes a run of bytes, as `sigil::io::writeBytes`
beside the sink concept is the one place a run of bytes becomes a file.

The whole vocabulary is standard library only, header only.

### The concepts

`sigil::io::ByteSource` is anything that answers a URI with bytes: null
when the URI cannot be served. The result is shared and immutable, so a
source may hand out a cached copy and a caller may keep it for as long as
it likes.

`sigil::io::ResolvingByteSource` is a byte source that can also say WHERE
a URI's bytes live on the local filesystem — empty when the URI does not
map to a file. Optional: a network or in-memory source has no path to
give.

`sigil::io::Decoder` turns bytes into a T, or nothing when the bytes are
not one. The hint is the resource's name (a path or URI); a decoder may
use its extension to sharpen format detection but must never REQUIRE it
— bytes arriving from memory carry no name. So the hint is OFFERED
rather than demanded: a decoder that reads the bytes alone spells
`decode(bytes)` and is as good a decoder as one that takes both.

### Probing a kind of meaning

`sigil::io::Probable` is WHAT A KIND OF MEANING IS PROBED WITH: a free
function found by argument-dependent lookup in T's own namespace,
answering what the bytes are without decoding them.

```cpp
// declared in the namespace of the type it answers for, sigil::image
std::optional<sigil::image::ImageProbe> probeResource(
    std::type_identity<sigil::image::ImageProbe>,
    std::span<const std::byte>, const std::filesystem::path& hint);
```

The library that owns the meaning declares it, against nothing from the
byte vocabulary but the standard library — a span of bytes and a name.
That is what lets a byte source answer `Hub::probe` for a T it has never
heard of, and keeps deciding what a format is out of the code that only
knows where bytes live. The hint is the resource's name, which a prober
may use to sharpen format detection and must never require.

### Holding any source as a value

`sigil::io::AnyByteSource` is a byte source VALUE holding any byte
source: the type-erased form for code that stores a source rather than
being templated on one. Built from a reference, it borrows — the source
must outlive it. Built from a shared_ptr, it shares ownership.
`AnyByteSource::resolve` answers an empty path for a source that has no
resolve of its own.

### Reading and writing the local file

`sigil::io::readBytes` answers every byte of a path, or nothing when it
cannot be read whole. A file that shrank between the size and the read,
or that could not be opened at all, answers nothing rather than the part
that arrived: a short read is not a shorter resource. An empty file
answers empty bytes, emptiness being a value a resource may have.

`sigil::io::writeBytes` is true only when every byte reached the file and
the stream closed clean, so a half-written file reads as a failure rather
than as a shorter resource. A zero-length write still makes the file:
emptiness is a value a resource may have.

`sigil::io::ByteSink` is anything that stores bytes under a URI: false
when the URI cannot be written to. The mirror of the source concept, so
code that produces a resource can be written against a hub, a scratch
directory or a test double without knowing which it has.

## See also

`sigil::io::Bytes`, `sigil::io::ArchiveSource`, `sigil::io::Hub`.
