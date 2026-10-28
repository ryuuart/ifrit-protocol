# SigilGeometry — the device

The chapter on the one GPU device both APIs stand on: bringing it up
and what is public of it, the rule the shared queue is submitted under,
what a mesh and a map become on it, the device executors that stand
beside their CPU ones, and the Vulkan loader. `README.md` beside the
library is the front page.

`device::Device::create(config)` brings the one GPU device up, or answers
the `core::Error` that says why it could not:
Diligent creates a Vulkan device and its immediate context, and
SigilGpu's device adopts the Vulkan device, queue and loader
entry points it made. A texture named on `gpu()` is then an image both
APIs reach — 2D drawing through `graphite()` lands in it and a 3D pass
samples it, with no copy in either direction and one handle table naming
both. Where `graphite()` stands depends on the driver: elsewhere it
records onto that same Vulkan queue; on Apple, where the driver runs
Vulkan over Metal, it is Graphite's Metal backend on a queue of its own
on the Metal device the driver runs over, painting the Metal texture the
driver keeps behind each image, and the device makes no Vulkan Graphite
at all.

```cpp
#include <sigilgeometry/device/Device.h>

using namespace sigil;

geometry::device::DeviceConfig config;
auto created = geometry::device::Device::create(config);
if (!created) return;  // no Vulkan runtime, for instance; `created.error()` says why
std::unique_ptr<geometry::device::Device> device = std::move(*created);

gpu::GpuDevice& gpu = *device->gpu();
gpu::TextureDescription desc;
desc.width = desc.height = 512;
const gpu::TextureHandle texture = gpu.createTexture(desc);

// Paint 2D into a texture a 3D pass will sample, and have the device's
// queue wait on the GPU for the painting before the pass is submitted.
{
  skia::OffscreenSurface surface(*device->graphite(), gpu, texture);
  surface.canvas()->clear(SK_ColorBLUE);
  surface.submit();
}
device->waitForPainting();
```

**`device/Device.h` is the whole of what this feature says in public,
and it spells no engine header.** The two Diligent interfaces it hands
out are forward-declared, so a consumer that only wants a device — a
host bringing one up, a sketch reaching for one — compiles against no
engine at all. The headers that DO name the engine's buffers, samplers,
textures, pipelines and bindings sit beside this feature's sources
instead of under `include/`, and a consumer that genuinely holds one of
those objects reaches them by putting that directory on its own PRIVATE
include path — `SIGIL_GEOMETRY_DEVICE_PRIVATE_DIR` is the one door, and
the world's frame executor is what walks through it. A header that names
an engine interface is not a library's public word.

`renderDevice()` and `context()` are the Diligent side and are never
null on a device that was created. `gpu()` and `graphite()` are the
adopted side and are null together when the adoption failed — a driver
without timeline semaphores, for instance, since that is what a hardware
fence is. A failed adoption costs the shared 2D path and nothing else.

**The queue is shared, and sharing has a rule.** Diligent's passes, and
where 2D shares the queue Graphite's submissions, go into one queue in
submission order, which is what lets a submit stay asynchronous and
still be correct — but only while the two streams never interleave.
Every Graphite submit on that queue, and every fence signal or wait on
`gpu()`, is made under a `QueueLock`. Diligent takes the same lock from
inside its own submissions, which is why the lock does not nest: no
Diligent call may be made while one is held.

**Two queues keep two orders, on the GPU.** Where 2D paints on Metal,
`waitForPainting()` has the Vulkan queue wait for everything
`graphite()` submitted before it — one call covers every texture painted
so far, so a frame makes it once, before a pass samples one — and
`holdPaintingUntilSampled()` submits what the device has recorded and
has the next submission `graphite()`'s own recorder makes wait for all
of it, which a frame makes after its last pass that wore a painted
texture, since the next painting repaints part of that texture in place.
Both take the queue lock themselves, and both do nothing where 2D shares
the queue. The texture residency below makes both calls, so a renderer
wearing pages through it never does.

**A host may hand its own Metal queue over.** `paintWith(painter,
graphite)` makes a host's Metal device and Graphite context — a window's,
adopted from the queue its frames are drawn on — what `graphite()`
answers, so the window and the pages it shows stand on one context and
each draws the other's images as they stand. Both orders cover the
host's queue from then on, and the device's own as well, for a texture
adopted before the host came. A host on another Metal device, or half a
pair, is refused with nothing changed; `paintWith(nullptr, nullptr)`
gives the device's own back, and a host does that before its device or
context goes.

**A queue made on Metal itself reaches the device through its Metal
objects.** Where the driver runs Vulkan over Metal, every texture and
fence named on `gpu()` is also the Metal object the driver keeps behind
it: `gpu::GpuDevice::exportNative` answers the id<MTLTexture>
behind a texture and the id<MTLSharedEvent> behind a fence, so a queue on
Metal paints into a texture this device made, or samples one drawn here,
and waits for the drawing on the GPU. `metalEvent()` is that event for a
fence on `gpu()`, and `metalTexture()` the same texture for one the
engine made rather than `gpu()`. The other way round,
`gpu::GpuDevice::importNative` names a texture Metal made as a
Vulkan image over the same memory — the handle owns the image and the
texture stays its maker's — and brings a Metal event over as a fence the
queue waits on with `waitGpu`, under a `QueueLock` like every wait on
`gpu()`. On any other driver none of them answers a Metal object, and a
caller reads the picture back.

