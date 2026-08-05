# SigilImage

Image decoding: encoded bytes in, Skia images out. Skia's own codecs
cover PNG, JPEG, WebP, GIF and AVIF, including multi-frame animation; two
optional backends extend that — OpenImageIO for EXR, PSD, TIFF and HDR
with layer and channel selection, and Skia's SVG module for rasterizing
vector sources. For sources carrying more than plain RGBA, the raw float
channel planes are exposed directly. No Qt, no windowing, no filesystem
abstraction — the library sees bytes.

Namespace `sigil::image`. Headers `ImageAsset.h`, `Decode.h`.

## Using it

```cpp
#include <sigilimage/Decode.h>

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

// Metadata without a pixel decode.
auto info = sigil::image::probeImage(bytes.data(), bytes.size(), "logo.png");
```

## Mental model

There are two layers, and they are not interchangeable.

`ImageAsset` is a decoded document — frames, per-frame durations, total
duration, repetition count — built by the **Skia codec path only**. It is
also what the routing layer returns, and `ImageAsset::wrap` turns an
already-rendered `SkImage` into a one-frame asset.

`Decode.h` is the routing surface. `decodeImage()` sniffs the bytes and
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

**`ImageAsset::decode`, `::load` and `::probe` are Skia-codec only.** They
do not route through SVG or OpenImageIO. Handing an EXR to
`ImageAsset::load` returns `nullopt` no matter which backends are built
in. Only the free functions in `Decode.h` route.

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

The `SIGILIMAGE_HAS_OIIO` and `SIGILIMAGE_HAS_SVG` defines are `PRIVATE`
to the library. Consumers cannot test for backend availability at compile
time — an unsupported format simply fails to decode.

F32 images are not filterable on Apple GPUs, so a float source decoded
here is not automatically drawable on such a device. The fallback that
makes it drawable — an F16 copy — belongs to the GPU plumbing in
`common/skia`, not to this library.

AVIF needs both halves: `skia[avif]` alone installs libavif with no AV1
codec, which parses AVIF containers and then silently decodes no frames.
`vcpkg.json` therefore requests `libavif[dav1d]` explicitly.

## Boundary

Dependencies: `unofficial::skia::skia` publicly; OpenImageIO and Skia's
SVG module privately and optionally, each behind a `find_package` or
target check that degrades to "that format fails to decode" with a
configure-time warning.

SigilImage owns **meaning**: format sniffing, decode backends, probing,
channel and layer semantics, colour type choice. It owns nothing about
*access* — where bytes come from, how they are named, whether they are
cached or reloaded when they change. That is SigilLoader's half, and
SigilLoader adds no format knowledge in return.

## Build and test

From `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Debug
cmake --build build --config Debug --target image_test
ctest --test-dir build -C Debug -R image_test --output-on-failure
```

Targets: `SigilImage` (static library) and `image_test`. The fixtures are
committed 4x4 px files — one still per format plus a three-frame animation
for each animated format — located through the `IFRIT_IMAGE_TEST_ASSET_DIR`
compile definition, so the test runs from any working directory.
