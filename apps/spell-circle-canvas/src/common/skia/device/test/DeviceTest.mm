// The device feature on Metal: handles that go stale, destruction that
// waits out the frames in flight, textures crossing the boundary in both
// directions, fences as timelines, and Graphite standing up on a device
// the host adopted.

#import <Metal/Metal.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilskia/device/GpuDevice.h>
#include <sigilskia/device/Handle.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>

#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace sigil::skia;

namespace {

TextureDesc smallTexture() {
  TextureDesc desc;
  desc.width = 8;
  desc.height = 8;
  desc.label = "sigilskia_device_test";
  return desc;
}

/** Reads a Graphite surface back to CPU pixels: snap, insert, async read,
 *  then a synchronous submit and a spin until the callback lands. */
SkBitmap readbackSurface(GraphiteContext &ctx, SkSurface *surface) {
  SkBitmap bm;
  const SkImageInfo info = surface->imageInfo();
  if (auto recording = ctx.recorder()->snap()) {
    skgpu::graphite::InsertRecordingInfo insert;
    insert.fRecording = recording.get();
    ctx.context()->insertRecording(insert);
  }
  struct Read {
    std::unique_ptr<const SkImage::AsyncReadResult> result;
    bool called = false;
  } read;
  ctx.context()->asyncRescaleAndReadPixels(
      surface, info, SkIRect::MakeWH(info.width(), info.height()), SkImage::RescaleGamma::kSrc,
      SkImage::RescaleMode::kNearest,
      [](SkImage::ReadPixelsContext c, std::unique_ptr<const SkImage::AsyncReadResult> r) {
        auto *out = static_cast<Read *>(c);
        out->result = std::move(r);
        out->called = true;
      },
      &read);
  skgpu::graphite::SubmitInfo submitInfo;
  submitInfo.fSync = skgpu::graphite::SyncToCpu::kYes;
  ctx.context()->submit(submitInfo);
  for (int spin = 0; spin < 5000 && !read.called; ++spin) ctx.context()->checkAsyncWorkCompletion();
  if (!read.result) return bm;
  bm.allocPixels(info);
  const auto *src = static_cast<const uint8_t *>(read.result->data(0));
  const size_t srcRowBytes = read.result->rowBytes(0);
  for (int y = 0; y < info.height(); ++y)
    std::memcpy(bm.pixmap().writable_addr(0, y), src + (size_t)y * srcRowBytes,
                std::min(srcRowBytes, bm.rowBytes()));
  return bm;
}

}  // namespace

TEST(SigilSkiaHandle, NullHandleNamesNothing) {
  HandleTable<int, TextureHandle> table;
  EXPECT_FALSE(table.contains(TextureHandle{}));
  EXPECT_EQ(table.find(TextureHandle{}), nullptr);
  EXPECT_EQ(table.release(TextureHandle{}), 0);
}

TEST(SigilSkiaHandle, ReusedSlotRejectsTheOldHandle) {
  HandleTable<int, TextureHandle> table;
  const TextureHandle first = table.allocate(1);
  ASSERT_TRUE(table.contains(first));
  EXPECT_EQ(table.release(first), 1);
  EXPECT_FALSE(table.contains(first));

  const TextureHandle second = table.allocate(2);
  EXPECT_EQ(second.index, first.index) << "the freed slot is reused";
  EXPECT_NE(second.generation, first.generation);
  EXPECT_NE(second, first);
  EXPECT_FALSE(table.contains(first)) << "the stale handle stays stale";
  EXPECT_TRUE(table.contains(second));
  EXPECT_EQ(*table.find(second), 2);
  EXPECT_EQ(table.find(first), nullptr);
  EXPECT_EQ(table.release(first), 0) << "a stale release releases nothing";
  EXPECT_TRUE(table.contains(second));
}

TEST(SigilSkiaHandle, TypedHandlesDoNotMix) {
  static_assert(!std::is_convertible_v<TextureHandle, BufferHandle>);
  static_assert(!std::is_convertible_v<FenceHandle, TextureHandle>);
}

