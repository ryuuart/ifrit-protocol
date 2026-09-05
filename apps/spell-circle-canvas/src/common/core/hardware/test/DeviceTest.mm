// A real device: what it comes up with, what it refuses to adopt, when a
// destroyed resource is really gone, who releases an imported texture,
// fences as timelines, and the levels a Metal texture is built with. Every
// case here needs a GPU, which is why the binary carries the `gpu` label
// and every case skips rather than fails where there is none.

#import <Metal/Metal.h>

#include <sigilcore/hardware/GpuDevice.h>
#include <sigilcore/hardware/Handle.h>

#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace sigil::core::hardware;

namespace {

TextureDesc smallTexture() {
  TextureDesc desc;
  desc.width = 8;
  desc.height = 8;
  desc.label = "core_hardware_device_test";
  return desc;
}

}  // namespace

/** Name a device, or skip: a machine with no GPU has nothing to say about
 *  any claim below, and reporting that is not the same as failing it. */
#define DEVICE_OR_SKIP(name)                                     \
  std::unique_ptr<GpuDevice> name = GpuDevice::createOwned();    \
  if (!name) GTEST_SKIP() << "no GPU device on this machine"

TEST(HardwareDevice, AnOwnedDeviceComesUpWithItsOwnCommandQueue) {
  DEVICE_OR_SKIP(device);
  EXPECT_EQ(device->backend(), Backend::Metal);
  EXPECT_NE(device->native().mtlDevice, nullptr);
  EXPECT_NE(device->native().mtlCommandQueue, nullptr);
  EXPECT_EQ(device->frameIndex(), 0u);
}

TEST(HardwareDevice, AdoptRefusesAnIncompleteNativeDevice) {
  // The second half hands over a real device with no queue, so the
  // refusal it reads is about the queue rather than about the machine.
  DEVICE_OR_SKIP(present);
  // Adoption takes handles somebody else owns, so a missing one is the
  // caller's mistake to hear about rather than a device to half-build.
  NativeDevice vulkan;
  vulkan.backend = Backend::Vulkan;
  std::string error;
  EXPECT_EQ(GpuDevice::adopt(vulkan, &error), nullptr);
  EXPECT_FALSE(error.empty()) << "a refusal must say what was missing";

  NativeDevice half;
  half.backend = Backend::Metal;
  half.mtlDevice = present->native().mtlDevice;
  EXPECT_EQ(GpuDevice::adopt(half), nullptr) << "a queue is a handle too";
}

TEST(HardwareDevice, ADestroyedHandleIsStaleAtOnce) {
  DEVICE_OR_SKIP(device);
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

TEST(HardwareDevice, ADestroyedResourceRetiresAtFramePlusThree) {
  DEVICE_OR_SKIP(device);
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

TEST(HardwareDevice, ABorrowedImportAndAnOwnedOneDifferInWhoReleasesIt) {
  DEVICE_OR_SKIP(device);
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

TEST(HardwareDevice, AFenceStartsUnsignalledAndEachSignalAdvancesItsValue) {
  DEVICE_OR_SKIP(device);
  const FenceHandle fence = device->createFence();
  ASSERT_TRUE(device->isValid(fence));
  EXPECT_EQ(device->completedValue(fence), kFenceInitialValue);
  EXPECT_EQ(device->signal(fence), 1u);
  EXPECT_EQ(device->signal(fence), 2u);
  device->destroyFence(fence);
}

TEST(HardwareDevice, ACpuWaitReachesASignalledValueAndTimesOutOnAnyLaterOne) {
  DEVICE_OR_SKIP(device);
  const FenceHandle fence = device->createFence();
  const FenceValue first = device->signal(fence);
  EXPECT_EQ(device->waitCpu(fence, first), FenceWait::Reached);
  EXPECT_GE(device->completedValue(fence), first);
  EXPECT_EQ(device->waitCpu(fence, first + 1, std::chrono::milliseconds(20)),
            FenceWait::TimedOut);
  device->destroyFence(fence);
}

TEST(HardwareDevice, AGpuWaitHoldsLaterWorkUntilAnotherQueueSignalsTheValue) {
  DEVICE_OR_SKIP(device);
  const FenceHandle fence = device->createFence();
  const FenceValue gate = device->signal(fence) + 1;

  // The wait is queued, then work behind it. The signal has to come from
  // ANOTHER queue: one queued behind the wait on the same queue could
  // never run.
  device->waitGpu(fence, gate);
  id<MTLCommandQueue> queue = (id<MTLCommandQueue>)device->native().mtlCommandQueue;
  id<MTLCommandBuffer> held = [queue commandBuffer];
  [held commit];

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
  device->destroyFence(fence);
}

TEST(HardwareDevice, AWaitOnAnAlreadySignalledValueNeverHolds) {
  DEVICE_OR_SKIP(device);
  const FenceHandle fence = device->createFence();
  const FenceValue reached = device->signal(fence);
  ASSERT_EQ(device->waitCpu(fence, reached), FenceWait::Reached);

  device->waitGpu(fence, reached);
  id<MTLCommandQueue> queue = (id<MTLCommandQueue>)device->native().mtlCommandQueue;
  id<MTLCommandBuffer> free = [queue commandBuffer];
  [free commit];
  [free waitUntilCompleted];
  EXPECT_EQ(free.status, MTLCommandBufferStatusCompleted);
  device->destroyFence(fence);
}

TEST(HardwareDevice, ADestroyedFenceIsStaleAndRefusesEverySpellingOfItsName) {
  DEVICE_OR_SKIP(device);
  const FenceHandle fence = device->createFence();
  device->signal(fence);
  device->destroyFence(fence);
  EXPECT_FALSE(device->isValid(fence));
  EXPECT_EQ(device->exportNative(fence), nullptr);
  EXPECT_EQ(device->signal(fence), kFenceInitialValue);
  EXPECT_EQ(device->waitCpu(fence, 1), FenceWait::Invalid);
}

TEST(HardwareDevice, ATextureIsBuiltWithTheLevelsTheSizeAllowsAndNoMore) {
  DEVICE_OR_SKIP(device);
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
