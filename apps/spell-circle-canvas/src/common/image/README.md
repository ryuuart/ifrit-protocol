# SigilImage

Image meaning, both directions: encoded bytes in and Skia images out, and
pixels in and encoded bytes out. Skia's own codecs decode PNG, JPEG,
WebP, GIF and AVIF, including multi-frame animation, and encode the first
three; two optional backends extend that — OpenImageIO for EXR, PSD, TIFF
and HDR with layer and channel selection on the way in, and EXR on the way
out with those channels' names kept, and Skia's SVG module for
rasterizing vector sources. For sources
carrying more than plain RGBA, the raw float channel planes are exposed
directly. No Qt, no windowing, no filesystem abstraction — the library
sees bytes.

Namespace `sigil::image`. One feature library per directory, linked by
what a consumer uses; every public header lives under
`include/sigilimage/<feature>/` and is spelled `<sigilimage/<feature>/X.h>`:

| target | headers | holds |
|--------|---------|-------|
| `SigilImageAsset`  | `asset/ImageAsset.h` | `ImageProbe`, `Frame` and `ImageAsset` — the decoded document and the Skia codec path; Skia only |
| `SigilImageDecode` | `decode/Decode.h`, `decode/ChannelData.h` | `DecodeOptions`, `decodeImage()`, `probeImage()` and `decodeChannels()` — the routing surface, carrying the optional backends — and `ChannelData`, the raw channel planes |
| `SigilImageEncode` | `encode/Encode.h` | `Format`, `EncodeOptions`, `encodeImage()` — the routing surface the other way, over a pixmap, an image or named channel planes — and `formatForPath()`/`extensionFor()` |

`SigilImage` is the umbrella target over all three. `SigilImageEncode`
stands beside `SigilImageDecode` rather than under it: a consumer that
only writes pictures links the encoder and pulls in no codec it will not
call.

## Using it

```cpp
#include <sigilimage/decode/Decode.h>

std::vector<std::byte> bytes = readWholeFile("logo.png");

// Routes across every available backend by sniffing the content.
if (auto asset = sigil::image::decodeImage(bytes.data(), bytes.size(),
                                           {}, "logo.png")) {
  const sigil::image::Frame &frame = asset->frameAt(elapsedMs);
  canvas->drawImage(frame.image, 0, 0);
}

// Vector source, rasterized at an explicit size.
auto icon = sigil::image::decodeImage(svg.data(), svg.size(),
                                      {.width = 256}, "icon.svg");

// One EXR layer, composited into an SkImage.
auto beauty = sigil::image::decodeImage(exr.data(), exr.size(),
                                        {.layer = "diffuse"}, "shot.exr");

// Or every channel the source carries, as named float planes.
if (auto ch = sigil::image::decodeChannels(exr.data(), exr.size(),
                                           "shot.exr")) {
  const int z = ch->index("depth.Z");
  const float nearest = ch->at(0, 0, z);
  sk_sp<SkImage> glow = ch->makeImage("glow");
}

// …and back out again, with the names kept: one EXR carrying layers.
sk_sp<SkData> written = sigil::image::encodeImage(*ch, sigil::image::Format::Exr);

// Metadata without a pixel decode.
auto info = sigil::image::probeImage(bytes.data(), bytes.size(), "logo.png");
```

```cpp
#include <sigilimage/encode/Encode.h>

// Pixels the caller already holds, encoded exactly as they are.
sk_sp<SkData> png =
    sigil::image::encodeImage(bitmap.pixmap(), sigil::image::Format::Png);

// An SkImage, read back to the CPU on the way in.
sk_sp<SkData> jpeg = sigil::image::encodeImage(
    *image, sigil::image::Format::Jpeg, {.quality = 90});

// The one place a filename is allowed to pick a format.
if (auto format = sigil::image::formatForPath(out))
  bytes = sigil::image::encodeImage(*image, *format);
```

Where those bytes then go is SigilIO's half:
`io::writeBytes(path, bytes->data(), bytes->size())` for a plain path,
`hub.write(uri, …)` for one behind a mount.

## Mental model

There are two layers, and they are not interchangeable.

`ImageAsset` is a decoded document — frames, per-frame durations, total
duration, repetition count — built by the **Skia codec path only**. It is
also what the routing layer returns, and `ImageAsset::wrap` turns an
already-rendered `SkImage` into a one-frame asset.

