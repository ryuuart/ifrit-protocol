# SigilGeometry — the device

The chapter on the one GPU device both APIs stand on: bringing it up
and what is public of it, the rule the shared queue is submitted under,
what a mesh and a map become on it, the device executors that stand
beside their CPU ones, and the Vulkan loader. `README.md` beside the
library is the front page.

`device::Device::create(config, &error)` brings the one GPU device up:
Diligent creates a Vulkan device and its immediate context, and
SigilCore's hardware device adopts the Vulkan device, queue and loader
entry points it made, with Graphite recording onto that same queue. A
texture named on `gpu()` is then an image both APIs reach — 2D drawing
through `graphite()` lands in it and a 3D pass samples it, with no copy
in either direction and one handle table naming both.

```cpp
#include <sigilgeometry/device/Device.h>

using namespace sigil;

geometry::device::DeviceConfig config;
std::string error;
std::unique_ptr<geometry::device::Device> device =
    geometry::device::Device::create(config, &error);
if (!device) return;  // no Vulkan runtime, for instance; `error` says why

core::hardware::GpuDevice& gpu = *device->gpu();
core::hardware::TextureDescription desc;
desc.width = desc.height = 512;
const core::hardware::TextureHandle texture = gpu.createTexture(desc);
const core::hardware::FenceHandle fence = gpu.createFence();

// Paint 2D into a texture a 3D pass will sample. Everything that submits
// on the shared queue happens under the lock.
geometry::device::Device::QueueLock lock(*device);
skia::OffscreenSurface surface(*device->graphite(), gpu, texture);
surface.canvas()->clear(SK_ColorBLUE);
surface.submit(gpu, fence);
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

**The queue is shared, and sharing has a rule.** Graphite's submissions
and Diligent's passes go into one queue in submission order, which is
what lets a submit stay asynchronous and still be correct — but only
while the two streams never interleave. Every Graphite submit, and every
fence signal or wait on `gpu()`, is made under a `QueueLock`. Diligent
takes the same lock from inside its own submissions, which is why the
lock does not nest: no Diligent call may be made while one is held.

**Residency is the device's too.** A mesh is host memory and a map is an
image; a draw needs buffers and a texture. `device::MeshResidency`
(`device/residency/Meshes.h`) makes that crossing and remembers it: `upload()`
holds a mesh under the number the caller gave the artefact it came from,
so two frames looking at the same triangles cross once, and `stream()`
writes a mesh nobody can name into one pair of buffers grown to fit and
overwritten by the next draw. `device::MeshVertex` is the one vertex
layout every pipeline over those buffers declares — filled in on upload
for a mesh that carries no normals, uvs or tint — and `meshLayout()` is
that layout as a pipeline states it, with the primitive lane declared or
not over the same stride. `device::TextureResidency` (`device/residency/Textures.h`)
is the same story for maps: pixels that already stand on this very
device are wrapped where they are and nothing is copied, everything else
is brought over once and held under the image it came from, and the
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
ones**, each its own target: `SigilGeometryMeshPopDevice` in
`mesh/pop/device/` and `SigilGeometryMeshRenderDevice` in
`mesh/render/device/`. `pop::deviceRuntime(device)`
(`mesh/pop/device/Cook.h`) cooks a chain by dispatching the kernel this
build compiled, `pop::sweepDeviceRuntime(device)`
(`mesh/pop/device/Sweep.h`) forms a sweep's rings by dispatching theirs,
`points::deviceRuntime(device)` (`mesh/pop/device/Stamp.h`) forms a
stamping's vertices by dispatching the third, and
`render::deviceRuntime(device)` (`mesh/render/device/Painter.h`)
rasterises a mesh draw. None of the first three computes an arithmetic
of its own — the kernel is one Slang source compiled twice, to the C++
the host executor calls and to the SPIR-V dispatched here — which is
what lets the two tiers be held to bit identity rather than to a
tolerance. Each target is separate so that the feature it stands beside
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