**Pipelines are kept between runs.** Every pipeline is built through the
driver's own cache, `pipelineStateCache()`, which `Device::create` seeds
from `DeviceConfig::pipelineStore` and which is written back there when
the device goes; the driver discards what another device or driver
wrote.

**Residency is the device's too.** A mesh is host memory and a map is an
image; a draw needs buffers and a texture. `device::MeshResidency`
(`device/residency/Meshes.h`) makes that crossing and remembers it: `upload()`
holds a mesh under the number the caller gave the artefact it came from
and the primitive colour lane it is packed for, so two frames looking at
the same triangles cross once — and a mesh drawn tinted and untinted in
one frame is two packings, each held — and `stream()`
writes a mesh nobody can name into one pair of buffers grown to fit and
overwritten by the next draw. `device::MeshVertex` is the one vertex
layout every pipeline over those buffers declares — filled in on upload
for a mesh that carries no normals, uvs or tint — and `meshLayout()` is
that layout as a pipeline states it, one form for each choice of
instance elements and tangent, every form over the same stride. `device::TextureResidency` (`device/residency/Textures.h`)
is the same story for maps: pixels that already stand on this very
device are wrapped where they are, in the format they were painted in and
with the levels they were made with, and nothing crosses through host
memory — a page made with a chain is worn through that chain, its levels
below zero filled in place from level zero once for each painting of
it a frame wears (once a frame where its painter does not count its
paintings, as the device frame's `revision` says), so a body
wearing a page at a distance does not alias across it, and a renderer
that has the residency make its pages ready before a pass opens has them
filled there, so no draw breaks its pass to do it — everything else is brought over once and held
under the image it came from, and the
prefiltered panorama and its cosine convolution are uploaded once per
sky as half floats, because a sky holds values above one. `endFrame()`
on either lets go of what no draw has named lately, so a window sliding
along a curve does not hold what it cooked for the life of the scene.

Two executors sharing a device share both, exactly as they share
`device::Resources`. A renderer above says WHAT it wants drawn; putting
it on the device is not a thing each renderer answers for itself.
`mapMipLevels()` is the one arithmetic behind the chain an uploaded map
carries. All three — the two residencies and `device::PipelineCache` —
are `SigilGeometryDeviceResidency`, the sibling target beside the device,
so a consumer that only wants a device links no mesh currency and no
material. Their headers are that feature's own for the same reason the
device's engine-facing ones are — each names the engine's buffer,
texture, pipeline or binding interfaces — so they stand beside its
sources and a consumer holding one of those objects puts
`SIGIL_GEOMETRY_DEVICE_RESIDENCY_PRIVATE_DIR` on its own PRIVATE include
path.

**The device executors of this library's own seams stand beside their CPU
ones**, in their own target: `SigilGeometryMeshPopDevice` in
`mesh/pop/device/`. `pop::deviceRuntime(device)`
(`mesh/pop/device/Cook.h`) cooks a chain by dispatching the kernel this
build compiled, `pop::sweepDeviceRuntime(device)`
(`mesh/pop/device/Sweep.h`) forms a sweep's rings by dispatching theirs,
and `points::deviceRuntime(device)` (`mesh/pop/device/Stamp.h`) forms a
stamping's vertices by dispatching the third. None of the three computes an arithmetic
of its own — the kernel is one Slang source compiled twice, to the C++
the host executor calls and to the SPIR-V dispatched here — which is
what lets the two tiers be held to bit identity rather than to a
tolerance. The target is separate so that the feature it stands beside
stays free of a device, and
`mesh/pop/device/test/DeviceCookTest.cpp`, `DeviceStampTest.cpp` and
`DeviceSweepTest.cpp` are the conformance: every chain, every stamping
and every sweep the device runtimes say they can do, done both ways and
compared bit for bit — and, beside that, that the backend says nothing
while they do, because a wrong barrier is reported and then the right
picture is drawn anyway, so comparing answers cannot see one.

`device::Resources` is what every executor on that device stands on,
made once and shared: the buffer a draw's uniforms go into, the samplers
a map is read through (linear and nearest, clamped and tiled, plus the
one a panorama needs — periodic in azimuth, clamped at the poles and
linear ACROSS the prefiltered levels), the one white texel an unfilled
sampled slot reads, and the staging copy `read()` brings a texture's
pixels home through. None of it is a frame's; a frame's targets, meshes
and pipelines belong to whatever draws frames.

Four of those five samplers ARE the engine's own named states, and the
uniform buffer is made by its own one-call constructor, so nothing here
respells what the engine already spells; the readback's row copy is its
stride-aware subresource copy, which is what the two differing strides
need. The panorama's sampler is the one that stays written out, because
it is the one no named state covers: two different wraps on its two axes,
and a level range a roughness reads across.

**The Vulkan loader is opened once**, by the volk shim in
`device/VolkShim.c`, and the `vkGetInstanceProcAddr` it resolves is
handed to the hardware device — which refuses an adoption without one,
because dispatching through a second copy of the same library is what
makes two APIs stop being one device. `SIGILGEOMETRY_VULKAN_LIBRARY` names a
Vulkan library to open ahead of the built-in candidates. There is no
Metal path, because Diligent has no Metal backend: `create` fails on a
machine with no Vulkan runtime and says so, and on macOS the runtime is
`brew install molten-vk vulkan-loader`.

The `Device` suite is therefore where the hardware device's Vulkan
backend is exercised at all — the formats it maps, the import and export
round trip, the timeline fence, and Graphite over a texture the device
named — because a Vulkan device exists here and nowhere below.
