# SigilSkia

SigilSkia brings Skia's Graphite GPU backend up on a device someone else
already owns. Given a native device and command queue — Metal or Vulkan —
it builds a Graphite `Context` and `Recorder`, and given a texture — the
API's own object, or the name a `GpuDevice` gave one — it wraps that
texture as an `SkSurface`, so ordinary `SkCanvas` draw calls land
directly in the texture with no copy, or as an `SkImage`, so a draw
samples it where it stands. Beside those it reads an image out
at the width a device texture takes it. A Qt host reaches the same
bring-up through an adapter over its `QRhi`. There is no scene, product,
or drawing logic here: nothing in this library knows what is being drawn.

**The device is not here.** `GpuDevice`, its handles and its fences are
SigilCoreHardware's — `<sigilcore/hardware/GpuDevice.h>`, namespace
`sigil::core::hardware` — because a device knows nothing about what draws
on it, and a Diligent renderer adopts the same one. This library stands
Graphite on whatever device it is handed and reads it through the entry
points below.

Namespace `sigil::skia`. Headers live under `<sigilskia/<feature>/...>`;
`<sigilskia/Skia.h>` is the umbrella over the Qt-free features.

## Features

Each feature is one directory, one static library, and one header
directory, and links only what it needs.

| Feature | Target | Headers | What it holds |
|---|---|---|---|
| graphite | `SigilSkiaGraphite` | `<sigilskia/graphite/GraphiteContext.h>`, `<sigilskia/graphite/OffscreenSurface.h>`, `<sigilskia/graphite/TextureImage.h>`, `<sigilskia/graphite/Pixels.h>` | the context over a native device and queue, the surface over a texture, the image over one, and the pixel reads a device upload takes; Metal and Vulkan as parallel paths, and the entry points that read a `GpuDevice` — `GraphiteContext::create`, the `OffscreenSurface` wrap over a `TextureHandle`, the submit that signals a `FenceHandle` |
| qt | `SigilSkiaQt` | `<sigilskia/qt/QtInterop.h>` | the adapters that unwrap a `QRhi`'s native handles and forward to graphite |
| draw | `SigilSkiaDraw` | `<sigilskia/draw/Direct.h>` | the two `SkCanvas` ops Graphite leaves unimplemented, decomposed into ones every backend performs |

`SigilSkia` is the umbrella over the Qt-free features. Dependencies point
one way: Graphite stands on the hardware device, so the graphite feature
links `SigilCoreHardware` — the qt feature links graphite — and nothing
there knows Skia exists.
The draw feature is header-only over Skia and links neither. A Qt host links `SigilSkiaQt`, which carries the
umbrella with it. A consumer that owns its own Metal device (the native
macOS app, the headless gallery, the GPU tests and benchmarks) links
`SigilSkia` and never sees Qt.

## Using it

Stand the context up once, from handles you already have, and keep it
alive for as long as you draw:

```cpp
#include <sigilskia/Skia.h>

// From raw Metal handles (id<MTLDevice> / id<MTLCommandQueue> bridged to
// void*):
std::unique_ptr<sigil::skia::GraphiteContext> graphite =
    sigil::skia::GraphiteContext::createMetal(device, queue);

// …or from Vulkan handles, every one an opaque value:
sigil::core::hardware::VulkanHandles handles;
handles.instance = instance;
handles.physicalDevice = physicalDevice;
handles.device = device;
handles.queue = queue;
handles.queueFamilyIndex = family;
handles.apiVersion = VK_API_VERSION_1_1;
handles.getInstanceProcAddr = vkGetInstanceProcAddr;
graphite = sigil::skia::GraphiteContext::createVulkan(handles);

// …or from Qt, inside a QQuickRhiItem renderer:
#include <sigilskia/qt/QtInterop.h>
graphite = sigil::skia::createGraphiteContext(rhi());
```

Then, per frame, wrap the texture you want to render into, draw, and
submit:

```cpp
sigil::skia::OffscreenSurface surface(*graphite, mtlTexture, width, height);
SkCanvas *canvas = surface.canvas();
if (!canvas)
  return;                       // the wrap failed; nothing to draw into

canvas->clear(SK_ColorTRANSPARENT);
drawMyScene(*canvas);
surface.submit();
```