TEST(SigilSkiaHandle, DrainStalesEveryHandle) {
  HandleTable<int, BufferHandle> table;
  const BufferHandle a = table.allocate(1);
  const BufferHandle b = table.allocate(2);
  const std::vector<int> drained = table.drain();
  EXPECT_EQ(drained, (std::vector<int>{1, 2}));
  EXPECT_EQ(table.size(), 0u);
  EXPECT_FALSE(table.contains(a));
  EXPECT_FALSE(table.contains(b));
}

TEST(SigilSkiaDevice, OwnedMetalDeviceHasAQueue) {
  auto device = GpuDevice::createOwned(Backend::Metal);
  ASSERT_NE(device, nullptr) << "no Metal device";
  EXPECT_EQ(device->backend(), Backend::Metal);
  EXPECT_NE(device->native().mtlDevice, nullptr);
  EXPECT_NE(device->native().mtlCommandQueue, nullptr);
  EXPECT_EQ(device->frameIndex(), 0u);
}

TEST(SigilSkiaDevice, AdoptVulkanNeedsEveryHandle) {
  NativeDevice vulkan;
  vulkan.backend = Backend::Vulkan;
  std::string error;
  EXPECT_EQ(GpuDevice::adopt(vulkan, &error), nullptr);
  EXPECT_FALSE(error.empty());
}

TEST(SigilSkiaDevice, AdoptNeedsBothHandles) {
  NativeDevice half;
  half.backend = Backend::Metal;
  half.mtlDevice = (void *)MTLCreateSystemDefaultDevice();
  EXPECT_EQ(GpuDevice::adopt(half), nullptr);
}

TEST(SigilSkiaDevice, DestroyedHandleIsStaleAtOnce) {
  auto device = GpuDevice::createOwned(Backend::Metal);
  ASSERT_NE(device, nullptr);
  const TextureHandle texture = device->createTexture(smallTexture());
  ASSERT_TRUE(device->isValid(texture));
  EXPECT_TRUE(device->exportNative(texture));
  device->destroy(texture);
  EXPECT_FALSE(device->isValid(texture));
  EXPECT_FALSE(device->exportNative(texture));
  device->destroy(texture);  // a second destroy does nothing
  EXPECT_EQ(device->pendingDestroys(), 1u);

  const TextureHandle next = device->createTexture(smallTexture());
  EXPECT_EQ(next.index, texture.index) << "the slot is reused";
  EXPECT_FALSE(device->isValid(texture)) << "and the old handle still fails";
  EXPECT_TRUE(device->isValid(next));
}

TEST(SigilSkiaDevice, DestroyRetiresAtFramePlusThree) {
  auto device = GpuDevice::createOwned(Backend::Metal);
  ASSERT_NE(device, nullptr);
  device->beginFrame();
  device->beginFrame();
  ASSERT_EQ(device->frameIndex(), 2u);
  const TextureHandle texture = device->createTexture(smallTexture());
  id<MTLTexture> native = (id<MTLTexture>)device->exportNative(texture).mtlTexture;
  // The test's own reference keeps the object alive past the device's
  // release, so the count can be read after it.
  CFRetain((CFTypeRef)native);
  const NSUInteger held = CFGetRetainCount((CFTypeRef)native);
  device->destroy(texture);  // in frame 2
  EXPECT_EQ(device->pendingDestroys(), 1u);
  EXPECT_EQ(CFGetRetainCount((CFTypeRef)native), held) << "still held";
  device->beginFrame();  // 3
  EXPECT_EQ(device->pendingDestroys(), 1u);
  device->beginFrame();  // 4
  EXPECT_EQ(device->pendingDestroys(), 1u);
  device->beginFrame();  // 5 = 2 + kFramesInFlight
  EXPECT_EQ(device->pendingDestroys(), 0u);
  EXPECT_EQ(CFGetRetainCount((CFTypeRef)native), held - 1) << "released";
  CFRelease((CFTypeRef)native);
}

