# SigilSkia

SigilSkia brings Skia's Graphite GPU backend up on a device someone else
already owns. Given a native device and command queue — Metal or Vulkan —
it builds a Graphite `Context` and `Recorder`, and given a texture — the
API's own object, or the name a device gave one — it wraps that texture
as an `SkSurface`, so ordinary `SkCanvas` draw calls land directly in the
texture with no copy. It also offers the device itself as an object:
created or adopted, with textures and fences named by handles that go
stale rather than dangle, and destruction that waits out the frames still
in flight. A host that holds one of those devices never spells a graphics
API: it hands over handles and reads back fences. A Qt host reaches the same bring-up
through an adapter over its `QRhi`. There is no scene, product, or
drawing logic here: nothing in this library knows what is being drawn.

Namespace `sigil::skia`. Headers live under `<sigilskia/<feature>/...>`;
`<sigilskia/Skia.h>` is the umbrella over the Qt-free features.

## Features

Each feature is one directory, one static library, and one header
directory, and links only what it needs.

| Feature | Target | Headers | What it holds |
|---|---|---|---|
| graphite | `SigilSkiaGraphite` | `<sigilskia/graphite/GraphiteContext.h>`, `<sigilskia/graphite/OffscreenSurface.h>` | the context over a native device and queue, the surface over a texture; Metal and Vulkan as parallel paths |
| device | `SigilSkiaDevice` | `<sigilskia/device/GpuDevice.h>`, `<sigilskia/device/Handle.h>`, `<sigilskia/device/Fence.h>` | the device and its queue, owned or adopted; textures and fences by generation-checked handle; deferred destruction; and the entry points the graphite headers declare over a device — `GraphiteContext::create(GpuDevice&)`, the `OffscreenSurface` wrap over a `TextureHandle`, the submit that signals a `FenceHandle` |
| qt | `SigilSkiaQt` | `<sigilskia/qt/QtInterop.h>` | the adapters that unwrap a `QRhi`'s native handles and forward to graphite |

`SigilSkia` is the umbrella over the Qt-free features — graphite and
device. Dependencies point one way: device links graphite, never the
reverse, which is why every entry point that reads a `GpuDevice` is
declared in a graphite header and defined by the device feature. The
graphite headers do include `<sigilskia/device/Handle.h>` and
`<sigilskia/device/Fence.h>` to spell those declarations, but those two
headers are names and values with no device code behind them; a caller of
the entry points links `SigilSkiaDevice`. A Qt host links `SigilSkiaQt`, which carries the umbrella with it. A
consumer that owns its own Metal device (the native macOS app, the
headless gallery, the GPU tests and benchmarks) links `SigilSkia` and
never sees Qt.

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
sigil::skia::VulkanHandles handles;
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

### Owning the device

A host that has no device of its own, or wants its textures and fences
named rather than held as raw API objects, goes through `GpuDevice`:

```cpp
#include <sigilskia/Skia.h>
using namespace sigil::skia;

// The system default device and a fresh queue — or adopt the host's:
std::unique_ptr<GpuDevice> device = GpuDevice::createOwned(Backend::Metal);
// NativeDevice native{Backend::Metal, mtlDevice, mtlCommandQueue};
// device = GpuDevice::adopt(native);      // never frees them

std::unique_ptr<GraphiteContext> graphite = GraphiteContext::create(*device);

TextureDesc desc;
desc.width = 1920;
desc.height = 1080;
desc.format = TextureFormat::BGRA8Unorm;
TextureHandle target = device->createTexture(desc);

FenceHandle fence = device->createFence();
for (;;) {
  device->beginFrame();                     // retires destroys three frames old
  OffscreenSurface surface(*graphite, *device, target);
  drawMyScene(*surface.canvas());
  FenceValue done = surface.submit(*device, fence);   // signal behind the draw
  // …later: device->waitCpu(fence, done) blocks; device->waitGpu(fence, done)
  // holds later queue work instead.
}
device->destroy(target);                    // stale at once, released at frame + 3
```

`device->exportNative(target)` still hands the API's own object out, for
a host that draws with the API directly or publishes the texture
onwards.

A texture the host made enters the same table through
`importNative(nativeTexture)` — borrowed, so destroy only forgets it —
or `importNative(nativeTexture, /*takeOwnership=*/true)`, after which the
device releases it like one of its own.

**A texture may carry a chain.** `TextureDesc::mipLevels` asks for one,
level 0 at the description's size and each level after it half the last;
`mipLevelsFor(width, height)` is how deep the size allows, and a count
past it is clamped to it. A chain is not only a filtering aid here: a
PREFILTERED ENVIRONMENT is a different image on every level, and the
level a shader reads is the one its roughness picked, so the count has to
be part of the description rather than something generated afterward from
level 0. `exportNative` reports what the texture actually got.

