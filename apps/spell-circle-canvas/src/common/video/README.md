# SigilVideo

Video meaning for Skia applications: encoded container bytes open as a
seekable, streaming `Video`; frames decode around the playhead and stay in a
small presentation cache; pixels flow the other direction through an
incremental `Encoder` and finish as container bytes. FFmpeg supplies container
and codec support. SigilVideo opens no files and resolves no URIs —
SigilIO fetches encoded bytes and stores the bytes an encoder returns;
`formatForPath` is extension-to-format meaning, the way SigilImage spells it.

Namespace `sigil::video`. The targets are deliberately separable:

| target | holds |
|---|---|
| `SigilVideoCore` | shared hardware policy and native-frame vocabulary |
| `SigilVideoDecode` | FFmpeg demux and decode, seeking, frame cache, the CPU executor, and beside it the device executor — VideoToolbox pixel buffers become Metal Y and UV textures for Graphite on Apple platforms, behind a seam private to this target |
| `SigilVideoEncode` | FFmpeg video encoding and MP4 muxing into memory |
| `SigilVideo` | umbrella over decode and encode |

FFmpeg itself arrives as `sigil_video_ffmpeg`, one INTERFACE target
declared from the find module's variables and linked privately by the
two features that call it: the port publishes a module and no package,
and the release and debug archives have to be resolved as a pair, which
the pkg-config files beside them cannot do.

## Decode and compose

```cpp
#include <sigilvideo/decode/Decode.h>

auto bytes = hub.blob("res://motion/title.mp4");
auto clip = sigil::video::decodeVideo(bytes->bytes.data(), bytes->bytes.size());

clip->draw(canvas, destination, elapsedSeconds);
```

`Video` copies the encoded input because demuxing and later seeks outlive the
call that created it. Opening finds the best video stream and prepares its
decoder; it does not decode the whole file. `frameAt()` seeks when needed,
decodes forward to the requested presentation time, and retains only
`DecodeOptions::cachedFrames` decoded frames.

A device grants a hardware decompression session on the first decode, not
when the decoder opens, so a `Video` separates the two: `hardwareConfigured()`
is the configuration it holds and `hardwareDecoding()` is the surface its
most recent frame arrived on. A caller that needs the fact rather than the
intent decodes one frame and then asks.

Alpha-bearing video is reported by `VideoProbe::hasAlpha` and produces
premultiplied `VideoFrame` images. WebM VP8 and VP9 alpha use FFmpeg's libvpx
decoder because the container carries the alpha bitstream beside the colour
bitstream. Platform hardware decoders that expose only opaque YUV surfaces are
skipped for those clips; the decoded premultiplied frame is cached and uploads
through Skia when it is drawn on a GPU canvas. Requiring hardware decode rejects
an alpha clip rather than silently dropping its alpha plane.

With `HardwarePreference::Preferred`, Apple builds first request a
VideoToolbox decoder. A hardware frame carries its `CVPixelBuffer` as a
`NativeFrame`, so a platform host may publish it through a display-overlay
path without converting it. When `frameAt()` receives the Graphite recorder
owned by the destination canvas, the device executor makes Metal views of the
pixel buffer's Y and UV planes and hands them to SigilSkia's `wrapImage` as the
two planes of one image, described by an `SkYUVAInfo` this library builds. Drawing that image composites video on the GPU without an RGBA upload
or CPU colour conversion. One texture cache serves every decoder on the same
Metal device, and one wrapped image per decoded frame can feed any number of
draws.
A raster canvas, an unsupported native pixel format, or a disabled device
policy transfers and converts through the CPU executor. Both executors read
the stream's colour matrix and range — BT.709, BT.2020 or BT.601, limited
or full — off the frame, and an untagged stream is BT.601 limited on both,
so a clip composites to the same colours whichever executor answers.

The native device passed in `DecodeOptions::metalDevice` must be the device
behind the destination recorder. Null selects the system Metal device. A host
with an explicit device should always pass it.

## Asynchronous presentation

`Playback` is the many-video presentation path. It owns a bounded decoder
worker pool over a multi-producer, multi-consumer queue, coalesces repeated
requests that still fall inside the displayed source frame or match the
in-flight request, and lets newer requests replace queued stale work. The
render thread only requests a time and reads the last complete frame:

```cpp
#include <sigilvideo/decode/Playback.h>

sigil::video::Playback playback;
auto handle = playback.add(clip);

playback.request(handle, elapsedSeconds);
auto frame = playback.frame(handle, canvas.recorder());
if (frame.image) canvas.drawImage(frame.image, 0, 0);
```

