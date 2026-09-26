# SigilMedia — video

The chapter on the video sub-library: encoded container bytes opened as a
seekable `Video` whose frames decode around a playhead and stay in a
small cache, the pool that keeps many clips fed without blocking the
thread that draws, and frames appended to an `Encoder` that finishes as
a movie's bytes. FFmpeg supplies container and codec support.
`README.md` beside it is the library's front page.

FFmpeg arrives as `sigil_media_ffmpeg`, one INTERFACE target declared
from the find module's variables and linked privately by the two
features that call it: the port publishes a module and no package, and
the release and debug archives have to be resolved as a pair, which the
pkg-config files beside them cannot do.

## Decode and compose

```cpp
#include <sigilmedia/video/Video.h>

auto clip = hub.load<sigil::media::Video>("res://motion/title.mp4");
const sigil::media::Frame frame = clip->frameAt(elapsed);
const sk_sp<SkImage> drawn = sigil::media::deviceImage(frame, canvas.recorder());
if (drawn) canvas.drawImageRect(drawn, destination, SkSamplingOptions());
```

A `Video` copies the encoded input because demuxing and later seeks
outlive the call that opened it. Opening finds the best video stream and
prepares its decoder; it does not decode the whole file. `frameAt()`
places the elapsed time on the clip's clock by `Timing` — a video loops
forever unless told `Loop::Once` — seeks when needed, decodes forward to
that time, and keeps only `VideoOptions::cachedFrames` decoded frames.
The answer is the frame covering the time, or the one before it when the
frame durations leave a gap there.

A stream whose frames carry no presentation timestamps — a bare
elementary stream, the container that would have stamped them absent —
is placed by decode order against the probed frame rate. No time inside
such a stream can be sought to, so an ask behind the playhead reads it
again from the beginning.

A device grants a hardware decompression session on the first decode,
not when the decoder opens, so `Video::hardware` answers both:
`configured` is the configuration the decoder holds and `decoding` the
surface its most recent frame arrived on. A caller that needs the fact
rather than the intent decodes one frame and then asks.

With `HardwarePreference::Preferred`, Apple builds first request a
VideoToolbox decoder. A hardware frame stays where it was decoded: its
`Frame` carries the `CVPixelBuffer` as a `DeviceFrame` and no `image`,
and `deviceImage(frame, recorder)` makes Metal views of the buffer's Y
and UV planes and hands them to SigilSkia's `wrapPlanarImage` as the two
planes of one image, described by an `SkYUVAInfo` this library builds.
Drawing that image composites video on the GPU without an RGBA upload or
CPU colour conversion; one texture cache serves every decoder on the
same Metal device, and one wrapped image per decoded frame and recorder
feeds any number of draws. With no recorder — a raster canvas, an
encode — the same call transfers the frame and converts it through the
CPU executor. Both executors read the stream's colour matrix and range —
BT.709, BT.2020 or BT.601, limited or full — off the frame, and an
untagged stream is BT.601 limited on both, so a clip composites to the
same colours whichever executor answers.

Alpha-bearing video is reported by `Metadata::hasAlpha` and produces
premultiplied raster frames. WebM VP8 and VP9 alpha use FFmpeg's libvpx
decoder because the container carries the alpha bitstream beside the
colour bitstream. Platform hardware decoders that expose only opaque YUV
surfaces are skipped for those clips; requiring hardware decode rejects
an alpha clip rather than silently dropping its alpha plane.

## The decode pool

A video opened with a `Playback` never makes the thread that draws wait:
`frameAt()` asks the pool for the frame at the time and answers the
newest frame the pool has finished. The pool is a bounded set of workers
over a multi-producer, multi-consumer queue; an ask that falls inside
the frame on show, or matches the one in flight, is coalesced away, and
a newer ask replaces queued stale work.

```cpp
auto pool = std::make_shared<sigil::media::Playback>();
auto shown = hub.load<sigil::media::Video>("res://loop.mp4",
                                           {.playback = pool, .cachedFrames = 12});
if (shown->hasFrame()) draw(shown->frameAt(elapsed));
```