The Vulkan constructor takes a `VulkanImage` — the `VkImage`, its
current layout and format, and its size — and the Qt adapter is
`sigil::skia::wrapTexture(*graphite, qrhiTexture, pixelSize)`, which
returns the same `OffscreenSurface` by value. These are the wraps for a
host that holds the API's own object; a host whose textures are named by
a `GpuDevice` hands the handle over instead and names no API at all.

`OffscreenSurface` is a thin wrapper around a texture someone else owns —
construct it fresh each time rather than caching it. If you need the
underlying objects, `graphite->context()`, `graphite->recorder()` and
`surface.surface()` hand them out.

### Sampling a texture someone else painted

The other direction: `wrapImage` reads a texture as an `SkImage` a draw
samples, with nothing copied.

```cpp
#include <sigilskia/graphite/TextureImage.h>

sk_sp<SkImage> image = sigil::skia::wrapImage(
    *graphite->recorder(), mtlTexture, width, height);
if (image)
  canvas->drawImageRect(image, destination, sampling, &paint);
```

It takes the RECORDER rather than the context, because that is what a
wrap is recorded on, and a thread with a recorder of its own — a web
renderer, a decoder — hands that one over. The texels are read as the
format the texture itself declares; the alpha type and the colour space
are the two things a texture cannot say for itself.

**The image holds the texture.** A wrap retains it and releases it when
the last image naming it is gone, so an image outliving the view or the
frame that owned its texture still samples pixels rather than whatever
now holds the slot.

**Several planes are one image.** The overload taking a span of
`TexturePlane` and an `SkYUVAInfo` wraps a frame that arrived as
separate luma and chroma textures, so the shader that samples it does
the colour arithmetic and nothing is converted on the way in. That one
retains nothing: the planes live as long as the release context the
caller hands over — one buffer every plane was made from, rather than
each plane in turn — and the release runs on every path out, including a
wrap that never happened.

Only the Metal arm is built: it is what every caller of these wraps
holds. A Vulkan arm stands beside it the day something asks for one, the
same way the surface wrap has both.

### Drawing into a texture a device named

A host whose textures and fences are named by a `GpuDevice` hands the
handle over and names no API at all. The wrap, the submit and the fence
signal are the same three calls whichever backend the device is:

```cpp
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilskia/Skia.h>
using namespace sigil::core::hardware;
using sigil::skia::GraphiteContext;
using sigil::skia::OffscreenSurface;

std::unique_ptr<GraphiteContext> graphite = GraphiteContext::create(*device);

FenceHandle fence = device->createFence();
for (;;) {
  device->beginFrame();
  OffscreenSurface surface(*graphite, *device, target);
  drawMyScene(*surface.canvas());
  FenceValue done = surface.submit(*device, fence);  // signal behind the draw
  // …later: device->waitCpu(fence, done) blocks; device->waitGpu(fence, done)
  // holds later queue work instead.
}
```

`GraphiteContext::create(device)` is the one factory that reads a device
rather than raw handles: it takes whichever API the device is and calls
the matching bring-up. A stale handle wraps nothing — `canvas()` stays
null — so a texture destroyed under a host's feet stops it drawing
rather than reaching whatever now holds the slot. What a device is, how a
handle goes stale and what a fence promises are all in SigilCore's
README; what is here is only what Graphite does over one.

**A float image needs a half-float copy to be sampled.** A decoded HDR
panorama lands as 32-bit float RGBA, which keeps the range a sun needs
and is not filterable on Apple GPUs — a sampler asked to interpolate
between two F32 texels there answers nothing. `halfFloatPixels(image)`
is the copy that makes it drawable, tightly packed four halves a texel,
with `bytePixels(image)` beside it for the ordinary path and
`isFloatImage(image)` to choose between them. The conversion lives here
rather than beside the decoder because it is a property of the hardware
the pixels are going to and not of the file they came from — and here
rather than in the hardware feature because reading an `SkImage` is
Skia's business.

## The mental model

**Two paths, one shape.** Metal and Vulkan are parallel bring-up paths:
one factory and one wrapping constructor each, both Qt-free, both in the
graphite feature. The Metal translation units exist on Apple alone; the
Vulkan ones compile on every platform and are live only where the linked
Skia carries the backend (`SK_VULKAN`) — elsewhere the factory returns
null and the wrap leaves `canvas()` null. Its consumer is the renderer
that creates the Vulkan device: SigilGeometry's `device` feature adopts
what Diligent made and stands Graphite on it, and its test is where the
Vulkan arms of this one live.