**A float image needs a half-float copy to be sampled.** A decoded HDR
panorama lands as 32-bit float RGBA, which keeps the range a sun needs
and is not filterable on Apple GPUs — a sampler asked to interpolate
between two F32 texels there answers nothing. `halfFloatPixels(image)`
is the copy that makes it drawable, tightly packed four halves a texel,
with `bytePixels(image)` beside it for the ordinary path and
`isFloatImage(image)` to choose between them. The conversion lives here
rather than beside the decoder because it is a property of the hardware
the pixels are going to and not of the file they came from.

### Adopting a device an engine created

Some 3D engines create the Vulkan device themselves and cannot attach to
one that already exists — Diligent Engine is one of them. There the
device is theirs and Graphite joins it, rather than the other way round.
What `adopt` wants is exactly what such an engine exposes:

```cpp
NativeDevice native;
native.backend = Backend::Vulkan;
// Off Diligent's IRenderDeviceVk:
native.vulkan.instance       = deviceVk->GetVkInstance();
native.vulkan.physicalDevice = deviceVk->GetVkPhysicalDevice();
native.vulkan.device         = deviceVk->GetVkDevice();
native.vulkan.apiVersion     = deviceVk->GetVkVersion();
// The queue sits behind the engine's own lock: take the handle, drop the
// lock. (Diligent: context->LockCommandQueue(), narrowed to
// ICommandQueueVk, then UnlockCommandQueue().)
native.vulkan.queue            = queueVk->GetVkQueue();
native.vulkan.queueFamilyIndex = queueVk->GetQueueFamilyIndex();
// The engine already opened a loader; hand over its vkGetInstanceProcAddr
// rather than letting this library open a second one.
native.vulkan.getInstanceProcAddr = engineGetInstanceProcAddr;

std::unique_ptr<GpuDevice> device = GpuDevice::adopt(native, &error);
std::unique_ptr<GraphiteContext> graphite = GraphiteContext::create(*device);
```

Three conditions come with it:

- **Timeline semaphores must be enabled on that device.** A fence here is
  one, and a device created without them cannot make one. Ask the engine
  for the feature before it creates the device — Diligent spells it
  `NativeFence` — because it cannot be turned on afterwards.
- **The loader should be the engine's.** Leaving `getInstanceProcAddr`
  null is legal and this library finds a loader itself, but then the two
  APIs dispatch through separately opened copies of it.
- **The queue is now shared, and sharing has a rule.** Graphite's
  submissions and the engine's passes go into the one queue in submission
  order — that is what makes `submit()` asynchronous and still correct —
  but only while the two streams never interleave. Hold whatever lock the
  engine guards its queue with around every Graphite submit and around
  every `signal`, `waitGpu` and `waitCpu` on this device. Diligent's is
  `LockCommandQueue()` / `UnlockCommandQueue()` on the immediate context,
  and it does not nest, so no engine call may be made while it is held.

Nothing here is freed by the device: an adopted instance, device and
queue stay the engine's, and keeping them alive for as long as the
`GpuDevice` and its Graphite context live is the caller's business.

## The mental model

**Two paths, one shape.** Metal and Vulkan are parallel bring-up paths:
one factory and one wrapping constructor each, both Qt-free, both in the
graphite feature. The Metal translation units exist on Apple alone; the
Vulkan ones compile on every platform and are live only where the linked
Skia carries the backend (`SK_VULKAN`) — elsewhere the factory returns
null and the wrap leaves `canvas()` null. The Vulkan path has no host
that exercises it yet: it builds, and the first consumer with a Vulkan
device is its test.

**The Qt adapter serves one API per build.** `createGraphiteContext(QRhi
*)` unwraps Metal handles on Apple and Vulkan handles elsewhere, and
returns null for any other `QRhi` backend. Null is a normal, expected
outcome — it means "this build cannot serve that backend" — and a caller
handles it by drawing another way, never by assuming an API.

**Handles are names, not pointers.** A `TextureHandle` or `FenceHandle`
is a slot index plus the generation the slot had when the name was
issued. Destroying a resource frees its slot and bumps the generation,
so a handle kept past the destroy compares unequal to whatever later
lives in that slot: `isValid` says no, `exportNative` returns empty,
`signal` returns the initial value, and nothing reaches the wrong
resource. The typed handles (`TextureHandle`, `BufferHandle`,
`FenceHandle`) do not convert into each other. `HandleTable` in
`<sigilskia/device/Handle.h>` is the store behind them and can name
anything a host wants named the same way.

**Destruction waits out the frames in flight.** `destroy(texture)` makes
the handle stale at once but releases the native texture only when
`beginFrame()` has advanced `kFramesInFlight` (three) frames past the one
it was destroyed in — a frame that was recording when the destroy came in
may still reference it on the GPU. A host that never calls `beginFrame()`
never releases anything until the device is torn down, which releases
everything.

