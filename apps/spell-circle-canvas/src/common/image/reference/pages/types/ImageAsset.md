---
kind: type
library: SigilImage
name: ImageAsset
qualified: sigil::image::ImageAsset
group: Assets
status: stable
---

# ImageAsset

A decoded image document — still or animated — ready to draw on any
canvas, the offscreen scene canvases included; upload to the GPU happens
implicitly on first draw through a Graphite recorder.

## Description

Formats: PNG, JPEG, WebP, GIF and AVIF, including animation for the
formats that carry it — GIF, animated WebP, animated AVIF. Every frame
is fully composited at decode time, so the source format's disposal and
blend semantics are already applied and drawing frame N never depends on
frame N-1.

Decoding is CPU-side and eager: an asset's frames stay resident for its
lifetime, which fits canvas-drawing workloads — decode once at import,
draw per frame. It is not for streaming video-sized content.

**Bytes in, never a path.** Where bytes come from — files, URIs, caches
— is the resource library's concern, and this type only ever sees
memory.

## Make one

`ImageAsset::decode` decodes encoded bytes, answering nothing when they
are not one of the supported formats or are corrupt. `ImageAsset::probe`
sniffs them instead: dimensions, frame count and format name, with no
pixel decode. `ImageAsset::wrap` takes an already-rendered still image
as a one-frame asset — the bridge for textures generated on an
intermediate canvas or surface, such as procedural nine-slice frames or
baked patterns — and a null image yields an empty asset.

Both doors are the Skia codec path ALONE. They do not route through the
vector or the OpenImageIO backends; only the free functions in the
decode header route.

## Reading one

`ImageAsset::width` and `ImageAsset::height` are every frame's size in
pixels, zero for an empty asset. `ImageAsset::animated` is whether it
carries more than one frame, and `ImageAsset::frames` is every frame in
playback order, already composited.

`ImageAsset::totalDurationMs` is the sum of all frame durations, zero
for a still. `ImageAsset::repetitionCount` is how many times the
animation plays, or `ImageAsset::kInfinite` — the repetition count of an
animation that never stops, which is the common case for animated
stickers, and what a still image reports.

`ImageAsset::frameAt` is the frame to show at a number of milliseconds
since playback start, looping according to the repetition count; a
finished finite animation holds its last frame, and a still image always
answers its one frame.

`Frame` is one decoded frame: `Frame::image`, a premultiplied,
immutable, raster-backed image, and `Frame::durationMs`, how long it
stays on screen, zero for a still.

`ImageProbe` is the cheap metadata read with no pixel decode:
`ImageProbe::width`, `ImageProbe::height`, `ImageProbe::channels`,
`ImageProbe::frames` (above one for an animation),
`ImageProbe::floatingPoint` for an HDR or float source,
`ImageProbe::format` as the format's own name, and
`ImageProbe::layers` and `ImageProbe::channelNames` for a source that
carries them.

## See also

- `asset/ImageAsset.h` — the header: `ImageAsset`, `Frame`, `ImageProbe`
- [ChannelData](ChannelData.md) — the format-neutral escape hatch, for a
  source whose channels matter
- [`embeddedPngs`](../functions/embeddedPngs.md) — recovering encoded
  images from a container with no parser here