TEST(SigilSkiaDevice, ImportExportRoundTrip) {
  auto device = GpuDevice::createOwned(Backend::Metal);
  ASSERT_NE(device, nullptr);
  id<MTLDevice> mtl = (id<MTLDevice>)device->native().mtlDevice;
  MTLTextureDescriptor *desc =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                         width:16
                                                        height:4
                                                     mipmapped:NO];
  id<MTLTexture> hostTexture = [mtl newTextureWithDescriptor:desc];
  // Released at the end of the test. The one path that skips that
  // release is a failed assertion, which ends the run.
  // NOLINTNEXTLINE(clang-analyzer-osx.cocoa.RetainCount)
  ASSERT_NE(hostTexture, nil);

  NativeTexture native;
  native.backend = Backend::Metal;
  native.mtlTexture = (void *)hostTexture;
  native.width = 16;
  native.height = 4;

  // Borrowed: the device forgets it on destroy and never releases it.
  const NSUInteger before = CFGetRetainCount((CFTypeRef)hostTexture);
  const TextureHandle borrowed = device->importNative(native);
  ASSERT_TRUE(device->isValid(borrowed));
  const NativeTexture out = device->exportNative(borrowed);
  EXPECT_EQ(out.mtlTexture, (void *)hostTexture);
  EXPECT_EQ(out.width, 16);
  EXPECT_EQ(out.height, 4);
  EXPECT_EQ(out.backend, Backend::Metal);
  EXPECT_EQ(CFGetRetainCount((CFTypeRef)hostTexture), before);
  device->destroy(borrowed);
  for (int i = 0; i < 4; ++i) device->beginFrame();
  EXPECT_EQ(CFGetRetainCount((CFTypeRef)hostTexture), before);

  // Owned: the device takes its own reference and drops it at retire.
  const TextureHandle owned = device->importNative(native, /*takeOwnership=*/true);
  EXPECT_EQ(CFGetRetainCount((CFTypeRef)hostTexture), before + 1);
  device->destroy(owned);
  for (int i = 0; i < 4; ++i) device->beginFrame();
  EXPECT_EQ(CFGetRetainCount((CFTypeRef)hostTexture), before);

  // Another API's texture is refused.
  NativeTexture vulkan;
  vulkan.backend = Backend::Vulkan;
  vulkan.vkImage = 1;
  EXPECT_FALSE(device->importNative(vulkan));
  EXPECT_FALSE(device->importNative(NativeTexture{}));
  CFRelease((CFTypeRef)hostTexture);
}

TEST(SigilSkiaDevice, FenceSignalsAndWaits) {
  auto device = GpuDevice::createOwned(Backend::Metal);
  ASSERT_NE(device, nullptr);
  const FenceHandle fence = device->createFence();
  ASSERT_TRUE(device->isValid(fence));
  EXPECT_EQ(device->completedValue(fence), kFenceInitialValue);

  const FenceValue first = device->signal(fence);
  EXPECT_EQ(first, 1u);
  EXPECT_EQ(device->waitCpu(fence, first), FenceWait::Reached);
  EXPECT_GE(device->completedValue(fence), first);
  EXPECT_EQ(device->waitCpu(fence, first + 1, std::chrono::milliseconds(20)), FenceWait::TimedOut);

  // A GPU-side wait holds later work on the device's queue: a command
  // buffer committed after the wait completes only once the value is
  // signalled — from another queue, because a signal queued behind the
  // wait on the same queue could never run.
  const FenceValue gate = first + 1;
  device->waitGpu(fence, gate);
  id<MTLCommandQueue> queue = (id<MTLCommandQueue>)device->native().mtlCommandQueue;
  id<MTLCommandBuffer> held = [queue commandBuffer];
  [held commit];
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  EXPECT_NE(held.status, MTLCommandBufferStatusCompleted) << "held by the wait";

  id<MTLSharedEvent> event = (id<MTLSharedEvent>)device->exportNative(fence);
  ASSERT_NE(event, nil);
  id<MTLCommandQueue> other = [(id<MTLDevice>)device->native().mtlDevice newCommandQueue];
  id<MTLCommandBuffer> release = [other commandBuffer];
  [release encodeSignalEvent:event value:gate];
  [release commit];
  [held waitUntilCompleted];
  EXPECT_EQ(held.status, MTLCommandBufferStatusCompleted);
  EXPECT_EQ(device->waitCpu(fence, gate), FenceWait::Reached);
  CFRelease((CFTypeRef)other);

  // An already-signalled value never holds.
  device->waitGpu(fence, gate);
  id<MTLCommandBuffer> free = [queue commandBuffer];
  [free commit];
  [free waitUntilCompleted];
  EXPECT_EQ(free.status, MTLCommandBufferStatusCompleted);

  device->destroyFence(fence);
  EXPECT_FALSE(device->isValid(fence));
  EXPECT_EQ(device->exportNative(fence), nullptr);
  EXPECT_EQ(device->signal(fence), kFenceInitialValue);
  EXPECT_EQ(device->waitCpu(fence, 1), FenceWait::Invalid);
}

