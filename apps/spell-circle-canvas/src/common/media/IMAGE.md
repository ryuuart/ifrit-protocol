# SigilMedia — images

The chapter on the image sub-library: stills and animations decoded
whole into an `Image`, their float planes into `Channels`, and pixels
encoded back out as a still format. `README.md` beside it is the
library's front page.

Skia's own codecs decode PNG, JPEG, WebP, GIF and AVIF, including
multi-frame animation, and encode the first three; a KTX 1 or 2 with
uncompressed texels is read here from its header, a cube map's six faces
as one 1:6 column; two optional backends extend that — OpenImageIO for
EXR, PSD, TIFF, HDR and DDS (a cube map as the same column) with layer
and channel selection on the way in, and EXR on the way out with those
channels' names kept, and Skia's SVG module for rasterizing vector
sources.

## Decoding

```cpp
#include <sigilmedia/image/Decode.h>

std::vector<std::byte> bytes = readWholeFile("logo.png");

// Routes across every available backend by sniffing the content.
if (auto logo = sigil::media::decode<sigil::media::Image>(bytes, {}, "logo.png")) {
  const sigil::media::Frame frame = logo->frameAt(elapsed);
  canvas->drawImage(frame.image, 0, 0);
}

// A vector source, rasterized at an explicit size.
auto icon = sigil::media::decode<sigil::media::Image>(svg, {.width = 256}, "icon.svg");

// One EXR layer, composited into an image.
auto beauty = sigil::media::decode<sigil::media::Image>(exr, {.layer = "diffuse"}, "shot.exr");

// Or every channel the source carries, as named float planes.
if (auto planes = sigil::media::decode<sigil::media::Channels>(exr, "shot.exr")) {
  const int depth = planes->index("depth.Z");
  const float nearest = planes->at(0, 0, depth);
  auto glow = planes->image("glow");
  auto roughness = planes->image(sigil::media::ChannelPick{.red = depth});
}
```

Through a hub the same decoders answer a URI, once
`sigil::media::registerDecoders(hub)` has put them on it:
`hub.load<sigil::media::Image>("res://ui/mark.svg", {.width = 256})`,
`hub.load<sigil::media::Channels>("res://shot.exr")`. The options are
this library's `ImageOptions` because `core/Image.h` declares
`loadOptions(std::type_identity<Image>)` answering them, found by
argument-dependent lookup the way the probe is: a hub takes the options
as this library spells them without knowing an image format. Beside it
`meaningName(std::type_identity<Image>)` answers "media.Image" — and
"media.Channels" and "media.Video" for the other two — the name a hub
registers and finds the decoder under, so a sketch compiled and loaded
while its host runs, holding `Image` under another C++ identity, reaches
the decoder the host registered.

`image/Decode.h` is the route. Decoding an `Image` sniffs the bytes and
tries the Skia codecs first (skipped when a layer is named, since layers
are an OpenImageIO concept), then KTX, then SVG, then OpenImageIO; the
probe follows the same order. The name only sharpens format detection —
nothing dispatches on a file extension.

Animated frames are fully composited at decode time. The source format's
disposal and blend semantics are already applied, so drawing frame N
never depends on frame N-1. Colour types follow the source: float
sources land as `kRGBA_F32_SkColorType` so HDR range survives, LDR
sources as premultiplied N32. `Channels` is the format-neutral escape
hatch — named interleaved float planes plus `Channels::image` for
pouring a channel group back into a picture.

## Encoding

```cpp
#include <sigilmedia/image/Encode.h>

// A document's first frame, or a picture in hand, read back at the depth
// the format holds.
std::vector<std::byte> png = sigil::media::encode(*poster, sigil::media::Format::Png);
std::vector<std::byte> jpeg =
    sigil::media::encode(*picture, sigil::media::Format::Jpeg, {.quality = 90});

// Pixels the caller already holds, encoded exactly as they are.
std::vector<std::byte> deep = sigil::media::encode(bitmap.pixmap(), sigil::media::Format::Png);

// …and named planes back out with the names kept: one EXR carrying layers.
std::vector<std::byte> layered = sigil::media::encode(*planes, sigil::media::Format::Exr);
```

The asymmetry between the doors is the point. The pixmap door encodes
the pixels exactly as given — the colour type is the caller's decision,
so F16 pixels reach the PNG encoder as sixteen bits per channel and
float pixels reach EXR as float. The picture and document doors read
back to the CPU first, at the depth the format can hold: premultiplied
N32 for the LDR formats, RGBA float for EXR. A caller who wants a depth
the format allows but the readback would not choose reads back itself
and uses the pixmap door.

