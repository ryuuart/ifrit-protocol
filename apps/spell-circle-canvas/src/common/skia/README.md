# SigilSkia

SigilSkia brings Skia's Graphite GPU backend up on a device someone else
already owns. Given a native device and command queue — Metal or Vulkan —
it builds a Graphite `Context` and `Recorder`, and given a native texture
it wraps that texture as an `SkSurface`, so ordinary `SkCanvas` draw calls
land directly in the texture with no copy. It also offers the device
itself as an object: created or adopted, with textures and fences named
by handles that go stale rather than dangle, and destruction that waits
out the frames still in flight. A Qt host reaches the same bring-up
through an adapter over its `QRhi`. There is no scene, product, or
drawing logic here: nothing in this library knows what is being drawn.

Namespace `sigil::skia`. Headers live under `<sigilskia/<feature>/...>`;
`<sigilskia/Skia.h>` is the umbrella over the Qt-free features.

## Features

Each feature is one directory, one static library, and one header
directory, and links only what it needs.

| Feature | Target | Headers | What it holds |
|---|---|---|---|
| graphite | `SigilSkiaGraphite` | `<sigilskia/graphite/GraphiteContext.h>`, `<sigilskia/graphite/OffscreenSurface.h>` | the context over a native device and queue, the surface over a native texture; Metal and Vulkan as parallel paths |
| device | `SigilSkiaDevice` | `<sigilskia/device/GpuDevice.h>`, `<sigilskia/device/Handle.h>`, `<sigilskia/device/Fence.h>` | the device and its queue, owned or adopted; textures and fences by generation-checked handle; deferred destruction; `GraphiteContext::create(GpuDevice&)` |
| qt | `SigilSkiaQt` | `<sigilskia/qt/QtInterop.h>` | the adapters that unwrap a `QRhi`'s native handles and forward to graphite |

`SigilSkia` is the umbrella over the Qt-free features — graphite and
device. Dependencies point one way: device links graphite, never the
reverse, which is why `GraphiteContext::create(GpuDevice&)` is declared
in the graphite header but defined by the device feature. A Qt host links `SigilSkiaQt`, which carries the umbrella with it. A
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
returns the same `OffscreenSurface` by value.

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
NativeTexture native = device->exportNative(target);   // for the wrap, or the host

FenceHandle fence = device->createFence();
for (;;) {
  device->beginFrame();                     // retires destroys three frames old
  OffscreenSurface surface(*graphite, native.mtlTexture, native.width, native.height);
  drawMyScene(*surface.canvas());
  surface.submit();
  FenceValue done = device->signal(fence);  // after everything queued so far
  // …later: device->waitCpu(fence, done) blocks; device->waitGpu(fence, done)
  // holds later queue work instead.
}
device->destroy(target);                    // stale at once, released at frame + 3
```

A texture the host made enters the same table through
`importNative(nativeTexture)` — borrowed, so destroy only forgets it —
or `importNative(nativeTexture, /*takeOwnership=*/true)`, after which the
device releases it like one of its own.

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

**The Vulkan device is not driven yet.** `GpuDevice::createOwned(Vulkan)`
and `adopt` of a Vulkan `NativeDevice` return null with a diagnostic on
stderr; the graphite feature's Vulkan factory is reachable directly
through `GraphiteContext::createVulkan` for a host that has its own
handles.

**A null canvas means the wrap failed.** `OffscreenSurface::canvas()`
returns null in that case — check it before drawing.

**`SIGILSKIA_GLYPH_ATLAS_BYTES`** caps the Graphite glyph-atlas texture
budget from the environment. Unset leaves Skia's default in place; it
exists so the budget can be varied while measuring.

## Boundary

Public dependency: Skia. `SigilSkiaQt` adds `Qt6::Core` publicly and uses
`Qt6::GuiPrivate` for the `QRhi` API; on Apple, `SigilSkiaGraphite` links
Foundation and `SigilSkiaDevice` links Metal, both privately. The graphite
and device features never link Qt, and nothing here links a renderer, a
text engine, or a scene.

The two headers at the include root, `SkiaGraphiteContext.h` and
`SkiaOffscreenSurface.h`, are the global-namespace spellings of the same
two classes for a consumer that has not yet moved to `<sigilskia/...>`.
Nothing new includes them.

## Building

Added only when `SPELLCIRCLE_ENABLE_SKIA_CANVAS` is on. Targets:
`SigilSkiaGraphite`, `SigilSkiaDevice`, `SigilSkiaQt`, the `SigilSkia`
umbrella, and on Apple `sigilskia_graphite_test` and
`sigilskia_device_test` (ctest) plus `sigilskia_device_bench` (Google
Benchmark, through the `benches` target and `scripts/bench_ledger.py`).
The graphite test takes the Metal path end to end on the system device: a
context, a wrapped texture, a clear, and the pixels read back through the
queue the context shares. The device test proves the handles go stale,
destruction retires at frame + 3, a texture round-trips through import and
export, a fence signals and holds, and Graphite stands up on an adopted
device. The Qt adapters and the products are exercised through
`compose_gpu_test`, `scry_gpu_test` and the applications.

At run time the Metal path needs a Metal device — on macOS, everything.
The Vulkan path needs a Skia built with its Vulkan backend (the `vulkan`
feature of the `skia` port, which the manifest lists) and a host with a
Vulkan device; a Qt host selects it with `QSG_RHI_BACKEND=vulkan`.