`encode/Encode.h` is the mirror of the routing surface, and the asymmetry
between its two doors is the point. The `SkPixmap` overload encodes the
pixels exactly as given — the colour type is the caller's decision, so F16
pixels reach the PNG encoder as sixteen bits per channel and float pixels
reach EXR as float. The `SkImage` overload reads the image back to the CPU
first, at the depth the format can hold: premultiplied N32 for the LDR
formats, RGBA float for EXR. A caller who wants a depth the format allows
but the readback would not choose reads back itself and uses the pixmap
door.

A third door takes `ChannelData` rather than pixels and writes every
channel under the name it carries, so a group like
`diffuse.R`/`diffuse.G`/`diffuse.B` comes back through
`DecodeOptions::layer = "diffuse"`. It is the only way to write a layer —
the pixmap doors carry four channels called R, G, B and A and nothing
else — and only EXR holds it: every other format this library writes is
three or four channels with fixed meanings, so it declines rather than
dropping the names. A caller with a layer and a PNG to put it in
composites the group with `ChannelData::makeImage` first.

`decode/Decode.h` is the routing surface. `decodeImage()` sniffs the bytes and
tries the Skia codecs first (skipped when a layer is named, since layers
are an OpenImageIO concept), then SVG, then OpenImageIO; `probeImage()`
follows the same order. The `pathHint` argument only sharpens format
detection — nothing dispatches on file extension.

Animated frames are fully composited at decode time. The source format's
disposal and blend semantics are already applied, so drawing frame N never
depends on frame N-1.

Colour types follow the source: float sources land as
`kRGBA_F32_SkColorType` so HDR range survives, LDR sources as premultiplied
N32. `ChannelData` is the format-neutral escape hatch — named interleaved
float planes plus `makeImage()` helpers for pouring a channel group back
into Skia.

## Gotchas

**Encoding names its format; decoding sniffs for it.** `decodeImage()`
reads the bytes to find out what they are, and `pathHint` only sharpens
that. `encodeImage()` is told the format outright and never looks at a
name — `formatForPath()` is the separate, explicit step that turns a
filename into one, so a caller who wants a name to decide says so.

**`ImageAsset::decode` and `::probe` are Skia-codec only.** They do not
route through SVG or OpenImageIO. Handing an EXR to `ImageAsset::decode`
returns `nullopt` no matter which backends are built in. Only the free
functions in `decode/Decode.h` route.

Nothing here opens a file. `ImageAsset` takes `SkData`, `decode/Decode.h` takes
a byte range; a caller with a path reads it (SigilIO's `Hub::image`
is the usual way, `SkData::MakeFromFileName` the bare one) and hands the
bytes in.

Decoding is eager and CPU-side. Every frame of an animation is decoded up
front and stays resident for the asset's lifetime. That fits decode-once,
draw-many workloads; it is not a streaming path for video-sized content.

Frame durations at or below 10 ms are normalized to 100 ms. Legacy
encoders wrote 0 or 10 expecting the player to substitute a sane tick, and
browsers do exactly this — matching them makes such GIFs animate instead
of blurring past.

SVG sizing rules live in `DecodeOptions::width`/`height`. Zero on one axis
derives it from the other by aspect; both zero rasterizes at the intrinsic
size, falling back to 512 for percent-sized documents that have none; the
result clamps to `[1, 8192]` on each axis.

The `SIGILIMAGE_HAS_OIIO`, `SIGILIMAGE_HAS_SVG` and
`SIGILIMAGE_HAS_OIIO_ENCODE` defines are `PRIVATE` to their libraries.
Consumers cannot test for backend availability at compile time — an
unsupported format simply fails to decode, and fails to encode the same
way, answering null rather than throwing or writing a broken file.

**WebP at quality 100 is a different codec from WebP at 99.** The format
holds a lossy and a lossless encoder in one container, and the quality
number means visual fidelity to the first and compression effort to the
second. `EncodeOptions::quality` of 100 selects lossless, because a caller
asking for everything wants the pixels back unchanged; anything below it
is lossy at that quality.

