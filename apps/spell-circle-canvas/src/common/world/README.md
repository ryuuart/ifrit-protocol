# SigilWorld

SigilWorld describes a 3D scene as comparable values, turns those values
into a frame — a scene, an ordered list of passes and the readbacks the
caller asked for — and executes that frame on a GPU through
[Diligent Engine](https://github.com/DiligentGraphics/DiligentEngine)
over a device SigilSkia shares. It owns three things and nothing else:
the 3D scene description, the frame graph that orders passes from their
declared inputs and outputs, and the execution of that graph. It holds no
window, no swapchain, no clock, and no second copy of anything a library
beneath it already defines — meshes, point operators, cameras and the CPU
executor are SigilGeometry's; materials, recipes, programs and texture
sets are SigilMaterial's; the reconciler and its phases are SigilCore's;
the device, its handles and Graphite are SigilSkia's; animation is
SigilMotion's; counters and timers are SigilMeasure's.

Namespace `sigil::world`, headers under `include/sigilworld/`. Each
feature is its own static archive with its own tests and benchmark, and
links only the features beneath it. Two of them are built; the rest are
listed under **What is coming**, and this page says which is which rather
than describing a library that is not here.

## What is here

| directory | target | namespace | holds |
|---|---|---|---|
| `light/` | `SigilWorldLight` | `sigil::world::light` | emitters as plain comparable values over glm: a sun, a point light, a spot, their falloffs and the per-frame budget. No device, no registry, so a consumer that only needs to say where the lights are links this alone. |
| `diligent/` | `SigilWorldDiligent` | `sigil::world::diligent` | the one GPU device 2D and 3D share. |

### The one device

Diligent creates the Vulkan device and SigilSkia adopts it. That
direction is forced: this build of Diligent cannot attach to a device
that already exists and has no Metal backend, so standing a second device
up beside it would mean two queues, two handle tables and a CPU round
trip between 2D and 3D.

```cpp
#include <sigilworld/diligent/Device.h>

using namespace sigil;

world::diligent::DeviceConfig config;
std::string error;
std::unique_ptr<world::diligent::Device> device =
    world::diligent::Device::create(config, &error);
if (!device) return;  // no Vulkan runtime, for instance; `error` says why

skia::GpuDevice& gpu = *device->gpu();
skia::TextureDesc desc;
desc.width = desc.height = 512;
desc.format = skia::TextureFormat::RGBA8Unorm;
const skia::TextureHandle texture = gpu.createTexture(desc);
const skia::FenceHandle fence = gpu.createFence();

// Paint 2D into a texture a 3D pass will sample. Everything that submits
// on the shared queue happens under the lock.
world::diligent::Device::QueueLock lock(*device);
skia::OffscreenSurface surface(*device->graphite(), gpu, texture);
surface.canvas()->clear(SK_ColorBLUE);
surface.submit(gpu, fence);
```

`renderDevice()` and `context()` are the Diligent side, and are never
null on a device that was created. `gpu()` and `graphite()` are the
adopted side and are null together when the adoption failed — a driver
without timeline semaphores, for instance, since that is what a SigilSkia
fence is. A failed adoption costs the shared 2D path and nothing else.

The Vulkan loader is opened once, by the volk shim vendored under
`diligent/thirdparty/volk`, and the `vkGetInstanceProcAddr` it resolves
is handed to SigilSkia, so both APIs dispatch through the same entry
points. `SIGILWORLD_VULKAN_LIBRARY` names a Vulkan library to open ahead
of the built-in candidates.

There is no Metal path here, because Diligent has no Metal backend:
`create` fails on a machine with no Vulkan runtime, and the answer for
such a machine is the CPU executor, not a second GPU path.

## What is coming

These are not built yet. They arrive in this order, each one leaving the
tree buildable:

| directory | target | holds |
|---|---|---|
| `element/` | `SigilWorldElement` | `Element`, the verbs, the transform lanes, tags, the `Generator` seam |
| `scene/` | `SigilWorldScene` | the retained side: the reconcile host operations, the EnTT store, the declared phases, extract |
| `frame/` | `SigilWorldFrame` | `Frame`, `Pass`, `Readback`, the pass selectors, the `Runtime`/`Executor` seam and `Runtime::cpu()` |
| `graph/` | `SigilWorldGraph` | ordering from declared reads and writes, transient aliasing, a backend-free barrier plan |
| `kit/` | `SigilWorldKit` | presets that compose elements: a three-point rig, a turntable, a lit set |
| `testing/` | `SigilWorldTesting` | the study harness and the plate capture |

`diligent/` grows the rest of the execution side beside its device:
pipelines from resolved `material::Program`s, the Slang compiler
registration, the GPU `Runtime` value and `importNative`. An umbrella
interface target named `SigilWorld` gathers every feature once there is
more than one worth gathering.

## Testing

```sh
ctest --test-dir build -C Debug -R world_
```

`world_light_test` runs anywhere. `world_diligent_test` needs a Vulkan
runtime (`brew install molten-vk vulkan-loader`) and *skips* rather than
fails without one, so a machine with no GPU stays green.

`world_light_bench` and `world_diligent_bench` build through the
`benches` target and run through `scripts/bench_ledger.py`; use a Release
build.
