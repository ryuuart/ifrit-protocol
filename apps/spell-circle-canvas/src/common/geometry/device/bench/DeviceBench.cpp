/** @file
 * geometry_device_bench — the way in and what the device then costs:
 * bringing the one device up and turning it into a device both APIs draw
 * on, then naming a texture on it, borrowing one, signalling a fence and
 * wrapping a texture as a Graphite surface. Run a Release build; Debug
 * numbers say nothing. Needs a Vulkan runtime and reports a skip without
 * one.
 *
 * THE TIMED ARM IS A PROPERTY OF THIS LIBRARY, because the bench ledger
 * judges every timed arm against a band. The driver's own cost is not:
 * it makes a Vulkan device more slowly the more devices a process has
 * already made. It is measured outside the timed region and reported as
 * a COUNTER, which the ledger never judges.
 */

#include <benchmark/benchmark.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilgeometry/device/Device.h>
#include <sigilmeasure/time/Stopwatch.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>

#include <memory>
#include <string>

// The adoption itself, which is this feature's own seam rather than a
// public header of it.
#include "AdoptDevice.h"

using namespace sigil;
using namespace sigil::geometry::device;
// The hardware device's own vocabulary — its texture and fence handles —
// is SigilCoreHardware's, and the arms below spell it as its own library
// does rather than through the one that adopts it.
using namespace sigil::core::hardware;

namespace {

/** The one device this process brings up, kept for its lifetime, and the
 *  adopted device and Graphite context standing on it. Making a second
 *  Vulkan device costs the driver more the more it has already made, and
 *  every arm below wants the same one anyway. */
Device *sharedDevice(std::string *why) {
  static std::string error;
  static std::unique_ptr<Device> device = [] {
    const DeviceConfig config;
    return Device::create(config, &error);
  }();
  if (why) *why = error;
  return device.get();
}

core::hardware::GpuDevice *adoptedDevice(std::string *why) {
  Device *made = sharedDevice(why);
  if (!made) return nullptr;
  if (!made->gpu() && why) *why = "the device was created but not adopted";
  return made->gpu();
}

skia::GraphiteContext *adoptedGraphite() {
  Device *made = sharedDevice(nullptr);
  return made ? made->graphite() : nullptr;
}

/** One render target of the size a frame actually wraps. */
core::hardware::TextureHandle target(core::hardware::GpuDevice &dev) {
  core::hardware::TextureDesc desc;
  desc.width = 1024;
  desc.height = 1024;
  desc.format = core::hardware::TextureFormat::BGRA8Unorm;
  return dev.createTexture(desc);
}

/** THE WAY IN, less the driver: the Vulkan handles read off Diligent's
 *  interfaces, those handles and the loader entry point this process
 *  already opened handed to the hardware device, and Graphite stood up on what
 *  comes back. That is the whole of what this library does to turn one
 *  device into a device both APIs draw on, and it is measured against a
 *  device that is already standing.
 *
 *  `bringup_ms` is the whole way in — the driver's device creation and
 *  this — for the one device every arm in this file shares, taken once
 *  and outside every timed region. Teardown is untimed. */
void BM_DeviceAdopt(benchmark::State& state) {
  std::string error;
  const measure::Stopwatch bringUp;
  Device* device = sharedDevice(&error);
  const double bringUpMs = bringUp.elapsedMs();
  if (!device) {
    state.SkipWithError(error);
    return;
  }
  if (!device->gpu()) {
    state.SkipWithError("the device was created but not adopted");
    return;
  }
  state.counters["bringup_ms"] = bringUpMs;

  for ([[maybe_unused]] auto iteration : state) {
    std::unique_ptr<core::hardware::GpuDevice> gpu = adoptVulkanDevice(device->renderDevice(),
                                                      device->context(), &error);
    std::unique_ptr<skia::GraphiteContext> graphite;
    if (gpu) graphite = skia::GraphiteContext::create(*gpu);
    benchmark::DoNotOptimize(graphite);
    state.PauseTiming();
    // Graphite borrows the adopted device, so it goes first; the adopted
    // device frees none of the Vulkan objects Diligent owns.
    graphite.reset();
    gpu.reset();
    state.ResumeTiming();
  }
}
BENCHMARK(BM_DeviceAdopt)->Unit(benchmark::kMillisecond);

/** On the adopted device: create a small image with its memory and destroy it, with the
 *  frame advance that actually releases it. */
void BM_Adopted_CreateDestroyTexture(benchmark::State &state) {
  std::string why;
  core::hardware::GpuDevice *dev = adoptedDevice(&why);
  if (!dev) {
    state.SkipWithMessage("no adopted device: " + why);
    return;
  }
  TextureDesc desc;
  desc.width = 64;
  desc.height = 64;
  for ([[maybe_unused]] auto iteration : state) {
    const TextureHandle handle = dev->createTexture(desc);
    dev->destroy(handle);
    dev->beginFrame();
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Adopted_CreateDestroyTexture);

/** On the adopted device: import a VkImage borrowed, export it, destroy and retire. */
void BM_Adopted_ImportExportRetire(benchmark::State &state) {
  std::string why;
  core::hardware::GpuDevice *dev = adoptedDevice(&why);
  if (!dev) {
    state.SkipWithMessage("no adopted device: " + why);
    return;
  }
  TextureDesc desc;
  desc.width = 64;
  desc.height = 64;
  const TextureHandle original = dev->createTexture(desc);
  const NativeTexture native = dev->exportNative(original);
  for ([[maybe_unused]] auto iteration : state) {
    const TextureHandle handle = dev->importNative(native);
    benchmark::DoNotOptimize(dev->exportNative(handle).vkImage);
    dev->destroy(handle);
    dev->beginFrame();
  }
  state.SetItemsProcessed(state.iterations());
  dev->destroy(original);
}
BENCHMARK(BM_Adopted_ImportExportRetire);

/** On the adopted device: signal a timeline semaphore from the queue and wait on the
 *  CPU. */
void BM_Adopted_FenceRoundTrip(benchmark::State &state) {
  std::string why;
  core::hardware::GpuDevice *dev = adoptedDevice(&why);
  if (!dev) {
    state.SkipWithMessage("no adopted device: " + why);
    return;
  }
  const FenceHandle fence = dev->createFence();
  for ([[maybe_unused]] auto iteration : state) {
    const FenceValue value = dev->signal(fence);
    if (dev->waitCpu(fence, value) != FenceWait::Reached) {
      state.SkipWithError("fence did not signal");
      break;
    }
  }
  state.SetItemsProcessed(state.iterations());
  dev->destroyFence(fence);
}
BENCHMARK(BM_Adopted_FenceRoundTrip);

void BM_Adopted_Wrap_Handle(benchmark::State &state) {
  std::string why;
  core::hardware::GpuDevice *dev = adoptedDevice(&why);
  skia::GraphiteContext *ctx = adoptedGraphite();
  if (!dev || !ctx) {
    state.SkipWithMessage("no Graphite on the adopted device: " + why);
    return;
  }
  const core::hardware::TextureHandle handle = target(*dev);
  for ([[maybe_unused]] auto iteration : state) {
    skia::OffscreenSurface surface(*ctx, *dev, handle);
    benchmark::DoNotOptimize(surface.canvas());
  }
  state.SetItemsProcessed(state.iterations());
  dev->destroy(handle);
}
BENCHMARK(BM_Adopted_Wrap_Handle);

}  // namespace

BENCHMARK_MAIN();
