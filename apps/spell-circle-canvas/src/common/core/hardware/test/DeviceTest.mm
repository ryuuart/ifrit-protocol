// The hardware feature on Metal and on Vulkan: handles that go stale,
// destruction that waits out the frames in flight, textures crossing the
// boundary in both directions, mip chains as deep as a size allows, and
// fences as timelines.

#import <Metal/Metal.h>

#include <sigilcore/hardware/GpuDevice.h>
#include <sigilcore/hardware/Handle.h>

#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace sigil::core::hardware;

namespace {

TextureDesc smallTexture() {
  TextureDesc desc;
  desc.width = 8;
  desc.height = 8;
  desc.label = "core_hardware_test";
  return desc;
}

}  // namespace

TEST(SigilCoreHardwareHandle, NullHandleNamesNothing) {
  HandleTable<int, TextureHandle> table;
  EXPECT_FALSE(table.contains(TextureHandle{}));
  EXPECT_EQ(table.find(TextureHandle{}), nullptr);
  EXPECT_EQ(table.release(TextureHandle{}), 0);
}

TEST(SigilCoreHardwareHandle, ReusedSlotRejectsTheOldHandle) {
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

TEST(SigilCoreHardwareHandle, TypedHandlesDoNotMix) {
  static_assert(!std::is_convertible_v<TextureHandle, BufferHandle>);
  static_assert(!std::is_convertible_v<FenceHandle, TextureHandle>);
}

TEST(SigilCoreHardwareHandle, DrainStalesEveryHandle) {
  HandleTable<int, BufferHandle> table;
  const BufferHandle a = table.allocate(1);
  const BufferHandle b = table.allocate(2);
  const std::vector<int> drained = table.drain();
  EXPECT_EQ(drained, (std::vector<int>{1, 2}));
  EXPECT_EQ(table.size(), 0u);
  EXPECT_FALSE(table.contains(a));
  EXPECT_FALSE(table.contains(b));
}

TEST(SigilCoreHardware, OwnedMetalDeviceHasAQueue) {
  auto device = GpuDevice::createOwned();
  ASSERT_NE(device, nullptr) << "no Metal device";
  EXPECT_EQ(device->backend(), Backend::Metal);
  EXPECT_NE(device->native().mtlDevice, nullptr);
  EXPECT_NE(device->native().mtlCommandQueue, nullptr);
  EXPECT_EQ(device->frameIndex(), 0u);
}

TEST(SigilCoreHardware, AdoptVulkanNeedsEveryHandle) {
  NativeDevice vulkan;
  vulkan.backend = Backend::Vulkan;
  std::string error;
  EXPECT_EQ(GpuDevice::adopt(vulkan, &error), nullptr);
  EXPECT_FALSE(error.empty());
}

TEST(SigilCoreHardware, AdoptNeedsBothHandles) {
  NativeDevice half;
  half.backend = Backend::Metal;
  half.mtlDevice = (void *)MTLCreateSystemDefaultDevice();
  EXPECT_EQ(GpuDevice::adopt(half), nullptr);
}

TEST(SigilCoreHardware, DestroyedHandleIsStaleAtOnce) {
  auto device = GpuDevice::createOwned();
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

TEST(SigilCoreHardware, DestroyRetiresAtFramePlusThree) {
  auto device = GpuDevice::createOwned();
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

TEST(SigilCoreHardware, ImportExportRoundTrip) {
  auto device = GpuDevice::createOwned();
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

TEST(SigilCoreHardware, FenceSignalsAndWaits) {
  auto device = GpuDevice::createOwned();
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

TEST(SigilCoreHardware, AMipChainIsAsDeepAsTheSizeAllows) {
  // The rule has no backend in it, so it is checked without one.
  EXPECT_EQ(mipLevelsFor(1, 1), 1);
  EXPECT_EQ(mipLevelsFor(8, 8), 4);
  EXPECT_EQ(mipLevelsFor(256, 128), 9);
  // A chain runs until BOTH sides are one, so a wide panorama keeps
  // levels after its height has bottomed out.
  EXPECT_EQ(mipLevelsFor(1024, 2), 11);
}

TEST(SigilCoreHardware, MetalTextureCarriesTheLevelsItWasAskedFor) {
  std::unique_ptr<GpuDevice> device = GpuDevice::createOwned();
  ASSERT_TRUE(device);
  TextureDesc desc = smallTexture();
  desc.width = 256;
  desc.height = 128;
  desc.mipLevels = 9;
  const TextureHandle chained = device->createTexture(desc);
  ASSERT_TRUE(device->isValid(chained));
  EXPECT_EQ(device->exportNative(chained).mipLevels, 9);
  id<MTLTexture> native = (__bridge id<MTLTexture>)device->exportNative(chained).mtlTexture;
  EXPECT_EQ((int)native.mipmapLevelCount, 9);
  device->destroy(chained);

  // More levels than the size can carry is what the size can carry, and
  // fewer than one is one.
  desc.mipLevels = 40;
  const TextureHandle clamped = device->createTexture(desc);
  EXPECT_EQ(device->exportNative(clamped).mipLevels, mipLevelsFor(256, 128));
  device->destroy(clamped);
  desc.mipLevels = 0;
  const TextureHandle flat = device->createTexture(desc);
  EXPECT_EQ(device->exportNative(flat).mipLevels, 1);
  device->destroy(flat);
}
