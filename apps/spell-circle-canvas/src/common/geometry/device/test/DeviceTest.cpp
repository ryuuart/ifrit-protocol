/** @file
 * The one device 2D and 3D share: Diligent creates the Vulkan device and
 * SigilCoreHardware adopts it, so Graphite draws on the very queue
 * Diligent submits through, the adopted device names every handle, and
 * Diligent still drives it afterwards.
 */

#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilgeometry/device/Device.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>

#include "DeviceFixtures.h"
#include "GraphiteReadback.h"
#include "OnDevice.h"

using namespace sigil;
using namespace sigil::geometry::device::test;

// ONE DEVICE, end to end. Diligent creates the Vulkan device and queue —
// it cannot attach to one that already exists — and SigilSkia adopts
// them, so there is a single device, a single queue and a single handle
// table under both APIs.
//
// The proof is the fence. It is a timeline semaphore signalled by an
// empty submission on the queue Diligent submits its own passes through,
// queued behind Graphite's drawing: reaching that value means Graphite's
// work landed on that queue. A second queue would leave the wait to time
// out, and the clear at the end shows Diligent still driving the same
// device after Graphite has submitted on it.
TEST(Device, GraphiteDrawsOnTheDeviceDiligentMade) {
  SIGIL_ON_DEVICE_OR_SKIP(d);

  core::hardware::GpuDevice* gpu = d->gpu();
  ASSERT_NE(gpu, nullptr) << "the Diligent device was not adopted";
  skia::GraphiteContext* graphite = vulkanGraphite(*d);
  ASSERT_NE(graphite, nullptr);
  EXPECT_EQ(gpu->backend(), core::hardware::Backend::Vulkan);
  EXPECT_NE(gpu->native().vulkan.device, nullptr);
  EXPECT_NE(gpu->native().vulkan.queue, nullptr);

  core::hardware::TextureDescription desc;
  desc.width = 8;
  desc.height = 8;
  desc.format = core::hardware::TextureFormat::RGBA8Unorm;
  const core::hardware::TextureHandle texture = gpu->createTexture(desc);
  ASSERT_TRUE(gpu->isValid(texture));
  const core::hardware::FenceHandle fence = gpu->createFence();
  ASSERT_TRUE(gpu->isValid(fence));

  const SkColor painted = SkColorSetARGB(255, 0, 0, 255);
  SkBitmap pixels;
  {
    // Everything that submits on the shared queue happens under the lock.
    geometry::device::Device::QueueLock lock(*d);
    skia::OffscreenSurface surface(*graphite, *gpu, texture);
    ASSERT_NE(surface.canvas(), nullptr);
    surface.canvas()->clear(painted);
    const core::hardware::FenceValue done = surface.submit(*gpu, fence);
    EXPECT_GT(done, core::hardware::kFenceInitialValue);
    EXPECT_EQ(gpu->waitCpu(fence, done), core::hardware::FenceWait::Reached);
    EXPECT_EQ(gpu->completedValue(fence), done);
    pixels = skia::test::readGraphiteSurface(*graphite, surface.surface());
  }
  ASSERT_FALSE(pixels.empty());
  EXPECT_EQ(pixels.getColor(0, 0), painted);
  EXPECT_EQ(pixels.getColor(7, 7), painted);

  // And Diligent still drives the same device and queue afterwards.
  EXPECT_TRUE(clearThroughDiligent(*d));

  gpu->destroyFence(fence);
  gpu->destroy(texture);
}
// The Diligent side stands on its own: a caller that only renders 3D
// never touches gpu() or graphite(), so a device whose adoption failed
// is still a device.
TEST(Device, DiligentSideStandsOnItsOwn) {
  SIGIL_ON_DEVICE_OR_SKIP(d);
  ASSERT_NE(d->renderDevice(), nullptr);
  ASSERT_NE(d->context(), nullptr);
  EXPECT_EQ(d->renderDevice()->GetDeviceInfo().Type,
            Diligent::RENDER_DEVICE_TYPE_VULKAN);
  EXPECT_TRUE(clearThroughDiligent(*d));
}
