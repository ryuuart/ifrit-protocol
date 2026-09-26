# SigilMedia

Pictures, still and moving: what their bytes mean, both directions. An
image document decoded whole — a still, or an animation whose every
frame is composited at decode time — and a video opened as a clip whose
frames decode around a playhead read through one `frameAt()`; the
metadata either says about itself without a decode; pixels and frames
written back out as a still format or a movie; and the answers a
silhouette or a baseline needs once pixels are in hand — which pixels a
picture covers, how far every other pixel is from them, and how far two
pictures stand apart. No Qt, no windowing, no filesystem: the library
sees bytes, and a SigilIO hub is how a URI becomes them.

Namespace `sigil::media`, one flat vocabulary over two sub-libraries —
the image sub-library and the video sub-library — and the core both
stand on. One feature library per directory, linked by what a consumer
uses; every public header lives under `include/sigilmedia/<feature>/`.

- [IMAGE.md](IMAGE.md) — the image chapter: the route bytes take to an
  `Image` or to `Channels`, the encoders, and the gotchas of each format.
- [VIDEO.md](VIDEO.md) — the video chapter: the streaming decode, the
  device executor, the decode pool, and the movie encoder.

## The common cases

```cpp
#include <sigilio/advanced/Decoding.h>
#include <sigilmedia/advanced/Resource.h>
#include <sigilmedia/core/Image.h>
#include <sigilmedia/core/PixelSource.h>
#include <sigilmedia/difference/Difference.h>
#include <sigilmedia/image/Encode.h>
#include <sigilmedia/video/Encoder.h>
#include <sigilmedia/video/Video.h>
using namespace std::chrono_literals;

sigil::media::registerDecoders(hub);                        // once, where the hub is built

// OPEN — through the hub; this library gives the meaning
auto poster = hub.load<sigil::media::Image>("res://poster.png");   // still or animated, decoded once, cached
auto clip   = hub.load<sigil::media::Video>("res://title.mp4");    // opened, not decoded
auto baked  = sigil::media::Image::of(surface->makeImageSnapshot());  // a picture already rendered

// READ — a still is a one-frame document, so one call reads any of them
sigil::media::Frame frame = poster->frameAt(elapsed);       // loops as the file says
sigil::media::Frame moving = clip->frameAt(elapsed);        // a video loops forever
frame.image;                                                // what every drawer takes
poster->size();  clip->duration();  poster->isRunning();

// SHOW — every other library takes a PixelSource: a picture, an Image, a
// Video, frames another application publishes, a rendered scene
sigil::media::PixelSource shown = clip;                     // read on the caller's clock
material::Texture(poster);                                  // Material's texture over any source
compose::image(poster);                                     // Compose's leaf

// WRITE
std::vector<std::byte> png = sigil::media::encode(*baked, sigil::media::Format::Png);
sigil::media::Encoder movie({.width = 1080, .height = 1920}); // MP4, 30 frames a second
movie.append(frame);
std::vector<std::byte> mp4 = movie.finish();

// COMPARE
sigil::media::PixelDifference apart = sigil::media::difference(*render, *baseline);
apart.identical();
```

`elapsed` is any `std::chrono` duration — `380ms`, `1.2s` — and so is
every time this library answers.

## The one seam pixels cross

`sigil::media::PixelSource` is how a picture reaches another library, in
the way the web's `CanvasImageSource` is how one reaches a canvas: a
picture in hand, an `Image` or a `Video` read under a `Timing`, a picture
baked once by `sigil::media::PixelSource::produce`, and any other type
that answers `frameAt(time)` and `isRunning()` and compares by value —
SigilIO's frame subscription and SigilCompose's rendered scene are two.
`frameAt` answers the frame standing at a time on the caller's clock,
`revision` counts the frames that have stood, `size` is the frame size.
A frame that stands on a device carries no image until
`sigil::media::deviceImage` binds it, which is what the drawing libraries
call. Two sources are equal when a consumer holding one may keep what it
made of the other, so a material holding a still prunes and one holding
a clip redraws.

## When a default is wrong

Every verb above takes one options struct last; each field is one some
consumer sets.