**EXR is written as half float.** That is the format's native storage and
it halves the file for the range a rendered panorama actually carries.
Full float per channel is a different request than "write me an EXR", and
the named-channel door writes half for the same reason. Channels called
`A` and `Z` are declared to the file as the alpha and the depth, because
those are the names EXR reads that way; a set naming neither declares
none rather than the first.

F32 images are not filterable on Apple GPUs, so a float source decoded
here is not automatically drawable on such a device. The fallback that
makes it drawable — an F16 copy — belongs to the GPU plumbing in
`src/common/skia` (SigilSkia's `halfFloatPixels`), not to this library.

AVIF needs both halves: `skia[avif]` alone installs libavif with no AV1
codec, which parses AVIF containers and then silently decodes no frames.
`vcpkg.json` therefore requests `libavif[dav1d]` explicitly.

## Boundary

Dependencies: `SigilImageAsset` links `unofficial::skia::skia` publicly
and nothing else. `SigilImageDecode` links `SigilImageAsset` publicly and
OpenImageIO and Skia's SVG module privately and optionally, each behind a
`find_package` or target check that degrades to "that format fails to
decode" with a configure-time warning. `SigilImageEncode` links
`unofficial::skia::skia` publicly and OpenImageIO privately and
optionally, degrading the same way to "that format fails to encode". Its
named-channel door names `ChannelData` and links nothing of the decode
feature: the value is a plain aggregate, and a consumer that writes one
includes that header and links that feature itself. A
consumer that only draws decoded images links `SigilImageAsset` and never
sees a backend.

**No codec is written here.** Every format is somebody else's encoder or
decoder called by name — `SkPngEncoder`, `SkJpegEncoder` and
`SkWebpEncoder` from Skia's `include/encode/`, `OIIO::ImageOutput` and
`OIIO::ImageInput` from OpenImageIO, Skia's own codecs and SVG module.
What this library adds is the routing, the readback, and the decisions a
format offers that a caller should not have to re-make.

SigilImage owns **meaning**: format sniffing, decode and encode backends,
probing, channel and layer semantics, colour type choice, and the quality
and depth decisions a format offers. It owns nothing about *access* —
where bytes come from, where they go, how they are named, whether they are
cached or reloaded when they change. That is SigilIO's half, and
SigilIO adds no format knowledge in return. Nothing here opens a file
in either direction: `encodeImage()` hands bytes back the way
`decodeImage()` takes them in.

## Build and test

From `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Release
cmake --build build --config Release --target image_asset_test \
  image_decode_test image_encode_test
ctest --test-dir build -C Release -R image_ --output-on-failure
```

Targets: `SigilImageAsset`, `SigilImageDecode` and `SigilImageEncode`
(static libraries, one per feature directory — `asset/`, `decode/` and
`encode/` — each holding its sources, its `test/` and its `bench/`; the
decode and encode backends are one translation unit each behind the
private `Backends.h` beside them), `SigilImage` (the umbrella), one test
per library — `image_asset_test`, `image_decode_test` and
`image_encode_test`, the last also linking `SigilImageDecode` because the
claim a round trip makes is that what came back out is what went in — and
two benchmarks (Google Benchmark, built by the `benches` target and run
from a Release build through `scripts/bench_ledger.py`).

`test/Pixels.h` beside the fixtures is what all three read a picture by:
where a committed file stands, one pixel out of a decoded frame
unpremultiplied, and a per-channel comparison a lossy format can pass. A
test target adds `test/` to its include path and spells `"Pixels.h"`.
`image_encode_test` asks the round trip as one parameterised case over
`{format, quality, lossless}`, because what separates PNG from WebP at 80
is those three values and not the shape of the question: a lossless
subject is compared for equality on all four quadrants of the fixture, a
lossy one at quadrant centres, away from the edge its chroma subsampling
smears.

The benchmarks:
`image_decode_bench` times `decodeImage` per megapixel over PNG and JPEG
fixtures encoded in memory at several sizes, the committed 4x4 stills for
the per-call floor, and `probeImage`; `image_encode_bench` times each
format per megapixel over a generated gradient, and the SkImage door
against the pixmap one so the readback's share is visible. The fixtures are
committed 4x4 px files under `test/assets/` at the library root — one
still per format plus a three-frame animation for each animated format —
located through the `SIGIL_TEST_ASSET_DIR` compile definition, so
the test and the benchmark that measures against them both run from any
working directory.