**The Qt adapter serves one API per build.** `createGraphiteContext(QRhi
*)` unwraps Metal handles on Apple and Vulkan handles elsewhere, and
returns null for any other `QRhi` backend. Null is a normal, expected
outcome — it means "this build cannot serve that backend" — and a caller
handles it by drawing another way, never by assuming an API.

**A fence signalled after a submit means the drawing has landed.**
Graphite shares the device's one command queue, so a signal queued behind
`submit()` is reached only once the work has run. What a fence is, how a
handle goes stale and when a destroyed texture is actually released are
SigilCoreHardware's — its README states them.

**One recorder per thread, one context under a lock.** A Graphite
`Recorder` belongs to the thread that records on it; the `Context` may be
used from several threads but never at the same time. `recorder()` is
for the thread that made the context. Another thread — a web renderer,
a loader — takes its own from `makeRecorder()`, which carries the same
image provider and ordering preconditions, records on it alone, and
inserts what it snaps into `context()` under `lockContext()`. Once any
second thread can reach the context, the making thread's own inserts,
submits, readbacks and `checkAsyncWorkCompletion` calls hold that lock
too; `OffscreenSurface::submit()` already does. Textures cross threads
freely — a wrap on one recorder of a texture painted through another is
ordered by the one queue underneath.

**Graphite shares the host's command queue.** That is the whole reason
this library exists in the shape it does: because Graphite's submissions
and the host's own render pass go into one queue in submission order, the
host's later work observes the finished texture without any CPU
synchronization. `submit()` is therefore asynchronous, and correct only
under that sharing.

## Gotchas

These two are preconditions, not tuning knobs. Missing either produces
silent, total failure — no crash, no error, nothing rendered.

**Recordings must stay ordered.** The recorder is created with ordered
replay required, because unordered replay makes every `snap()` evict the
glyph, path and clip atlases, re-uploading every glyph once per frame.
The price is a hard rule: a `snap()` that returns null, or a snapped
`Recording` that is never inserted, skips an ID and permanently kills the
recorder. Every later `insertRecording` then fails and the process renders
nothing for the rest of its life. Never snap in order to discard, anywhere
downstream of this context. A failed insert warns once on stderr; after
that the silence is all you get.

**Every recorder must carry the caching image provider.** Graphite
performs no implicit uploads. A draw that samples a non-Graphite (raster)
`SkImage` asks the recorder's client image provider for a texture version,
and *drops the draw* when there is none.
`GraphiteContext::makeRecorderOptions()` installs a provider that promotes
on first use and caches by image ID and mipmap flag, so a raster atlas
uploads once rather than per draw; every backend factory here passes it.
Any recorder created outside this library with a bare `makeRecorder()`
will silently swallow raster-image draws.

That provider carries two details worth knowing. Its cache pins textures
until a crude full evict at 256 entries — sized for a handful of
long-lived generated atlases, so a host that churns thousands of distinct
images should revisit it. And it retries `kRGBA_F32_SkColorType` sources
as an F16 copy, because F32 textures are not filterable on Apple GPUs and
would otherwise fail promotion outright.

**Ownership of the native handles.** `createMetal` retains the device and
queue for the context's lifetime (that translation unit compiles without
ARC, so the retain is explicit); `createVulkan` retains nothing and the
handles must outlive the context. Either way the caller keeps its own
references and stays the owner, and a wrapped texture's memory is never
freed by Skia. Who frees a `GpuDevice`'s own device and queue is the
device's business, not this library's.

**Two backends, one bring-up each.** `createMetal` on Apple,
`createVulkan` on the adopted Vulkan handles; `GraphiteContext::create`
picks between them off the device it is handed. The Vulkan path is live
only where the linked Skia carries the backend, and a device with no
Vulkan runtime behind it never reaches this library at all — SigilCore's
README states what each backend a device can be actually supports.

**A null canvas means the wrap failed.** `OffscreenSurface::canvas()`
returns null in that case — check it before drawing. A wrap by handle
fails that way for a stale handle too, so a texture destroyed under a
host's feet stops it drawing rather than reaching whatever now holds
the slot.

**A wrapped Vulkan image is wrapped in the layout the device last knew.**
A device-created image starts undefined, and wrapping an undefined image
does not preserve what is in it — the first thing drawn into one must be
what fills it. Nothing hands Skia's final layout back to the device, so a
host that needs a particular layout afterwards transitions it itself.