| options | taken by | fields |
|---|---|---|
| `sigil::media::ImageOptions` | `hub.load<Image>(uri, {…})`, `media::decode<Image>` | `layer` (an EXR channel group), `width`/`height` (a vector source's raster size) |
| `sigil::media::VideoOptions` | `hub.load<Video>(uri, {…})`, `media::decode<Video>` | `playback` (the decode pool), `cachedFrames`, `hardware` |
| `sigil::media::Timing` | `Image::frameAt`, `Video::frameAt` | `start`, `rate`, `loop` (`Loop::AsEncoded`, `Forever`, `Once`) |
| `sigil::media::EncodeOptions` | `media::encode` | `quality` |
| `sigil::media::Encoder::Options` | `media::Encoder` | `width`, `height`, `format`, `framesPerSecond`, `bitRate`, `hardware` |
| `sigil::media::FieldOptions` | `coverageMask`, `distanceField` | `threshold` |
| `sigil::media::Playback::Options` | `media::Playback` | `workers` |

Bytes already in hand decode without a hub through the same decoders:
`sigil::media::decode<sigil::media::Image>(bytes, {.width = 256}, "mark.svg")`,
`decode<sigil::media::Channels>(bytes)`, `decode<sigil::media::Video>(bytes)`.
What a document says about itself is `sigil::media::Metadata`, read as
`sigil::io::probe<sigil::media::Metadata>(hub, uri)` or
`sigil::media::probe(bytes)`.

A video opened with a pool never makes the thread that draws wait:

```cpp
auto pool = std::make_shared<sigil::media::Playback>();
auto shown = hub.load<sigil::media::Video>("res://loop.mp4", {.playback = pool});
if (shown->hasFrame()) draw(shown->frameAt(elapsed));
```

## Control

| names | for | header |
|---|---|---|
| `sigil::media::DeviceFrame`, `sigil::media::deviceImage`, `sigil::media::HardwareUse` | a frame that stands on a device, and binding it for the recorder that draws it — what a leaf, a pen and a texture call | `advanced/Device.h` |
| `sigil::media::PixelSourceType`, `sigil::media::RevisedPixelSource`, `sigil::media::SizedPixelSource`, `sigil::media::TimedDocument` | writing a new pixel source | `advanced/Source.h` |
| `sigil::media::formatForPath`, `sigil::media::extensionFor` | the filename question | `advanced/Formats.h` |
| `sigil::media::registerDecoders`, `sigil::media::probeResource` | this library on a resource hub | `advanced/Resource.h` |
| `sigil::media::embeddedImages` | the last-resort signature scan | `advanced/Embedded.h` |
| `sigil::media::Video::decodeAt`, `sigil::media::Video::hardware` | the worker half of the pool; which way the device was taken | `video/Video.h` |
| the pixmap and channel-plane doors of `encode`, `difference` and `coverageMask`; `sigil::media::canEncode` | a caller that chose a depth or holds rows it did not decode | their feature headers |

The backends — Skia's codecs, the KTX reader, SVG, OpenImageIO in and
out, FFmpeg's demux, decode and mux, the VideoToolbox executor — have no
public door: each is a private `Backends.h` or `Device.h` beside its
feature.

## The headers

| target | directory | holds |
|---|---|---|
| `SigilMediaCore` | `core/` | `Frame`, `Timing`, `Loop`, `HardwarePreference`, `Image`, `ImageOptions`, `Metadata`, `Format`, `PixelSource`, the `decode<T>` door, the device frame and `deviceImage`, the filename question; Skia only |
| `SigilMediaImageDecode` | `image/decode/` | the image route and `Channels`, over the Skia codecs, KTX, and OpenImageIO and SVG where they are built in; the embedded scan |
| `SigilMediaImageEncode` | `image/encode/` | `encode`, `EncodeOptions`, `canEncode`, `encodeResource` |
| `SigilMediaVideoDecode` | `video/decode/` | `Video`, `VideoOptions`, `Playback`, over FFmpeg with the device executor beside the CPU one |
| `SigilMediaVideoEncode` | `video/encode/` | `Encoder` |
| `SigilMediaResource` | `resource/` | `registerDecoders`, `probe`, `probeResource` |
| `SigilMediaField` | `field/` | `Mask`, `DistanceField`, `FieldOptions`, `coverageMask`, `distanceField` |
| `SigilMediaDifference` | `difference/` | `PixelDifference`, `difference` |

`SigilMediaImage` and `SigilMediaVideo` are the two sub-libraries'
umbrellas and `SigilMedia` the whole library's. The encoders stand
beside the decoders rather than under them, so a consumer that only
writes pictures pulls in no codec it will not call, and a consumer that
only draws pictures somebody else decoded links the core alone.

- `core/Frame.h` — `Frame`, `Timing`, `Loop`, `HardwarePreference`
- `core/Image.h` — `Image`, `ImageOptions`, `loadOptions`
- `core/Metadata.h` — `Metadata`
- `core/Format.h` — `Format`
- `core/Decode.h` — `decode`, `ConfiguredDocument`, `DocumentOptions`
- `core/PixelSource.h` — `PixelSource`, `Produced`
- `advanced/Source.h` — `PixelSourceType`, `RevisedPixelSource`, `SizedPixelSource`, `TimedDocument`
- `advanced/Device.h` — `DeviceFrame`, `DeviceBinding`, `HardwareUse`, `deviceImage`
- `advanced/Formats.h` — `formatForPath`, `extensionFor`
- `advanced/Resource.h` — `registerDecoders`, `probe`, `probeResource`
- `advanced/Embedded.h` — `EmbeddedImage`, `EmbeddedScan`, `embeddedImages`
- `image/Decode.h` — `decodeDocument`, `probeDocument`
- `image/Channels.h` — `Channels`, `ChannelPick`
- `image/Encode.h` — `EncodeOptions`, `encode`, `canEncode`, `encodeResource`
- `video/Video.h` — `Video`, `VideoOptions`, `Playback`, `decodeDocument`, `probeDocument`, `loadOptions`
- `video/Encoder.h` — `Encoder`
- `field/DistanceField.h` — `Mask`, `DistanceField`, `FieldOptions`, `coverageMask`, `distanceField`
- `difference/Difference.h` — `PixelDifference`, `difference`

## The pixel difference

It reads the two pictures as unpremultiplied 8-bit colours, alpha
included, so a picture stored BGRA and one stored RGBA compare as the
colours they hold; two of one colour type are compared a row at a time
as stored first, so the pictures a passing test holds against its
baseline cost a memory comparison a row. The count and the widest gap
separate the two things a render can be: a rounding difference moves
many pixels by a count or two, a compositing or layout difference moves
some pixel a long way. How much of either a caller accepts is its own
judgement; this answers the facts. A picture not already on the CPU is
read back first.

## The distance field

```cpp
#include <sigilmedia/field/DistanceField.h>

// How far every pixel is from the pixels the picture covers — alpha
// GREATER than the threshold, half by default, which is the rule an
// unantialiased rasteriser uses, so the mask's edge is the drawn edge.
const sigil::media::DistanceField field = sigil::media::distanceField(*silhouette);

// "Everything within 8 px of the shape" — a DISC offset, corners
// rounded, a diagonal edge standing off by 8 and not by 8·root-two.
const bool insideTheDilation = field.at(x, y) <= 8.0f;
```

It is EXACT, not an approximation: two separable passes — a lower
parabola envelope down each row, then down each column — answer the true
squared distance for every pixel in time proportional to the raster,
where a chamfer pass would answer an integer approximation of it. It
lives here because it is a question about pixels: an analytic distance
function evaluates per pixel for the handful of shapes it is written for
and cannot answer for an arbitrary raster at all. SigilWeave reads this
one to give an exclusion's margin its meaning.

## Boundary

SigilMedia owns **meaning**: format sniffing, decode and encode backends,
probing, channel and layer semantics, colour type choice, frame
timestamps, hardware video surfaces, muxing, and the quality and depth
decisions a format offers. It owns nothing about *access* — where bytes
come from, where they go, how they are named, whether they are cached or
reloaded when they change. That is SigilIO's half, and SigilIO adds no
format knowledge in return: a hub registers nothing until
`registerDecoders` puts this library's decoders on it. SigilSkia owns
the Graphite context and recorder and the wrap that turns a plane
standing on a device into an image; nothing here names a Graphite
backend texture.

The core links `unofficial::skia::skia` publicly and nothing else. The
image decoders link OpenImageIO and Skia's SVG module privately and
optionally, each degrading to "that format fails to decode" with a
configure-time warning; the still encoders link OpenImageIO the same
way. The video features link FFmpeg privately, through one interface
target this library declares from the port's find module. No codec is
written here, with one container read: every format is somebody else's
encoder or decoder called by name — Skia's codecs and encoders,
`OIIO::ImageInput` and `OIIO::ImageOutput`, FFmpeg — except KTX, whose
uncompressed texels are plain rows behind a header that no dependency
this build installs reads without a device.

## Build and test

[docs/overview/testing.md](../../../docs/overview/testing.md) is the
contract every library here is built, tested and measured under: one
`media_test` and one `media_bench`, ctest one entry per CASE, what a
case may pin, and what a label promises. The committed fixtures are 4x4
px files under `test/assets/` — one still per format plus a three-frame
animation for each animated format — and a VP8 clip with alpha;
`test/Pixels.h` is what the image cases read a picture by. The optional
decode backends are a build-time fact, so a case that wants one compiles
whatever this build has and skips naming the backend it wanted: the
decode suites carry the `svg` and `oiio` labels, and the device suite
the `gpu` label. The chapters say what each sub-library's cases and
benches cover.
