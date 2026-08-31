// The device feature's costs: naming a resource and forgetting it, a
// texture crossing the boundary in both directions and retiring through
// the deferred queue, and a fence round trip through the queue.

#import <Metal/Metal.h>

#include <benchmark/benchmark.h>
#include <sigilskia/device/GpuDevice.h>
#include <sigilskia/device/Handle.h>

#include <string>
#include <vector>

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

/** Allocate and release one handle per iteration on a table that keeps
 *  one free slot warm — the steady-state cost of naming. */
void BM_HandleAllocateRelease(benchmark::State &state) {
  HandleTable<int, TextureHandle> table;
  for ([[maybe_unused]] auto iteration : state) {
    const TextureHandle handle = table.allocate(1);
    benchmark::DoNotOptimize(table.find(handle));
    table.release(handle);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_HandleAllocateRelease);

/** A table growing to N live handles, then every one released: what a
 *  frame's worth of transient resources costs to name. */
void BM_HandleBatch(benchmark::State &state) {
  const int count = (int)state.range(0);
  HandleTable<int, TextureHandle> table;
  std::vector<TextureHandle> handles(count);
  for ([[maybe_unused]] auto iteration : state) {
    for (int i = 0; i < count; ++i) handles[i] = table.allocate(i);
    for (int i = 0; i < count; ++i) table.release(handles[i]);
  }
  state.SetItemsProcessed(state.iterations() * count);
}
BENCHMARK(BM_HandleBatch)->Arg(64)->Arg(1024);

/** Import a host texture, export it back, destroy it, and advance the
 *  frames that retire it: the full life of a borrowed texture. */
void BM_ImportExportRetire(benchmark::State &state) {
  GpuDevice *dev = device();
  if (!dev) {
    state.SkipWithError("no Metal device");
    return;
  }
  id<MTLDevice> mtl = (id<MTLDevice>)dev->native().mtlDevice;
  MTLTextureDescriptor *desc =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                         width:64
                                                        height:64
                                                     mipmapped:NO];
  id<MTLTexture> texture = [mtl newTextureWithDescriptor:desc];
  NativeTexture native;
  native.backend = Backend::Metal;
  native.mtlTexture = (void *)texture;
  native.width = 64;
  native.height = 64;
  for ([[maybe_unused]] auto iteration : state) {
    const TextureHandle handle = dev->importNative(native);
    benchmark::DoNotOptimize(dev->exportNative(handle).mtlTexture);
    dev->destroy(handle);
    dev->beginFrame();
  }
  state.SetItemsProcessed(state.iterations());
  CFRelease((CFTypeRef)texture);
}
BENCHMARK(BM_ImportExportRetire);

/** Create a small private texture on the device and destroy it, with the
 *  frame advance that actually releases it. */
void BM_CreateDestroyTexture(benchmark::State &state) {
  GpuDevice *dev = device();
  if (!dev) {
    state.SkipWithError("no Metal device");
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
BENCHMARK(BM_CreateDestroyTexture);

/** Signal from the queue and wait on the CPU: the round trip a frame
 *  pays to know its work has finished. */
void BM_FenceRoundTrip(benchmark::State &state) {
  GpuDevice *dev = device();
  if (!dev) {
    state.SkipWithError("no Metal device");
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
BENCHMARK(BM_FenceRoundTrip);

/** Vulkan: create a small image with its memory and destroy it, with the
 *  frame advance that actually releases it. */
void BM_Vulkan_CreateDestroyTexture(benchmark::State &state) {
  std::string why;
  GpuDevice *dev = vulkanDevice(&why);
  if (!dev) {
    state.SkipWithMessage("no Vulkan device: " + why);
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
BENCHMARK(BM_Vulkan_CreateDestroyTexture);

/** Vulkan: import a VkImage borrowed, export it, destroy and retire. */
void BM_Vulkan_ImportExportRetire(benchmark::State &state) {
  std::string why;
  GpuDevice *dev = vulkanDevice(&why);
  if (!dev) {
    state.SkipWithMessage("no Vulkan device: " + why);
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
BENCHMARK(BM_Vulkan_ImportExportRetire);

/** Vulkan: signal a timeline semaphore from the queue and wait on the
 *  CPU. */
void BM_Vulkan_FenceRoundTrip(benchmark::State &state) {
  std::string why;
  GpuDevice *dev = vulkanDevice(&why);
  if (!dev) {
    state.SkipWithMessage("no Vulkan device: " + why);
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
BENCHMARK(BM_Vulkan_FenceRoundTrip);

}  // namespace

BENCHMARK_MAIN();
