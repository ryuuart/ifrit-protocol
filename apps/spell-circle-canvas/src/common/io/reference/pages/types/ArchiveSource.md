---
kind: type
library: SigilIO
name: ArchiveSource
qualified: sigil::io::ArchiveSource
group: The byte vocabulary
status: stable
---

# ArchiveSource

## Description

AN ARCHIVE READ AS A BYTE SOURCE: a zip held in memory, whole, whose
files answer to their names the way any other source's resources do.

A brush, a font pack and a scene bundle are all one file with files
inside it, and the reader is the same reader every time — so it lives
beside the byte vocabulary rather than inside whichever decoder needed it
first. What is here is access: names in, bytes out. What the bytes inside
MEAN is the decoding library's, exactly as it is for a file on disk.

Whole, not streamed: an archive small enough to hold in memory is the
only kind this reads, and holding it is what lets every entry be answered
without seeking the source again.

Construction reads every entry; `ArchiveSource::fetch` then answers a
name with the bytes already in hand, so a decoder that asks for three
files out of a brush pays for one read. A name is matched as the archive
spells it. Directories are left out — a name is a path, and what a reader
wants is the files under it.

### The entry ceiling

`ArchiveSource::kEntryCeiling` is THE MOST ANY ONE ENTRY MAY DECOMPRESS
TO, and the reason there is a number here at all: an archive's directory
is a CLAIM, and a two-hundred-byte file may claim a two-gigabyte entry.
The claim is read before a byte of the entry is, so an entry claiming
more than this is left out rather than allocated for. Tens of megabytes
is past anything an authored asset holds and far under what a hostile
claim asks for.

Entries claiming more than the ceiling the constructor was given, or more
than a thousand times the archive's own size, are left out — both bounds
are the same question, which is whether the claim could be true.

## See also

`sigil::io::ArchiveEntry`, `sigil::io::ByteSource`.
