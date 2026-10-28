/** @file
 * The device the whole process shares, adopted from the one Diligent made.
 * A Vulkan device is never created here — Diligent owns the API and makes
 * it, and the hardware device adopts what it made — so what these arms
 * exercise is the adopted path and nothing else. Every one skips, naming
 * why, on a machine without a Vulkan runtime (on macOS: brew install
 * molten-vk vulkan-loader).
 */

#include <gtest/gtest.h>
#include <sigilgeometry/device/Device.h>
#include <sigilgpu/GpuDevice.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "DeviceFixtures.h"
#include "OnDevice.h"

using namespace sigil;
using namespace sigil::geometry::device::test;

namespace {

using gpu::Backend;
using gpu::FenceHandle;
using gpu::FenceValue;
using gpu::FenceWait;
using gpu::GpuDevice;
using gpu::kFenceInitialValue;
using gpu::NativeFence;
using gpu::NativeTexture;
using gpu::TextureDescription;
using gpu::TextureFormat;
using gpu::TextureHandle;
using gpu::VulkanHandles;

}  // namespace

TEST(AdoptedDevice, CarriesEveryVulkanHandle) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  GpuDevice* device = on->gpu();
  if (!device) GTEST_SKIP() << "the device was created but not adopted";
  EXPECT_EQ(device->backend(), Backend::Vulkan);
  const VulkanHandles& handles = device->native().vulkan;
  EXPECT_NE(handles.instance, nullptr);
  EXPECT_NE(handles.physicalDevice, nullptr);
  EXPECT_NE(handles.device, nullptr);
  EXPECT_NE(handles.queue, nullptr);
  EXPECT_NE(handles.getInstanceProcAddr, nullptr);
  EXPECT_NE(handles.apiVersion, 0u);
}

TEST(AdoptedDevice, AdoptsItsOwnHandlesAgain) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  GpuDevice* owned = on->gpu();
  if (!owned) GTEST_SKIP() << "the device was created but not adopted";
  // The adopted device's handles, adopted again by a second device
  // object that also frees none of them: both name textures on the one
  // VkDevice Diligent made.
  std::string error;
  auto adopted = [&error, &native = owned->native()] {
    auto made = GpuDevice::adopt(native);
    if (!made) error = made.error().message;
    return std::move(made).value_or(nullptr);
  }();
  ASSERT_NE(adopted, nullptr) << error;
  EXPECT_EQ(adopted->native().vulkan.device, owned->native().vulkan.device);
  TextureDescription desc = smallTexture();
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

TEST(AdoptedDevice, TextureFormatsMapAndRetire) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  GpuDevice* device = on->gpu();
  if (!device) GTEST_SKIP() << "the device was created but not adopted";
  // Every case in this binary stands on the one device, so what a destroy
  // adds to the retirement queue is a DELTA — an absolute count would be
  // reading whatever ran before.
  const size_t pendingBefore = device->pendingDestroys();
  const TextureFormat formats[] = {TextureFormat::RGBA8Unorm,
                                   TextureFormat::BGRA8Unorm,
                                   TextureFormat::RGBA16Float};
  const uint32_t expected[] = {37 /*R8G8B8A8_UNORM*/, 44 /*B8G8R8A8_UNORM*/,
                               97 /*R16G16B16A16_SFLOAT*/};
  for (int i = 0; i < 3; ++i) {
    TextureDescription desc = smallTexture();
    desc.format = formats[i];
    const TextureHandle texture = device->createTexture(desc);
    ASSERT_TRUE(device->isValid(texture)) << "format " << i;
    const NativeTexture native = device->exportNative(texture);
    EXPECT_EQ(native.vkFormat, expected[i]);
    EXPECT_EQ(native.width, 8);
    EXPECT_EQ(native.mipLevels, 1);
    EXPECT_EQ(native.vkLayout, 0u) << "undefined until drawn";
    device->destroy(texture);
    EXPECT_FALSE(device->isValid(texture));
  }
  EXPECT_EQ(device->pendingDestroys(), pendingBefore + 3u);
  for (int i = 0; i < 3; ++i) device->beginFrame();
  EXPECT_EQ(device->pendingDestroys(), 0u);

  TextureDescription cpu = smallTexture();
  cpu.cpuAccessible = true;
  const TextureHandle hostVisible = device->createTexture(cpu);
  EXPECT_TRUE(device->isValid(hostVisible));
  device->destroy(hostVisible);

  // A prefiltered environment is one image per level, so a Vulkan image
  // has to be created with the whole chain rather than have one
  // generated from level 0.
  TextureDescription chained = smallTexture();
  chained.width = 256;
  chained.height = 128;
  chained.format = TextureFormat::RGBA16Float;
  chained.mipLevels = 9;
  const TextureHandle panorama = device->createTexture(chained);
  ASSERT_TRUE(device->isValid(panorama));
  EXPECT_EQ(device->exportNative(panorama).mipLevels, 9);
  device->destroy(panorama);
}