**`SIGILSKIA_GLYPH_ATLAS_BYTES`** caps the Graphite glyph-atlas texture
budget from the environment. Unset leaves Skia's default in place; it
exists so the budget can be varied while measuring.

**A shader that will not compile is a log line, not an error.** Graphite
builds the fragment program for a draw at record time and compiles it on
the device. A program that fails there is dropped, the draw paints
nothing, and the next frame tries again — so a runtime effect that is
valid as its own SkSL program and invalid once Graphite has inlined it
into a pipeline scrolls past forever rather than failing anything.
`GraphiteContext::reportShaderErrorsTo` puts a `skgpu::ShaderErrorHandler`
in Skia's place so a caller can act on it; it is read when the context is
created, so install it first. Unset, Skia prints the generated shader and
the compiler's errors to stderr. SigilMaterial's device sweep is built on
this.

## The two draws Graphite does not implement

`graphite::Device` overrides `drawImageLattice` and `drawAtlas` with
empty bodies, so every such call on a Graphite canvas silently vanishes —
a nine-slice frame or a sheet of instance stamps simply does not appear,
with no error anywhere. `<sigilskia/draw/Direct.h>` is the way round it:
`draw::drawLattice` emits per-cell `drawImageRect`s over the alternating
fixed and stretchable bands `draw::detail::latticeEdges` computes, and
`draw::drawSpriteAtlas` emits one `drawVertices` quad list sampling the
sheet.

**They decompose on EVERY backend and never call the native op.** A
picture recorded on a raster canvas must be able to replay on a Graphite
one, and a recorded native lattice or atlas op vanishes there. The
canvas's recorder gates only TEXTURE PROMOTION — `draw::ready` uploads a
raster source through a per-owner `draw::Promoted` cache, because
Graphite performs no implicit upload for direct image use.

This feature is unconditional where the rest of the library is gated:
the ops vanish on a Graphite canvas whether or not this repository is the
one that stood that canvas up.

## Boundary

Public dependencies: Skia and, for the two features that bring Graphite
up, SigilCoreHardware — the device this stands Graphite on, which knows
nothing of Skia in return. `SigilSkiaQt` adds
`Qt6::Core` publicly and uses `Qt6::GuiPrivate` for the `QRhi` API; on
Apple, `SigilSkiaGraphite` links Foundation privately. The graphite
feature never links Qt, and nothing here links a renderer, a text engine,
or a scene.

## Building

`SigilSkiaDraw`, `SigilSkiaGraphite`, `SigilSkiaQt` and the `SigilSkia`
umbrella are always built, with `skia_draw_test` and `skia_pixels_test`
everywhere and on Apple `skia_graphite_test` (ctest) plus
`skia_graphite_bench` (Google Benchmark, through the `benches` target and
`scripts/bench_ledger.py`). `skia_pixels_test` is arithmetic over an
`SkImage` — no device, no context, no bring-up — which is why it is its
own binary and runs on every machine. `skia_graphite_test` takes the
Metal path end to end on
the system device: a context, a wrapped texture, a clear, and the pixels
read back through the queue the context shares. It then takes the same
wrap through a device — a texture the device made, the surface built
from its handle, a stale handle that wraps nothing, a fence the submit
signals, and the factory that reads a device the host adopted — on
Metal. The same wrap on a Vulkan device is `geometry_device_test`'s,
since that is where a Vulkan device is made. The half-float read is
checked in the pixel binary, since its whole reason is a sampler that
refuses F32.
Every case in it that needs a device says so — `skia_graphite_test`
carries the ctest label `gpu` and each such case skips, naming what it
wanted, rather than failing on a machine with no Metal device. A case
here asserts one thing a header promises and is named that promise as a
sentence; it pins only what editing this library could falsify — a band
split's arithmetic, a clear read back, a stale handle refused — never a
size a rasteriser chose. The benchmark weighs the handle wrap against
the native one on the same texture. The Qt adapters and the products are exercised
through `compose_gpu_test`, `scry_gpu_test` and the applications.

At run time the Metal path needs a Metal device — on macOS, everything.
The Vulkan path needs a Skia built with its Vulkan backend (the `vulkan`
feature of the `skia` port, which the manifest lists) and a host with a
Vulkan device; a Qt host selects it with `QSG_RHI_BACKEND=vulkan`.