**A fence is a timeline.** Its value only ever grows; `signal` queues a
raise to the next value behind everything submitted so far and returns
that value, `waitGpu` holds every later submission until the value is
reached, and `waitCpu` blocks for it with a timeout. On Metal a fence is
an `MTLSharedEvent` and every wait and signal is a command buffer on the
device's queue. Because Graphite shares that queue, a fence signalled
after `submit()` is reached only once the drawing has landed. A queue
executes in order, so `waitGpu` is for a value that is already signalled
or will be signalled from *another* queue — `exportNative(fence)` hands
the event to one — and a signal queued on the same queue behind the wait
can never run.

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
freed by Skia. A `GpuDevice` from `createOwned` releases its device and
queue when it dies; one from `adopt` never does. Every device call is
safe from any thread except `beginFrame()`, which belongs to the one
thread that counts frames.

**Two device backends, one contract.** What each supports:

| | Metal | Vulkan |
|---|---|---|
| `createOwned` | the system default device and a fresh queue | the loader (platform search, or `SIGILSKIA_VULKAN_LIBRARY`), an instance with portability enumeration where offered, the first physical device with a graphics queue, a device with timeline semaphores enabled and the portability subset where required, that queue |
| `adopt` | `mtlDevice` + `mtlCommandQueue` | instance, physical device, device, queue and family index; `getInstanceProcAddr` optional (the loader is found otherwise); the device must have timeline semaphores enabled |
| texture | `id<MTLTexture>`; `cpuAccessible` is shared storage | `VkImage` + `VkDeviceMemory`, optimal tiling, sampled, colour-attachment, input-attachment and transfer usage (+ storage for `ShaderWrite`) — input attachment because Graphite reads a render target back through one; `cpuAccessible` prefers host-visible coherent memory and falls back to device-local; formats map to `R8G8B8A8_UNORM`, `B8G8R8A8_UNORM`, `R16G16B16A16_SFLOAT` |
| import with ownership | retains the texture | destroys the `VkImage` and frees `vkMemory` when given |
| fence | `MTLSharedEvent` | timeline `VkSemaphore`; `waitCpu` is `vkWaitSemaphores`, queue signal and wait are empty submissions |
| loading | the framework | every entry point resolved from `vkGetInstanceProcAddr` at run time; nothing links Vulkan, and a machine without a loader or driver gets the reason from `createOwned` |
| the Skia path | `createMetal` | `createVulkan` on the adopted handles |

Without a Vulkan runtime, `createOwned(Backend::Vulkan)` returns null and
names the reason; on macOS the runtime is `brew install molten-vk
vulkan-loader`, and the tests and benchmarks on that backend skip with
the same message.

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

## Boundary

Public dependency: Skia. `SigilSkiaQt` adds `Qt6::Core` publicly and uses
`Qt6::GuiPrivate` for the `QRhi` API; on Apple, `SigilSkiaGraphite` links
Foundation and `SigilSkiaDevice` links Metal, both privately. The graphite
and device features never link Qt, and nothing here links a renderer, a
text engine, or a scene.

## Building

Added only when `SPELLCIRCLE_ENABLE_SKIA_CANVAS` is on. Targets:
`SigilSkiaGraphite`, `SigilSkiaDevice`, `SigilSkiaQt`, the `SigilSkia`
umbrella, and on Apple `sigilskia_graphite_test` and
`sigilskia_device_test` (ctest) plus `sigilskia_graphite_bench` and
`sigilskia_device_bench` (Google Benchmark, through the `benches` target
and `scripts/bench_ledger.py`). The graphite test takes the Metal path
end to end on the system device: a context, a wrapped texture, a clear,
and the pixels read back through the queue the context shares. It then
takes the same wrap through a device — a texture the device made, the
surface built from its handle, a stale handle that wraps nothing, and a
fence the submit signals — on Metal, and again on Vulkan where a runtime
is installed; both it and the benchmark link the device feature, since
that is where those entry points are defined. The benchmark weighs the
handle wrap against the native one on the same texture. The device test
proves the handles go stale,
destruction retires at frame + 3, a texture round-trips through import and
export, a fence signals and holds, and Graphite stands up on an adopted
device. The Qt adapters and the products are exercised through
`compose_gpu_test`, `scry_gpu_test` and the applications.

At run time the Metal path needs a Metal device — on macOS, everything.
The Vulkan path needs a Skia built with its Vulkan backend (the `vulkan`
feature of the `skia` port, which the manifest lists) and a host with a
Vulkan device; a Qt host selects it with `QSG_RHI_BACKEND=vulkan`.