TEST(AdoptedDevice, ImportExportRoundTrip) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  GpuDevice* device = on->gpu();
  if (!device) GTEST_SKIP() << "the device was created but not adopted";
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
  EXPECT_FALSE(device->importNative(metal)) << "a Metal texture with none";
}

// THE METAL OBJECT BEHIND A TEXTURE NAMED HERE, on a driver that runs
// Vulkan over Metal: one texture is one object however often it is asked
// for, and a handle that names nothing any more names no object either.
TEST(AdoptedDevice, ATextureNamedHereIsReachedOnMetal) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  GpuDevice* device = on->gpu();
  if (!device) GTEST_SKIP() << "the device was created but not adopted";
  const TextureHandle texture = device->createTexture(smallTexture());
  ASSERT_TRUE(device->isValid(texture));
  void* metal = device->exportNative(texture).mtlTexture;
  if (!metal) {
    device->destroy(texture);
    GTEST_SKIP() << "this driver does not name the Metal objects behind its "
                    "own";
  }
  EXPECT_EQ(device->exportNative(texture).mtlTexture, metal);
  device->destroy(texture);
  EXPECT_EQ(device->exportNative(texture).mtlTexture, nullptr)
      << "a stale handle";
  EXPECT_EQ(device->exportNative(TextureHandle{}).mtlTexture, nullptr)
      << "a null handle";
}

// A TEXTURE METAL MADE, NAMED HERE OVER THE SAME MEMORY: the image the
// import names is backed by the very texture it was handed — asked for
// the Metal object behind it, the handle answers that texture — and it
// carries the size, format and levels the texture was made with. The
// image is the handle's and the texture its maker's, so destroying the
// one leaves the other standing.
TEST(AdoptedDevice, ATextureMetalMadeIsNamedHereOverTheSameMemory) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  GpuDevice* device = on->gpu();
  if (!device) GTEST_SKIP() << "the device was created but not adopted";
  std::string error;
  const std::unique_ptr<GpuDevice> metal = [&error] {
    auto made = GpuDevice::createOwned();
    if (!made) error = made.error().message;
    return std::move(made).value_or(nullptr);
  }();
  if (!metal || metal->backend() != Backend::Metal)
    GTEST_SKIP() << "no Metal device: " << error;

  TextureDescription desc;
  desc.width = 64;
  desc.height = 32;
  desc.format = TextureFormat::RGBA8Unorm;
  desc.mipLevels = 4;
  const TextureHandle made = metal->createTexture(desc);
  ASSERT_TRUE(metal->isValid(made));
  void* texture = metal->exportNative(made).mtlTexture;
  ASSERT_NE(texture, nullptr);

  NativeTexture given;
  given.backend = Backend::Metal;
  given.mtlTexture = texture;
  const TextureHandle imported = device->importNative(given);
  if (!imported) {
    metal->destroy(made);
    GTEST_SKIP() << "this driver does not import a Metal texture";
  }
  ASSERT_TRUE(device->isValid(imported));
  const NativeTexture native = device->exportNative(imported);
  EXPECT_EQ(native.backend, Backend::Vulkan);
  EXPECT_NE(native.vkImage, 0u);
  EXPECT_EQ(native.vkMemory, 0u) << "the texture is the memory";
  EXPECT_EQ(native.vkFormat, 37u /*R8G8B8A8_UNORM*/);
  EXPECT_EQ(native.width, 64);
  EXPECT_EQ(native.height, 32);
  EXPECT_EQ(native.mipLevels, 4);
  EXPECT_EQ(native.mtlTexture, texture) << "the very same texture";

  device->destroy(imported);
  for (int i = 0; i < 3; ++i) device->beginFrame();
  EXPECT_TRUE(metal->isValid(made));
  EXPECT_EQ(metal->exportNative(made).mtlTexture, texture);
  metal->destroy(made);

  given.mtlTexture = nullptr;
  EXPECT_FALSE(device->importNative(given)) << "no texture";
}

