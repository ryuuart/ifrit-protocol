# SigilCore — the device

The chapter on the GPU device itself: one device and its one command queue,
created here or adopted from a host that owns them, with the textures and
fences living on it named by handles that go stale rather than dangle, and
destruction that waits out the frames still in flight. `README.md` beside
this file is the library.

It knows nothing about what draws — no Skia, no Diligent, no Qt — which is
exactly why it sits here: a 2D backend and a 3D engine can stand on one
device, name the same texture and read the same fence, and neither has to
link the other. A host that holds one never spells a graphics API.

| header | holds |
|--------|-------|
| `hardware/GpuDevice.h` | `GpuDevice`, `Backend`, `NativeDevice`, `VulkanHandles`, `NativeTexture`, `TextureDescription`, `TextureFormat`, `TextureUsage`, `mipLevelsFor` — the device, what it is made of, and what a texture on it is |
| `hardware/Handle.h` | `Handle`, `TypedHandle<Tag>`, `TextureHandle`, `BufferHandle`, `FenceHandle`, `HandleTable<T, H>` — a name that goes stale, and the slot store behind it |
| `hardware/Fence.h` | `FenceValue`, `FenceWait`, `kFenceInitialValue`, `kFenceDefaultTimeout` — a timeline and what waiting on one answers |

This feature has no umbrella header; a consumer includes the one it needs.

## Using it

A consumer that wants a GPU device — one it owns, or one an engine already
created — takes `GpuDevice`, and names textures and fences by handle rather
than holding the API's objects:

```cpp
#include <sigilcore/hardware/GpuDevice.h>
using namespace sigil::core::hardware;

// The platform's own device and a fresh queue — or adopt a host's:
std::unique_ptr<GpuDevice> device = GpuDevice::createOwned();
// NativeDevice native{Backend::Metal, mtlDevice, mtlCommandQueue};
// device = GpuDevice::adopt(native);      // never frees them

TextureDescription desc;
desc.width = 1920;
desc.height = 1080;
desc.format = TextureFormat::BGRA8Unorm;
TextureHandle target = device->createTexture(desc);

FenceHandle fence = device->createFence();
for (;;) {
  device->beginFrame();          // retires destroys three frames old
  drawSomehow(*device, target);  // whatever backend stands on this device
  FenceValue done = device->signal(fence);  // behind everything submitted
  // …later: device->waitCpu(fence, done) blocks; device->waitGpu(fence, done)
  // holds later queue work instead.
}
device->destroy(target);         // stale at once, released at frame + 3
```

`device->exportNative(target)` hands the API's own object out, for a host
that draws with the API directly or publishes the texture onwards. A
texture the host made enters the same table through
`importNative(nativeTexture)` — borrowed, so destroy only forgets it — or
`importNative(nativeTexture, /*takeOwnership=*/true)`, after which the
device releases it like one of its own.

**A texture may carry a chain.** `TextureDescription::mipLevels` asks for one,
level 0 at the description's size and each level after it half the last;
`mipLevelsFor(width, height)` is how deep the size allows, and a count past
it is clamped to it. A chain is not only a filtering aid here: a
PREFILTERED ENVIRONMENT is a different image on every level, and the level
a shader reads is the one its roughness picked, so the count has to be part
of the description rather than something generated afterward from level 0.
`exportNative` reports what the texture actually got.

## Adopting a device an engine created

**A VULKAN DEVICE IS ONLY EVER ADOPTED.** 3D engines create the Vulkan
device themselves and cannot attach to one that already exists, so whoever
owns the API in a process makes it and everything else joins it, rather
than the other way round — and a second instance here would mean two
loaders, two queues and a copy between them. `createOwned` therefore makes
the platform's own device (Metal on Apple) and nothing else.

What `adopt` wants is exactly what such an engine exposes: instance,
physical device, device, queue and queue family, the API version, and the
`vkGetInstanceProcAddr` the loader already in the process hands out. Three
conditions come with it:

