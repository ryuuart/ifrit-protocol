---
kind: function
library: SigilSkia
name: wrapImage
qualified: sigil::skia::wrapImage
group: Sampling a texture
status: stable
---

# wrapImage

A texture someone else owns, read as an SkImage a draw can SAMPLE — the
other direction from `sigil::skia::OffscreenSurface`, which is the same
texture as a surface a draw can WRITE.

## Two ways to read one

Which is possible is a fact about the devices rather than a preference. A
WRAP copies nothing: what crosses is a name, the API's own texture object
read where it stands, and the image belongs to the recorder that wrapped
it and to no other. A READ copies the pixels into host memory, which is
the only thing left when the drawing that must sample them stands on a
different device — or on a different graphics API — from the one the
texture was made on. `sigil::skia::readImage` is that copy.

## Description

An `id<MTLTexture>` bridged to `void*`, created on the device the
recorder was made from, becomes an image drawn through that recorder,
with no copy.

THE IMAGE HOLDS THE TEXTURE for as long as it lives, so a texture its
owner resizes away or drops under a draw stays valid until the last
image naming it is gone. The texels are read as the format the texture
itself declares; the alpha type says what its alpha channel means and
the colour space what its colours do, with null asking for no
conversion.

Null when there is no texture, no recorder, or the wrap failed. The
recorder is the one whose thread the draw is recorded on — the
context's own, or the one that thread took for itself.

The overload with no width and height is the whole of the texture, at
the size the texture itself reports — for the caller handed a texture
somebody else made, whose extent is that texture's own fact and not one
worth restating. Everything else is the wrap above.

## See also

- `graphite/TextureImage.h` — the header: `wrapImage`, `readImage`,
  `wrapPlanarImage`, `TexturePlane`, `TextureRelease`
- [readImage](page:SigilSkia/functions/readImage) — the copy, for a
  picture that has to cross devices
- [wrapPlanarImage](page:SigilSkia/functions/wrapPlanarImage) — several
  planes as one image