// A QUEUE ON METAL ORDERS THIS DEVICE'S QUEUE: a wait for an event that
// queue raises holds everything submitted here after it — a signal of
// this device's own fence among them — until the Metal queue has raised
// the event, and then lets it through. Nothing on this side raises the
// event, so the signal standing still is the wait and not a slow queue.
TEST(AdoptedDevice, AQueueOnMetalOrdersThisDevicesQueue) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  GpuDevice* device = on->gpu();
  if (!device) GTEST_SKIP() << "the device was created but not adopted";
  std::string error;
  const std::unique_ptr<GpuDevice> metal = [&error] {
    auto made = GpuDevice::createOwned();
    if (!made) error = made.error().message;
    return std::move(made).value_or(nullptr);
  }();
  if (!metal || metal->backend() != Backend::Metal)
    GTEST_SKIP() << "no Metal device: " << error;

  const FenceHandle painted = metal->createFence();
  const NativeFence event = metal->exportNative(painted);
  ASSERT_NE(event.mtlSharedEvent, nullptr);
  // The event is brought over once, and every wait after is on the one
  // handle.
  const FenceHandle brought = device->importNative(event);
  if (!brought) {
    metal->destroyFence(painted);
    GTEST_SKIP() << "this driver does not bring a Metal event over";
  }
  const FenceHandle after = device->createFence();
  FenceValue reached = kFenceInitialValue;
  {
    geometry::device::Device::QueueLock lock(*on);
    device->waitGpu(brought, 1);
    reached = device->signal(after);
  }
  EXPECT_EQ(device->waitCpu(after, reached, std::chrono::milliseconds(50)),
            FenceWait::TimedOut)
      << "the queue ran past a wait nothing had raised";

  EXPECT_EQ(metal->signal(painted), 1u);
  EXPECT_EQ(device->waitCpu(after, reached), FenceWait::Reached);

  // A second wait on the same handle, for a value already raised, holds
  // nothing.
  {
    geometry::device::Device::QueueLock lock(*on);
    device->waitGpu(brought, 1);
    reached = device->signal(after);
  }
  EXPECT_EQ(device->waitCpu(after, reached), FenceWait::Reached);
  EXPECT_FALSE(device->importNative(NativeFence{})) << "no event";

  device->destroyFence(after);
  device->destroyFence(brought);
  metal->destroyFence(painted);
}

TEST(AdoptedDevice, TimelineFenceSignalsAndWaits) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  GpuDevice* device = on->gpu();
  if (!device) GTEST_SKIP() << "the device was created but not adopted";
  const FenceHandle fence = device->createFence();
  ASSERT_TRUE(device->isValid(fence));
  EXPECT_EQ(device->completedValue(fence), kFenceInitialValue);
  EXPECT_TRUE(device->exportNative(fence));

  const FenceValue first = device->signal(fence);
  EXPECT_EQ(first, 1u);
  EXPECT_EQ(device->waitCpu(fence, first), FenceWait::Reached);
  EXPECT_GE(device->completedValue(fence), first);
  EXPECT_EQ(device->waitCpu(fence, first + 1, std::chrono::milliseconds(20)),
            FenceWait::TimedOut);

  // A wait on the queue for a value already reached holds nothing; the
  // signal queued after it is reached in turn.
  device->waitGpu(fence, first);
  const FenceValue second = device->signal(fence);
  EXPECT_EQ(second, first + 1);
  EXPECT_EQ(device->waitCpu(fence, second), FenceWait::Reached);

  device->destroyFence(fence);
  EXPECT_FALSE(device->isValid(fence));
  EXPECT_FALSE(device->exportNative(fence));
  EXPECT_EQ(device->waitCpu(fence, 1), FenceWait::Invalid);
}
