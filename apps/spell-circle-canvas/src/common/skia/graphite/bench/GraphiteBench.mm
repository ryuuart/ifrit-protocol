// The graphite feature's per-frame cost: wrapping a texture someone else
// owns as an SkSurface, once through the native handle its API spells and
// once through the name a device gave it. Both arms wrap the same texture
// on the same device, so what separates them is the handle lookup and
// nothing else — which is the question a host choosing between the two
// forms is asking.
//
// EVERY ARM BUILDS ITS OWN CONTEXT. A wrap binds a surface to the
// recorder, and these arms never submit, so the recorder's state grows
// with every iteration and a later arm on a shared context would be
// measured against the litter of the ones before it — an arm's position
// in the run order, not its cost. A recording is never snapped here for
// the same reason a host must never snap to discard: a skipped ID kills
// the recorder for the life of the process.

#include <benchmark/benchmark.h>
#include <include/core/SkCanvas.h>
#include <sigilskia/device/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>

#include <memory>
#include <string>

using namespace sigil::skia;

namespace {

GpuDevice *device() {
  static std::unique_ptr<GpuDevice> d = GpuDevice::createOwned(Backend::Metal);
  return d.get();
}

/** The Vulkan device, or null with the reason — an arm on it skips. */
GpuDevice *vulkanDevice(std::string *why) {
  static std::string error;
  static std::unique_ptr<GpuDevice> d = GpuDevice::createOwned(Backend::Vulkan, &error);
  if (why) *why = error;
  return d.get();
}

/** One render target of the size a frame actually wraps, per device: the
 *  two forms must name the same texture or they are not being compared. */
TextureHandle target(GpuDevice &dev) {
  TextureDesc desc;
  desc.width = 1024;
  desc.height = 1024;
  desc.format = TextureFormat::BGRA8Unorm;
  return dev.createTexture(desc);
}

/** Wrap the texture through the API's own handle: what a host that owns
 *  its device spells. Nothing is drawn and nothing is snapped — the wrap
 *  is the whole measurement. */
void BM_Wrap_Native(benchmark::State &state) {
  GpuDevice *dev = device();
  std::unique_ptr<GraphiteContext> ctx = dev ? GraphiteContext::create(*dev) : nullptr;
  if (!ctx) {
    state.SkipWithError("no Metal device");
    return;
  }
  const TextureHandle handle = target(*dev);
  const NativeTexture native = dev->exportNative(handle);
  for (auto _ : state) {
    OffscreenSurface surface(*ctx, native.mtlTexture, native.width, native.height);
    benchmark::DoNotOptimize(surface.canvas());
  }
  state.SetItemsProcessed(state.iterations());
  dev->destroy(handle);
}
BENCHMARK(BM_Wrap_Native);

/** The same wrap through the name the device gave that texture: one
 *  generation-checked lookup ahead of the identical bring-up. */
void BM_Wrap_Handle(benchmark::State &state) {
  GpuDevice *dev = device();
  std::unique_ptr<GraphiteContext> ctx = dev ? GraphiteContext::create(*dev) : nullptr;
  if (!ctx) {
    state.SkipWithError("no Metal device");
    return;
  }
  const TextureHandle handle = target(*dev);
  for (auto _ : state) {
    OffscreenSurface surface(*ctx, *dev, handle);
    benchmark::DoNotOptimize(surface.canvas());
  }
  state.SetItemsProcessed(state.iterations());
  dev->destroy(handle);
}
BENCHMARK(BM_Wrap_Handle);

/** Vulkan: the handle wrap on a device of this library's own. */
void BM_Vulkan_Wrap_Handle(benchmark::State &state) {
  std::string why;
  GpuDevice *dev = vulkanDevice(&why);
  std::unique_ptr<GraphiteContext> ctx = dev ? GraphiteContext::create(*dev) : nullptr;
  if (!ctx) {
    state.SkipWithMessage("no Vulkan Graphite context: " + why);
    return;
  }
  const TextureHandle handle = target(*dev);
  for (auto _ : state) {
    OffscreenSurface surface(*ctx, *dev, handle);
    benchmark::DoNotOptimize(surface.canvas());
  }
  state.SetItemsProcessed(state.iterations());
  dev->destroy(handle);
}
BENCHMARK(BM_Vulkan_Wrap_Handle);

}  // namespace

BENCHMARK_MAIN();