The channel-plane door writes every channel under the name it carries,
so a group like `diffuse.R`/`diffuse.G`/`diffuse.B` comes back through
`ImageOptions::layer = "diffuse"`. It is the only way to write a layer,
and only EXR holds it: every other still format is three or four
channels with fixed meanings, so it declines rather than dropping the
names.

Where the bytes then go is a hub's: `hub.save(uri, image)` encodes and
writes in one call, the name's extension choosing the format through
`sigil::media::encodeResource`; `hub.write(uri, bytes)` stores bytes
already encoded.

## Gotchas

**Encoding names its format; decoding sniffs for it.** Decoding reads
the bytes to find out what they are, and the name only sharpens that.
`encode` is told the format outright and never looks at a name —
`sigil::media::formatForPath` is the separate, explicit step that turns
a filename into one, and `sigil::media::encodeResource` is that step and
the encode together, which is what a hub's save asks.

Decoding is eager and CPU-side. Every frame of an animation is decoded
up front and stays resident for the document's lifetime. That fits
decode-once, draw-many; it is not a streaming path for video-sized
content, which is `sigil::media::Video`.

Frame durations at or below 10 ms are normalized to 100 ms. Legacy
encoders wrote 0 or 10 expecting the player to substitute a sane tick,
and browsers do exactly this — matching them makes such GIFs animate
instead of blurring past.

SVG sizing rules live in `ImageOptions::width`/`height`. Zero on one axis
derives it from the other by aspect; both zero rasterizes at the
intrinsic size, falling back to 512 for percent-sized documents that
have none; the result clamps to `[1, 8192]` on each axis.

The `SIGILMEDIA_HAS_OIIO`, `SIGILMEDIA_HAS_SVG` and
`SIGILMEDIA_HAS_OIIO_ENCODE` defines are `PRIVATE` to their libraries.
Consumers cannot test for backend availability at compile time — an
unsupported format simply fails to decode, and fails to encode the same
way, answering nothing rather than throwing or writing a broken file.
On the way out `sigil::media::canEncode` answers at run time instead,
per `Format`: Skia's three are always there, a movie never is, and EXR
is true only where the OpenImageIO backend is compiled in AND its roster
carries an EXR writer that writes to memory — two independent absences,
the second of which no define can see. There is no matching question on
the way in: decoding sniffs rather than being told a format, so a caller
asking whether this build reads a format hands the bytes over and reads
the answer.

**WebP at quality 100 is a different codec from WebP at 99.** The format
holds a lossy and a lossless encoder in one container, and the quality
number means visual fidelity to the first and compression effort to the
second. `EncodeOptions::quality` of 100 selects lossless, because a
caller asking for everything wants the pixels back unchanged.

**EXR is written as half float.** That is the format's native storage
and it halves the file for the range a rendered panorama carries.
Channels called `A` and `Z` are declared to the file as the alpha and
the depth, because those are the names EXR reads that way.

F32 images are not filterable on Apple GPUs, so a float source decoded
here is not automatically drawable on such a device. The fallback that
makes it drawable — an F16 copy — belongs to SigilSkia's
`halfFloatPixels`, not to this library.

AVIF needs both halves: `skia[avif]` alone installs libavif with no AV1
codec, which parses AVIF containers and then silently decodes no frames.
`vcpkg.json` therefore requests `libavif[dav1d]` explicitly.

## Tests and benches

The image cases read the committed fixtures through `test/Pixels.h`:
where a file stands, one pixel out of a decoded frame unpremultiplied,
and a per-channel comparison a lossy format can pass. The encode cases
ask the round trip as one parameterised case over `{format, quality,
lossless}`: a lossless subject is compared for equality on all four
quadrants of the fixture, a lossy one at quadrant centres, away from the
edge its chroma subsampling smears. The EXR cases ask `canEncode()` and
assert the round trip where there is a writer and the empty answer where
there is none. The document cases in `core/test/` need no fixture: a
document built from generated frames pins placement, looping and every
`Timing`.

The field suite needs no fixture either, because a distance has a closed
form: it pins which alpha counts as ink at each end of the threshold,
and asserts the transform against distances worked out by hand — three
across and four down reading 5 and not 7, a 45-degree edge dilating by a
margin of perpendicular standoff rather than by that margin times root
two, and a mask covering nothing answering `kOutside` everywhere. The
difference suite counts two hand-set pixels and locates the wider gap,
reads the same colour stored in two channel orders as identical, and
reads only the extent both pictures cover.

`media_bench`'s decode arms time the image route per megapixel over PNG
and JPEG fixtures encoded in memory at several sizes, the committed
stills for the per-call floor, and the probe; its encode arms time each
format per megapixel and the picture door against the pixmap one, so the
readback's share is visible; its difference arms time identical
pictures against pictures that differ in every row.
