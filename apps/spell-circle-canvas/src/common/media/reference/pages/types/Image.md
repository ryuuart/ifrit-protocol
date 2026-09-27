---
kind: type
library: SigilMedia
name: Image
qualified: sigil::media::Image
group: Frames, documents and sources
status: stable
---

# Image

A decoded image document — still or animated — ready to draw on any
canvas; upload to the GPU happens implicitly on first draw through a
Graphite recorder. A still is a one-frame document, so one `frameAt`
reads either, as it reads a `Video`.

## Description

Formats: PNG, JPEG, WebP, GIF and AVIF, including animation for the
formats that carry it — GIF, animated WebP, animated AVIF — KTX, and
through the optional backends SVG, EXR, PSD, TIFF, HDR and DDS. Every
frame is fully composited at decode time, so the source format's
disposal and blend semantics are already applied and drawing frame N
never depends on frame N-1.

Decoding is CPU-side and eager: a document's frames stay resident for
its lifetime, which fits canvas-drawing workloads — decode once at
import, draw per frame. It is not for streaming video-sized content,
which is `Video`.

**Bytes in, never a path.** Where bytes come from — files, URIs, caches
— is the resource library's concern, and this type only ever sees
memory.

## Make one

`hub.load<sigil::media::Image>(uri)` through a hub this library's
decoders were registered on, or `sigil::media::decode` over bytes in
hand; both take `ImageOptions` — `ImageOptions::layer` for an EXR
channel group, `ImageOptions::width` and `ImageOptions::height` for a
vector source's raster size.

`Image::of` takes an already-rendered picture as a one-frame document —
the bridge for textures generated on an intermediate canvas or surface,
such as procedural nine-slice frames or baked patterns — and a null
picture yields an empty document. A decoder builds one from its frames
and repetition count with the constructor, which places each frame's
time and index from the durations before it.

## Reading one

`Image::size` is every frame's size in pixels as a `glm::ivec2`, zero
for an empty document. `Image::isRunning` is whether it carries more than one frame,
and `Image::frames` is every frame in playback order, already
composited.

`Image::duration` is the sum of all frame durations, zero for a still.
`Image::repetitions` is how many times the animation plays, negative for
forever — the common case for animated stickers, and what a still
reports.

`Image::frameAt` is the frame to show at an elapsed `std::chrono` time,
placed by a `Timing`: left alone, looping as the file says, a finished
finite animation holding its last frame, and a still answering its one
frame at any time; `Timing::start`, `Timing::rate` and `Timing::loop`
move, scale and override that.

`Frame` is one decoded frame: `Frame::image`, a premultiplied, immutable
`Picture`, which `toSk` in `advanced/Skia.h` hands to Skia; `Frame::time`, where it begins on the document's clock;
`Frame::duration`, how long it stays on screen, zero for a still; and
`Frame::index`.

`Metadata` is what a document says with no pixel decode:
`Metadata::width`, `Metadata::height`, `Metadata::channels`,
`Metadata::frames` (above one for an animation), `Metadata::duration`,
`Metadata::repetitions`, `Metadata::floatingPoint` for an HDR or float
source, `Metadata::format` as the format's own name, and
`Metadata::layers` and `Metadata::channelNames` for a source that
carries them.

## See also

- `core/Image.h` — the header: `Image`, `ImageOptions`
- `core/Frame.h` — `Frame`, `Timing`, `Loop`
- [Channels](Channels.md) — the format-neutral escape hatch, for a
  source whose channels matter
- [`embeddedImages`](../functions/embeddedImages.md) — recovering
  encoded images from a container with no parser here