- **Timeline semaphores must be enabled on that device.** A fence here is
  one, and a device created without them cannot make one. Ask the engine
  for the feature before it creates the device — Diligent spells it
  `NativeFence` — because it cannot be turned on afterwards.
- **The loader must be the engine's.** An adoption with no
  `getInstanceProcAddr` is refused: opening the same library a second time
  would work, but then the two APIs dispatch through separately opened
  copies of it and "one device" stops meaning anything.
- **The queue is now shared, and sharing has a rule.** Every submission
  goes into the one queue in submission order — that is what lets a submit
  be asynchronous and still correct — but only while the streams never
  interleave. Hold whatever lock the engine guards its queue with around
  every foreign submit and around every `signal`, `waitGpu` and `waitCpu`
  on this device.

Nothing is freed by an adopted device: the instance, device and queue stay
the engine's, and keeping them alive for as long as the `GpuDevice` lives
is the caller's business.

## How the device behaves

**Handles are names, not pointers.** A `TextureHandle` or `FenceHandle` is
a slot index plus the generation the slot had when the name was issued.
Destroying a resource frees its slot and bumps the generation, so a handle
kept past the destroy compares unequal to whatever later lives in that
slot: `isValid` says no, `exportNative` returns empty, `signal` returns the
initial value, and nothing reaches the wrong resource. The typed handles do
not convert into each other. `HandleTable` is the store behind them and can
name anything a host wants named the same way.

**Destruction waits out the frames in flight.** `destroy(texture)` makes
the handle stale at once but releases the native texture only when
`beginFrame()` has advanced `kFramesInFlight` (three) frames past the one
it was destroyed in — a frame that was recording when the destroy came in
may still reference it on the GPU. A host that never calls `beginFrame()`
never releases anything until the device is torn down, which releases
everything.

**A fence is a timeline.** Its value only ever grows; `signal` queues a
raise to the next value behind everything submitted so far and returns that
value, `waitGpu` holds every later submission until the value is reached,
and `waitCpu` blocks for it with a timeout. On Metal a fence is an
`MTLSharedEvent` and every wait and signal is a command buffer on the
device's queue; a queue executes in order, so `waitGpu` is for a value that
is already signalled or will be signalled from *another* queue —
`exportNative(fence)` hands the event to one — and a signal queued on the
same queue behind the wait can never run.

**Every call is safe from any thread except `beginFrame()`**, which belongs
to the one thread that counts frames. A device from `createOwned` releases
its device and queue when it dies; one from `adopt` never does.

**Two backends, one contract.** What each supports:

| | Metal | Vulkan |
|---|---|---|
| `createOwned` | the system default device and a fresh queue | — a Vulkan device is only ever adopted |
| `adopt` | `mtlDevice` + `mtlCommandQueue` | instance, physical device, device, queue and family index, and the host's own `getInstanceProcAddr`; the device must have timeline semaphores enabled |
| texture | `id<MTLTexture>`; `cpuAccessible` is shared storage | `VkImage` + `VkDeviceMemory`, optimal tiling, sampled, colour-attachment, input-attachment and transfer usage (+ storage for `ShaderWrite`) — input attachment because a 2D backend reads a render target back through one; `cpuAccessible` prefers host-visible coherent memory and falls back to device-local; formats map to `R8G8B8A8_UNORM`, `B8G8R8A8_UNORM`, `R16G16B16A16_SFLOAT` |
| import with ownership | retains the texture | destroys the `VkImage` and frees `vkMemory` when given |
| fence | `MTLSharedEvent` | timeline `VkSemaphore`; `waitCpu` is `vkWaitSemaphores`, queue signal and wait are empty submissions |
| loading | the framework | every entry point resolved from the host's own `vkGetInstanceProcAddr` at run time; nothing links Vulkan |

The Vulkan arms of this feature are therefore exercised where a Vulkan
device is made: SigilGeometry's `Device` suite, beside the feature that
creates one. They skip, naming why, on a machine with no Vulkan runtime (on
macOS: `brew install molten-vk vulkan-loader`).
