/** @file
 * Graphite on the adopted device's Vulkan queue: a texture the device names
 * by handle wrapped as a surface, a submit that signals a device fence, and
 * a Graphite-owned target cleared and read back. Every case skips, naming
 * why, where the device was not adopted or this Skia carries no Vulkan
 * backend.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilgeometry/device/Device.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>

#include "DeviceFixtures.h"
#include "GraphiteReadback.h"
#include "OnDevice.h"

using namespace sigil;
using namespace sigil::geometry::device::test;

namespace {

using core::hardware::FenceHandle;
using core::hardware::FenceValue;
using core::hardware::FenceWait;
using core::hardware::GpuDevice;
using core::hardware::kFenceInitialValue;
using core::hardware::TextureDescription;
using core::hardware::TextureHandle;

}  // namespace

TEST(AdoptedGraphite, WrapsATextureNamedByHandle) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  GpuDevice* dev = on->gpu();
  if (!dev) GTEST_SKIP() << "the device was created but not adopted";
  skia::GraphiteContext* ctx = vulkanGraphite(*on);
  if (!ctx) GTEST_SKIP() << "this Skia carries no Vulkan backend";

  TextureDescription desc = smallTexture();
  // Host-visible memory is not what a render target wants on this path;
  // the pixels come back through Skia rather than a map.
  desc.cpuAccessible = false;
  const TextureHandle handle = dev->createTexture(desc);
  ASSERT_TRUE(dev->isValid(handle));
  skia::OffscreenSurface surface(*ctx, *dev, handle);
  ASSERT_NE(surface.canvas(), nullptr);
  surface.canvas()->clear(SkColorSetARGB(255, 0, 255, 0));

  const SkBitmap pixels =
      skia::test::readGraphiteSurface(*ctx, surface.surface());
  ASSERT_FALSE(pixels.empty());
  EXPECT_EQ(pixels.getColor(0, 0), SkColorSetARGB(255, 0, 255, 0));
  EXPECT_EQ(pixels.getColor(7, 7), SkColorSetARGB(255, 0, 255, 0));
  dev->destroy(handle);
}

TEST(AdoptedGraphite, SubmitSignalsAFence) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  GpuDevice* dev = on->gpu();
  if (!dev) GTEST_SKIP() << "the device was created but not adopted";
  skia::GraphiteContext* ctx = vulkanGraphite(*on);
  if (!ctx) GTEST_SKIP() << "this Skia carries no Vulkan backend";

  TextureDescription desc = smallTexture();
  desc.cpuAccessible = false;
  const TextureHandle handle = dev->createTexture(desc);
  const FenceHandle fence = dev->createFence();
  skia::OffscreenSurface surface(*ctx, *dev, handle);
  ASSERT_NE(surface.canvas(), nullptr);
  surface.canvas()->clear(SkColorSetARGB(255, 0, 0, 255));

  const FenceValue value = surface.submit(*dev, fence);
  EXPECT_GT(value, kFenceInitialValue);
  EXPECT_EQ(dev->waitCpu(fence, value), FenceWait::Reached);

  dev->destroyFence(fence);
  dev->destroy(handle);
}

TEST(AdoptedGraphite, RenderTargetClearsAndReadsBack) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  GpuDevice* dev = on->gpu();
  if (!dev) GTEST_SKIP() << "the device was created but not adopted";
  skia::GraphiteContext* ctx = vulkanGraphite(*on);
  if (!ctx) GTEST_SKIP() << "this Skia carries no Vulkan backend";

  // A Graphite-owned target: the context alone, no wrap. The surface
  // lives inside the context's lifetime — its memory is freed through the
  // context — so it is scoped to go first.
  const SkImageInfo info = SkImageInfo::MakeN32Premul(8, 8);
  sk_sp<SkSurface> target = SkSurfaces::RenderTarget(ctx->recorder(), info);
  ASSERT_NE(target, nullptr);
  target->getCanvas()->clear(SkColorSetARGB(255, 0, 255, 0));
  const SkBitmap pixels = skia::test::readGraphiteSurface(*ctx, target.get());
  ASSERT_FALSE(pixels.empty());
  EXPECT_EQ(pixels.getColor(3, 3), SkColorSetARGB(255, 0, 255, 0));
}
