---
kind: function
library: SigilSkia
name: readImage
qualified: sigil::skia::readImage
group: Sampling a texture
status: stable
---

# readImage

A Metal texture's pixels READ BACK INTO HOST MEMORY, as an image any
canvas and any renderer can draw — the answer for a picture that has
to reach a device the texture does not stand on, where a wrap has
nothing either side could hand the other.

## Description

THIS ONE COPIES and the wraps do not, which is the whole difference
between them. An image from here names bytes this process owns, so it
is uploaded again wherever it is drawn and costs a frame of pixels
every time one is made; a wrapped one costs nothing and is drawable on
exactly one recorder. Ask for a wrap wherever a wrap will do.

The copy runs on a command queue this call keeps for the texture's own
device — one queue held for the process, since a queue made per read
would be a queue allocated per read — and the calling thread blocks
until it has finished, which is what having the pixels means.

The texels are read as the format the texture declares, with the
channels in that format's own order; the alpha type says what its alpha
channel means and the colour space what its colours do, with null asking
for no conversion. Null when there is no texture, when the copy did
not complete, or when the format is not four eight-bit channels, which
is the only shape read here.

## See also

- `graphite/TextureImage.h` — the header: `readImage`, `wrapImage`,
  `wrapPlanarImage`
- [wrapImage](page:SigilSkia/functions/wrapImage) — the wrap, which
  copies nothing
