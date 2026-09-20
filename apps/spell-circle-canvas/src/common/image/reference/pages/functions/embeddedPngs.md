---
kind: function
library: SigilImage
name: embeddedPngs
qualified: sigil::image::embeddedPngs
group: Assets
status: stable
---

# embeddedPngs

Every PNG carried inside another file's bytes, found by its own
signature rather than by the container's index.

## Description

A great many authoring formats — an animation runtime's scene file, a
game archive, a document with its illustrations inlined — store whole
encoded images end to end inside one blob, and a reader that has no
parser for the container can still recover every one of them: an
encoded image says where it begins and where it ends, and the bytes
between two of them usually carry the name the author gave the one that
follows.

Each image is bounded exactly. The scan walks the chunk lengths from the
signature to the end marker, so the range is the whole encoded image and
nothing after it. A truncated or malformed image ends the scan rather
than yielding a range that runs off the end — the bytes after a broken
chunk table cannot be trusted to be an image either.

## This is a last resort, and says so

It knows nothing about the container, so it cannot tell an image the
container references from one it has abandoned, it cannot see an image
the container stored compressed, and the name it recovers is whatever
printable run stood closest before the signature — which is the author's
asset name in every format that writes one, and something else entirely
in a format that does not. **Where a container has a parser, use the
parser.**

## What comes back

`EmbeddedImage` is one image found inside a larger file: the half-open
byte range it occupies — `EmbeddedImage::offset` and
`EmbeddedImage::length` — and `EmbeddedImage::name`, the last printable
run of at least the minimum length standing between the previous image's
end and this one's start, empty where there was none.

The range is handed back rather than the decoded pixels because a caller
usually wants only a few of the images a container holds, and decoding
the rest to find out which is the whole cost of the read.

`EmbeddedScan` is how the blob is read.
`EmbeddedScan::minimumNameLength` is how long a printable run has to be
before it counts as a name, because short runs in a binary stream are
usually a container's own tags and type codes rather than anything an
author typed. `EmbeddedScan::limit` stops the scan after that many, zero
meaning every one, so a caller that wants the first thumbnail out of a
large archive pays for one image and not for all.

## See also

- `asset/Embedded.h` — the header: `embeddedPngs`, `EmbeddedImage`,
  `EmbeddedScan`
- [ImageAsset](../types/ImageAsset.md) — what a recovered range decodes
  to