TEST(SigilSkiaDevice, GraphiteOnAnAdoptedDeviceClearsAndReadsBack) {
  id<MTLDevice> mtl = MTLCreateSystemDefaultDevice();
  id<MTLCommandQueue> mtlQueue = [mtl newCommandQueue];
  // Both are released at the end of the test. The one path that skips
  // those releases is a failed assertion, which ends the run.
  // NOLINTNEXTLINE(clang-analyzer-osx.cocoa.RetainCount)
  ASSERT_NE(mtl, nil);
  NativeDevice native;
  native.backend = Backend::Metal;
  native.mtlDevice = (void *)mtl;
  native.mtlCommandQueue = (void *)mtlQueue;
  auto device = GpuDevice::adopt(native);
  ASSERT_NE(device, nullptr);
  EXPECT_EQ(device->native().mtlDevice, (void *)mtl);

  std::unique_ptr<GraphiteContext> graphite = GraphiteContext::create(*device);
  ASSERT_NE(graphite, nullptr);

  TextureDesc desc = smallTexture();
  desc.format = TextureFormat::BGRA8Unorm;
  desc.cpuAccessible = true;
  const TextureHandle texture = device->createTexture(desc);
  ASSERT_TRUE(device->isValid(texture));
  const NativeTexture nativeTexture = device->exportNative(texture);

  OffscreenSurface surface(*graphite, nativeTexture.mtlTexture, nativeTexture.width,
                           nativeTexture.height);
  ASSERT_NE(surface.canvas(), nullptr);
  surface.canvas()->clear(SkColorSetARGB(255, 0, 0, 255));
  surface.submit();

  // The fence is on the same queue as Graphite's submission, so reaching
  // it means the clear has landed.
  const FenceHandle fence = device->createFence();
  const FenceValue done = device->signal(fence);
  ASSERT_EQ(device->waitCpu(fence, done), FenceWait::Reached);

  std::vector<uint8_t> bytes(size_t(desc.width) * desc.height * 4);
  [(id<MTLTexture>)nativeTexture.mtlTexture getBytes:bytes.data()
                                         bytesPerRow:size_t(desc.width) * 4
                                          fromRegion:MTLRegionMake2D(0, 0, desc.width, desc.height)
                                         mipmapLevel:0];
  // BGRA, opaque blue.
  EXPECT_EQ(bytes[0], 255);
  EXPECT_EQ(bytes[1], 0);
  EXPECT_EQ(bytes[2], 0);
  EXPECT_EQ(bytes[3], 255);

  device->destroyFence(fence);
  device->destroy(texture);
  graphite.reset();
  device.reset();
  // The adopted objects are still the host's.
  EXPECT_NE(mtl, nil);
  CFRelease((CFTypeRef)mtlQueue);
  CFRelease((CFTypeRef)mtl);
}

// ---------------------------------------------------------------------------
// The Vulkan backend. Every arm skips, naming why, on a machine without a
// Vulkan loader and driver (on macOS: brew install molten-vk vulkan-loader).

