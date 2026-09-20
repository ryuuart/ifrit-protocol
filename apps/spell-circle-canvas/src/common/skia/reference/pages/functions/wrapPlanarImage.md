---
kind: function
library: SigilSkia
name: wrapPlanarImage
qualified: sigil::skia::wrapPlanarImage
group: Sampling a texture
status: stable
---

# wrapPlanarImage

ONE IMAGE OUT OF SEVERAL PLANES that each stand on the device — a
frame decoded straight into luma and chroma textures, most often —
wrapped as the YUVA description says, with no copy and no conversion
pass: the shader that samples the image does the arithmetic the
description names.

## Description

The planes are in the order the description declares, and there are as
many of them as it says. NOTHING IS RETAINED HERE: the planes live as
long as the release context keeps them, and the release is called with
it once the image is gone — which is the shape a decoder wants, holding
the one buffer every plane was made from rather than each plane in turn.

Null when a plane is missing, the description does not fit the planes,
or the wrap failed; the release runs either way, so a caller hands its
context over exactly once.

Named apart from `sigil::skia::wrapImage` because the ownership is the
other way round: that one retains the texture it is given, this one
retains nothing and hands the planes back through the release.

`sigil::skia::TexturePlane` is ONE PLANE of a wrapped image: the API's
own texture object and the size it is read at. A plane's size is its own
— a chroma plane of a subsampled frame is half the luma's. Its `texture`
is an `id<MTLTexture>` bridged to `void*` on Apple.
`sigil::skia::TextureRelease` is what frees the textures a planar wrap
named, once the image is gone, and what it is handed.

## See also

- `graphite/TextureImage.h` — the header: `wrapPlanarImage`,
  `TexturePlane`, `TextureRelease`, `wrapImage`
- [wrapImage](page:SigilSkia/functions/wrapImage) — one plane, retained