`hasFrame()` lets a host keep one loading cover visible until every
source has produced a frame. `Playback::Options::workers = 0` runs no
worker: every ask decodes on the calling thread before it returns, which
is what a plate or a test wants when the answer must be the frame asked
for. Hardware frames cross the queue as retained device surfaces and are
bound only by `deviceImage()` on the thread that draws; software and
alpha frames are converted on a worker and cross as immutable raster
images. A device may cap its simultaneous decoder sessions; `Preferred`
lets the excess streams decode on workers while the render thread keeps
GPU-compositing every resulting image, and `Required` rejects a frame if
the codec opened a hardware configuration but the device later refused
to produce a device surface.

`Video::decodeAt` is the worker half: the frame covering a document time,
decoded on the calling thread with no looping and no pool. A video
opened on a pool is decoded by the pool's workers, so nothing else asks
it to decode.

## Encode and export

```cpp
#include <sigilmedia/video/Encoder.h>

sigil::media::Encoder movie({.width = 1080, .height = 1920, .framesPerSecond = 30});
for (const sigil::media::Frame& frame : frames) movie.append(frame);
const std::vector<std::byte> mp4 = movie.finish();
hub.write("res://out/story.mp4", mp4);
```

MP4 output is H.264. The encoder prefers the platform hardware encoder
and falls back to OpenH264 unless hardware is required; no other H.264
encoder is tried. Every input is resized and converted to the codec's
YUV format with the BT.709 limited-range matrix the stream is tagged
with, so a decoder reads back the colour it was given. Dimensions, frame
rate and bit rate are fixed for the encoder's lifetime; odd dimensions
are refused because interoperable 4:2:0 H.264 needs whole chroma
samples, and an encoder that refused its options converts to false with
`error()` saying why.

`finish()` flushes the delayed codec frames, writes the MP4 trailer and
hands back the container's bytes; the encoder never opens an output
path. A movie is at least one frame, so finishing with none answers
nothing, and finishing is terminal on either outcome: a second
`finish()` and every later `append()` are refused with the reason in
`error()`.

## Caching and ownership

The least-recently-used cache stores decoded frames, not rendered copies
of the whole video. Device frames retain their platform surface, raster
frames their pixel data, and a Graphite wrap retains the texture planes
until Skia releases them. Seeking flushes codec state but does not
invalidate cached frames that still cover a later ask, so repeated seek
points stay hot while they fit the configured capacity. Every answered
frame is materialized in the cache — the frame before a gap in
presentation times included — so its raster or device binding serves
the next ask at the same time. A held frame answers the gap after it
only when the frame that follows it in decode order is held too, since a
frame evicted from between the two may be the one that covers the time.
A capacity of zero is one.

`Video` and `Encoder` are not thread-safe. A player that decodes on one
thread and draws on another opens its videos on a pool, or carries
`Frame` values across its own queue; it does not ask one `Video` from
two threads.

Audio streams are detected in `Metadata::hasAudio` but are not decoded
or encoded, so timing is a video clock only. Subtitle streams and
alpha-video encoding are outside this surface.

## Tests and benches

The encode cases write a short MP4 in memory, open it through the decode
feature, and check its timing and that the colours it was given read
back through the CPU executor, beside the odd dimensions an encoder
refuses, what a finished encoder refuses and the frameless finish it
refuses. One of them re-containers that MP4 as a bare H.264 elementary
stream, a stream whose frames carry no timestamps at all, and asks it
for frames on either side of the playhead.

The decode cases cover input that is not a video (one parameterised case
over no bytes at all and bytes of something else), alpha, seeking, every
`Timing` outside the duration, the cache's capacity, a pool in its
synchronous mode (`workers = 0`) and a pool let go with asks in flight —
nothing in either waits on a clock — and what
`HardwarePreference::Required` means: device frames or no frames,
asserted on whichever arm this build takes. The `VideoDevice` suite
binds hardware frames on a Graphite Metal surface where the platform
makes VideoToolbox available; it carries the `gpu` label.

`media_bench` measures independent mixed-resolution clocks through the
device path in two arms: `BM_RenderThread` asks every stream on the
thread that presents, and `BM_WorkerPool` opens the same streams on a
pool paced at the presentation rate, timing the render thread's own
work alone. Each arm prints a `VIDEO_DEVICE` line stating how many
hardware decompression sessions the device granted and the native, ready
and fresh frame percentages beside the frame time. `--streams`,
`--surfaces`, `--rate` and `--workers` override the arms' own counts.