namespace {

/** A Vulkan device this process owns, or the reason there is none. */
GpuDevice *vulkanDevice(std::string *why) {
  static std::string error;
  static std::unique_ptr<GpuDevice> device = GpuDevice::createOwned(Backend::Vulkan, &error);
  if (why) *why = error;
  return device.get();
}

// `var` names the variable the macro declares, and a declarator cannot
// be parenthesised; every caller passes a plain identifier.
// NOLINTBEGIN(bugprone-macro-parentheses)
#define SIGILSKIA_NEED_VULKAN(var)             \
  std::string vulkanError;                     \
  GpuDevice *var = vulkanDevice(&vulkanError); \
  if (!(var)) GTEST_SKIP() << "no Vulkan device: " << vulkanError
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace

TEST(SigilSkiaDeviceVulkan, OwnedDeviceHasEveryHandle) {
  SIGILSKIA_NEED_VULKAN(device);
  EXPECT_EQ(device->backend(), Backend::Vulkan);
  const VulkanHandles &handles = device->native().vulkan;
  EXPECT_NE(handles.instance, nullptr);
  EXPECT_NE(handles.physicalDevice, nullptr);
  EXPECT_NE(handles.device, nullptr);
  EXPECT_NE(handles.queue, nullptr);
  EXPECT_NE(handles.getInstanceProcAddr, nullptr);
  EXPECT_NE(handles.apiVersion, 0u);
}

TEST(SigilSkiaDeviceVulkan, AdoptsItsOwnHandles) {
  SIGILSKIA_NEED_VULKAN(owned);
  // The owned device's handles, adopted by a second device object that
  // never frees them: both name textures on one VkDevice.
  std::string error;
  auto adopted = GpuDevice::adopt(owned->native(), &error);
  ASSERT_NE(adopted, nullptr) << error;
  EXPECT_EQ(adopted->native().vulkan.device, owned->native().vulkan.device);
  TextureDesc desc = smallTexture();
  const TextureHandle texture = adopted->createTexture(desc);
  ASSERT_TRUE(adopted->isValid(texture));
  const NativeTexture native = adopted->exportNative(texture);
  EXPECT_EQ(native.backend, Backend::Vulkan);
  EXPECT_NE(native.vkImage, 0u);
  EXPECT_NE(native.vkMemory, 0u);
  adopted->destroy(texture);
  adopted.reset();  // releases what it still holds; the VkDevice stays
  EXPECT_NE(owned->native().vulkan.device, nullptr);
}

TEST(SigilSkiaDeviceVulkan, TextureFormatsMapAndRetire) {
  SIGILSKIA_NEED_VULKAN(device);
  const TextureFormat formats[] = {TextureFormat::RGBA8Unorm, TextureFormat::BGRA8Unorm,
                                   TextureFormat::RGBA16Float};
  const uint32_t expected[] = {37 /*R8G8B8A8_UNORM*/, 44 /*B8G8R8A8_UNORM*/,
                               97 /*R16G16B16A16_SFLOAT*/};
  for (int i = 0; i < 3; ++i) {
    TextureDesc desc = smallTexture();
    desc.format = formats[i];
    const TextureHandle texture = device->createTexture(desc);
    ASSERT_TRUE(device->isValid(texture)) << "format " << i;
    const NativeTexture native = device->exportNative(texture);
    EXPECT_EQ(native.vkFormat, expected[i]);
    EXPECT_EQ(native.width, 8);
    EXPECT_EQ(native.vkLayout, 0u) << "undefined until drawn";
    device->destroy(texture);
    EXPECT_FALSE(device->isValid(texture));
  }
  EXPECT_EQ(device->pendingDestroys(), 3u);
  for (int i = 0; i < 3; ++i) device->beginFrame();
  EXPECT_EQ(device->pendingDestroys(), 0u);

  TextureDesc cpu = smallTexture();
  cpu.cpuAccessible = true;
  const TextureHandle hostVisible = device->createTexture(cpu);
  EXPECT_TRUE(device->isValid(hostVisible));
  device->destroy(hostVisible);
}

TEST(SigilSkiaDeviceVulkan, ImportExportRoundTrip) {
  SIGILSKIA_NEED_VULKAN(device);
  // A device-made image stands in for the host's: exported, imported
  // borrowed under a second name, exported again unchanged.
  const TextureHandle original = device->createTexture(smallTexture());
  const NativeTexture native = device->exportNative(original);
  ASSERT_TRUE(native);
  const TextureHandle borrowed = device->importNative(native);
  ASSERT_TRUE(device->isValid(borrowed));
  EXPECT_NE(borrowed, original);
  const NativeTexture again = device->exportNative(borrowed);
  EXPECT_EQ(again.vkImage, native.vkImage);
  EXPECT_EQ(again.vkFormat, native.vkFormat);
  EXPECT_EQ(again.height, native.height);
  device->destroy(borrowed);  // forgets only; the image stays the original's
  for (int i = 0; i < 3; ++i) device->beginFrame();
  EXPECT_TRUE(device->exportNative(original));
  device->destroy(original);

  NativeTexture metal;
  metal.backend = Backend::Metal;
  metal.mtlTexture = &metal;
  EXPECT_FALSE(device->importNative(metal)) << "another API's texture";
}

TEST(SigilSkiaDeviceVulkan, TimelineFenceSignalsAndWaits) {
  SIGILSKIA_NEED_VULKAN(device);
  const FenceHandle fence = device->createFence();
  ASSERT_TRUE(device->isValid(fence));
  EXPECT_EQ(device->completedValue(fence), kFenceInitialValue);
  EXPECT_NE(device->exportNative(fence), nullptr);

  const FenceValue first = device->signal(fence);
  EXPECT_EQ(first, 1u);
  EXPECT_EQ(device->waitCpu(fence, first), FenceWait::Reached);
  EXPECT_GE(device->completedValue(fence), first);
  EXPECT_EQ(device->waitCpu(fence, first + 1, std::chrono::milliseconds(20)), FenceWait::TimedOut);

  // A wait on the queue for a value already reached holds nothing; the
  // signal queued after it is reached in turn.
  device->waitGpu(fence, first);
  const FenceValue second = device->signal(fence);
  EXPECT_EQ(second, first + 1);
  EXPECT_EQ(device->waitCpu(fence, second), FenceWait::Reached);

  device->destroyFence(fence);
  EXPECT_FALSE(device->isValid(fence));
  EXPECT_EQ(device->exportNative(fence), nullptr);
  EXPECT_EQ(device->waitCpu(fence, 1), FenceWait::Invalid);
}

TEST(SigilSkiaDeviceVulkan, GraphiteClearsAndReadsBack) {
  SIGILSKIA_NEED_VULKAN(device);
  std::unique_ptr<GraphiteContext> graphite = GraphiteContext::create(*device);
  ASSERT_NE(graphite, nullptr) << "Graphite on the Vulkan device";

  // A Graphite-owned target first: the context alone, no wrap. Every
  // surface lives inside the context's lifetime — its memory is freed
  // through the context — so each is scoped to go first.
  SkBitmap pixels;
  {
    const SkImageInfo info = SkImageInfo::MakeN32Premul(8, 8);
    sk_sp<SkSurface> target = SkSurfaces::RenderTarget(graphite->recorder(), info);
    ASSERT_NE(target, nullptr);
    target->getCanvas()->clear(SkColorSetARGB(255, 0, 255, 0));
    pixels = readbackSurface(*graphite, target.get());
    ASSERT_FALSE(pixels.empty());
    EXPECT_EQ(pixels.getColor(3, 3), SkColorSetARGB(255, 0, 255, 0));
  }

  // Then a device-created VkImage wrapped as the surface.
  TextureDesc desc = smallTexture();
  desc.format = TextureFormat::RGBA8Unorm;
  const TextureHandle texture = device->createTexture(desc);
  {
    const NativeTexture native = device->exportNative(texture);
    VulkanImage image;
    image.image = native.vkImage;
    image.layout = native.vkLayout;
    image.format = native.vkFormat;
    image.width = native.width;
    image.height = native.height;
    OffscreenSurface surface(*graphite, image);
    ASSERT_NE(surface.canvas(), nullptr);
    surface.canvas()->clear(SkColorSetARGB(255, 0, 0, 255));
    pixels = readbackSurface(*graphite, surface.surface());
    ASSERT_FALSE(pixels.empty());
    EXPECT_EQ(pixels.getColor(0, 0), SkColorSetARGB(255, 0, 0, 255));
    EXPECT_EQ(pixels.getColor(7, 7), SkColorSetARGB(255, 0, 0, 255));
  }
  graphite.reset();
  device->destroy(texture);
}