Request the initial presentation time immediately after `add()` to prefetch it
while the scene is being assembled; a clip may be added while workers are
busy, and adding a clip already registered answers its existing handle, since
one `Video` is never decoded by two workers at once. `ready(handle)` lets a
host keep one loading cover visible until every source has produced a frame.
Several draw leaves may reuse one handle when they show the same source
clock; the completed native or raster frame is then mapped once and fanned
out without another decode. `Options::workerThreads = 0` runs no worker:
`request()` decodes on the calling thread before it returns, which is what a
plate or a test wants when the answer must be readable from the next
`frame()` with nothing to wait for.

Hardware frames cross the queue as retained native surfaces and are mapped to
Graphite only by `frame()`. Software and alpha decode happens on a worker and
crosses as an immutable raster image. A device may cap its simultaneous native
decoder sessions; `Preferred` lets excess streams decode on workers while the
render thread keeps GPU-compositing every resulting image. `Required` rejects
a frame if the codec opens a hardware configuration but the device later
refuses to produce a native surface.

## Encode and export

```cpp
#include <sigilio/source/Sink.h>
#include <sigilvideo/encode/Encode.h>

auto encoder = sigil::video::Encoder::make(
    sigil::video::Format::Mp4,
    {.width = 1080, .height = 1920, .framesPerSecond = 30});

for (const SkPixmap& frame : frames)
  encoder->append(frame);

sk_sp<SkData> mp4 = encoder->finish();
sigil::io::writeBytes("story.mp4", mp4->data(), mp4->size());
```

MP4 output uses H.264. The encoder prefers the platform hardware encoder and
falls back to OpenH264 unless hardware is required; no other H.264 encoder is
tried. Every input is resized and converted to the codec's YUV format with
the BT.709 limited-range matrix the stream is tagged with, so a decoder reads
back the colour it was given. Dimensions, frame rate, and bit rate are fixed
for the encoder's lifetime; odd dimensions are rejected because interoperable
4:2:0 H.264 requires complete chroma samples.

`finish()` flushes the delayed codec frames, writes the MP4 trailer, and hands
back one `SkData`. The result can go through `Hub::write()` or any
`ByteSink`; the encoder never opens an output path.

## Caching and ownership

The least-recently-used cache stores decoded presentation frames, not rendered
copies of the whole video. Native frames retain their platform surface, raster
frames retain their pixel data, and a Graphite wrap retains the texture planes
until Skia releases them. Seeking flushes codec state but does not invalidate
cached frames that still cover a later request, so repeated seek points remain
hot while they fit the configured capacity. Every answered frame is
materialized in the cache — the frame before a gap in presentation times
included — so its raster or device wrap serves the next ask at the same
time. Cache capacity zero is normalized to one.

`Video` and `Encoder` are not thread-safe. A player that decodes on one thread
and draws on another transfers `VideoFrame` values across its own queue; it
does not call one `Video` concurrently.

## Boundary

SigilVideo owns temporal media meaning: containers, codecs, frame timestamps,
pixel formats, hardware video surfaces, and muxing. SigilSkia continues to own
the Graphite context and recorder, and the wrap that turns a plane standing on
a device into an image: nothing here names a Graphite backend texture. SigilCompose supplies a retained leaf that
samples a `Video` against its motion clock. SigilIO owns access and export.
None of those libraries re-export SigilVideo's vocabulary.

Audio streams are detected in `VideoProbe` but are not decoded or encoded.
Timing is therefore a video clock only. Subtitle streams and alpha-video
encoding are outside this surface.

## Build and test

From `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Release
cmake --build build --config Release --target video_decode_test \
  video_encode_test video_device_test
ctest --test-dir build -C Release -R '^video_' --output-on-failure
```

`video_encode_test` creates a short MP4 in memory, decodes it through
`SigilVideoDecode`, and checks its timing and that the colours it was given
read back through the CPU executor. `video_decode_test` covers input that is not a video (one parameterised
case over no bytes at all and bytes of something else), alpha, seeking,
the cache's capacity, `Playback` in its synchronous mode
(`workerThreads = 0`, so a request is decoded before it returns and
nothing here waits on a clock), and what
`HardwarePreference::Required` means — device frames or no frames,
asserted on whichever arm this build takes rather than on one platform. `video_device_test`
exercises the native device path on a Graphite Metal surface where the
platform makes VideoToolbox available; it carries the `gpu` ctest label.
`video_device_bench` measures independent mixed-resolution clocks through
the device path in two arms: `BM_RenderThread` pulls every stream with
`frameAt` on the thread that presents, and `BM_WorkerPool` drives the same
streams through `Playback` paced at the presentation rate, timing the render
thread's own work alone. Each arm prints a `VIDEO_DEVICE` line stating how
many hardware decompression sessions the device granted and the native,
ready and fresh frame percentages beside the frame time. `--streams`,
`--surfaces`, `--rate` and `--workers` override the arms' own counts; a
stream count past what the device will grant sessions for is what makes the
native percentage report where that limit lies.
