/** @file
 * On Apple the device's 2D paints on Metal, over textures the Vulkan device
 * made: the painting lands in those textures, the Vulkan queue waits for
 * the painting submitted before it, the next painting waits for the Vulkan
 * queue, and a host's own Metal queue can paint in place of the device's.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilgeometry/device/Device.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>
#ifdef __APPLE__
#include <sigilskia/graphite/TextureImage.h>
#endif

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "DeviceFixtures.h"
#include "GraphiteReadback.h"
#include "OnDevice.h"

using namespace sigil;
using namespace sigil::geometry::device::test;

#ifdef __APPLE__
// ON APPLE 2D PAINTS ON METAL, over the Vulkan device's own textures: the
// device's Graphite records on the Metal device the Vulkan driver runs
// over, paints the Metal texture behind an image the Vulkan device made,
// and the two queues wait for each other on the GPU in both directions —
// the Vulkan queue for the painting, and the next painting for the Vulkan
// queue — with neither wait left standing.
TEST(Device, OnAppleTwoDimensionsPaintOnMetalOverTheDevicesTextures) {
  SIGIL_ON_DEVICE_OR_SKIP(d);
  core::hardware::GpuDevice* gpu = d->gpu();
  skia::GraphiteContext* graphite = d->graphite();
  if (!gpu || !graphite) GTEST_SKIP() << "2D on the 3D device is unavailable";
  ASSERT_NE(graphite->recorder(), nullptr);
  EXPECT_EQ(graphite->recorder()->backend(), skgpu::BackendApi::kMetal);
  EXPECT_EQ(skia::deviceOf(*graphite->recorder()), gpu->native().mtlDevice);

  core::hardware::TextureDescription desc;
  desc.width = desc.height = 16;
  desc.format = core::hardware::TextureFormat::RGBA8Unorm;
  desc.mipLevels = core::hardware::mipLevelsFor(16, 16);
  const core::hardware::TextureHandle texture = gpu->createTexture(desc);
  ASSERT_TRUE(gpu->isValid(texture));
  const SkColor painted = SkColorSetARGB(255, 200, 40, 0);
  {
    skia::OffscreenSurface surface(*graphite, *gpu, texture);
    ASSERT_NE(surface.canvas(), nullptr);
    surface.canvas()->clear(painted);
    surface.submit();
  }
  // The Vulkan queue waits for the painting, and work queued behind the
  // wait still finishes: the painting was submitted before it.
  d->waitForPainting();
  EXPECT_TRUE(clearThroughDiligent(*d));
  // …and the next painting waits for the Vulkan queue, which has finished
  // what it was handed, so the read behind that wait comes back.
  d->holdPaintingUntilSampled();
  SkBitmap pixels;
  {
    skia::OffscreenSurface surface(*graphite, *gpu, texture);
    ASSERT_NE(surface.surface(), nullptr);
    pixels = skia::test::readGraphiteSurface(*graphite, surface.surface());
  }
  ASSERT_FALSE(pixels.empty());
  EXPECT_EQ(pixels.getColor(0, 0), painted);
  EXPECT_EQ(pixels.getColor(15, 15), painted);
  gpu->destroy(texture);
}

// THE DEVICE'S QUEUE WAITS FOR THE PAINTING SUBMITTED BEFORE IT: with the
// painting queue held shut behind a gate — an event on a Metal queue of
// the case's own — work the Vulkan queue takes after `waitForPainting()`
// does not finish, and once the gate opens and the painting finishes, it
// does.
TEST(Device, OnAppleTheDevicesQueueWaitsForThePaintingSubmittedBeforeIt) {
  SIGIL_ON_DEVICE_OR_SKIP(d);
  core::hardware::GpuDevice* gpu = d->gpu();
  skia::GraphiteContext* graphite = d->graphite();
  if (!gpu || !graphite) GTEST_SKIP() << "2D on the 3D device is unavailable";
  std::string error;
  const std::unique_ptr<core::hardware::GpuDevice> gate =
      core::hardware::GpuDevice::createOwned(&error);
  if (!gate) GTEST_SKIP() << error;
  const core::hardware::FenceHandle shut = gate->createFence();
  ASSERT_TRUE(gate->isValid(shut));

  core::hardware::TextureDescription desc;
  desc.width = desc.height = 16;
  const core::hardware::TextureHandle texture = gpu->createTexture(desc);
  ASSERT_TRUE(gpu->isValid(texture));
  // The painting waits for the gate on its own queue.
  skia::waitBeforeSampling(*graphite->recorder(),
                           gate->exportNative(shut).mtlSharedEvent, 1);
  {
    skia::OffscreenSurface surface(*graphite, *gpu, texture);
    ASSERT_NE(surface.canvas(), nullptr);
    surface.canvas()->clear(SK_ColorBLUE);
    surface.submit();
  }
  d->waitForPainting();
  const core::hardware::FenceHandle behind = gpu->createFence();
  core::hardware::FenceValue queued = core::hardware::kFenceInitialValue;
  {
    const geometry::device::Device::QueueLock lock(*d);
    queued = gpu->signal(behind);
  }
  EXPECT_EQ(gpu->waitCpu(behind, queued, std::chrono::milliseconds(100)),
            core::hardware::FenceWait::TimedOut)
      << "the device's queue ran before the painting finished";
  // The gate is raised on the case's own queue, which nothing holds.
  (void)gate->signal(shut);
  EXPECT_EQ(gpu->waitCpu(behind, queued), core::hardware::FenceWait::Reached);
  gpu->destroyFence(behind);
  gpu->destroy(texture);
}

// …AND THE NEXT PAINTING WAITS FOR THE DEVICE'S QUEUE: with the Vulkan
// queue held shut behind a gate, a painting submitted after
// `holdPaintingUntilSampled()` does not finish, and once the gate opens
// and the queue drains, it does — with the pixels it painted.
TEST(Device, OnAppleTheNextPaintingWaitsForTheDevicesQueue) {
  SIGIL_ON_DEVICE_OR_SKIP(d);
  core::hardware::GpuDevice* gpu = d->gpu();
  skia::GraphiteContext* graphite = d->graphite();
  if (!gpu || !graphite) GTEST_SKIP() << "2D on the 3D device is unavailable";
  std::string error;
  const std::unique_ptr<core::hardware::GpuDevice> gate =
      core::hardware::GpuDevice::createOwned(&error);
  if (!gate) GTEST_SKIP() << error;
  const core::hardware::FenceHandle shut = gate->createFence();
  const core::hardware::FenceHandle brought =
      gpu->importNative(gate->exportNative(shut));
  if (!brought) GTEST_SKIP() << "this driver does not bring a Metal event over";

  core::hardware::TextureDescription desc;
  desc.width = desc.height = 16;
  const core::hardware::TextureHandle texture = gpu->createTexture(desc);
  ASSERT_TRUE(gpu->isValid(texture));
  // The device's queue waits for the gate, and the next painting for the
  // device's queue.
  {
    const geometry::device::Device::QueueLock lock(*d);
    gpu->waitGpu(brought, 1);
  }
  d->holdPaintingUntilSampled();

  // The painting is read back on a thread of its own, which blocks until
  // the painting finishes; nothing else touches the context meanwhile.
  const SkColor painted = SkColorSetARGB(255, 20, 160, 90);
  std::atomic<bool> finished{false};
  SkBitmap pixels;
  std::thread painter([&] {
    skia::OffscreenSurface surface(*graphite, *gpu, texture);
    if (surface.canvas()) {
      surface.canvas()->clear(painted);
      pixels = skia::test::readGraphiteSurface(*graphite, surface.surface());
    }
    finished.store(true);
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_FALSE(finished.load())
      << "the painting finished before the device's queue did";
  (void)gate->signal(shut);
  painter.join();
  EXPECT_TRUE(finished.load());
  ASSERT_FALSE(pixels.empty());
  EXPECT_EQ(pixels.getColor(8, 8), painted);
  // The device's queue has passed its wait, since the painting behind it
  // finished.
  gpu->destroyFence(brought);
  gpu->destroy(texture);
}

// A HOST'S OWN METAL QUEUE PAINTS IN PLACE OF THE DEVICE'S: a context the
// host made on the Metal device the Vulkan driver runs over is what
// `graphite()` answers once handed over, the Vulkan queue waits for what
// it paints exactly as for the device's own — held while the host's queue
// is held — and handing nothing back gives the device's own context
// again. Half a pair is refused with nothing changed.
TEST(Device, OnAppleAHostsOwnMetalQueuePaintsInPlaceOfTheDevicesOwn) {
  SIGIL_ON_DEVICE_OR_SKIP(d);
  core::hardware::GpuDevice* gpu = d->gpu();
  skia::GraphiteContext* own = d->graphite();
  if (!gpu || !own) GTEST_SKIP() << "2D on the 3D device is unavailable";
  std::string error;
  const std::unique_ptr<core::hardware::GpuDevice> window =
      core::hardware::GpuDevice::createOwned(&error);
  if (!window) GTEST_SKIP() << error;
  if (window->native().mtlDevice != gpu->native().mtlDevice)
    GTEST_SKIP() << "the system's Metal device is not the driver's";
  const std::unique_ptr<skia::GraphiteContext> windowGraphite =
      skia::GraphiteContext::create(*window);
  ASSERT_NE(windowGraphite, nullptr);
  const std::unique_ptr<core::hardware::GpuDevice> gate =
      core::hardware::GpuDevice::createOwned(&error);
  ASSERT_NE(gate, nullptr) << error;
  const core::hardware::FenceHandle shut = gate->createFence();

  EXPECT_FALSE(d->paintWith(window.get(), nullptr));
  EXPECT_EQ(d->graphite(), own);
  ASSERT_TRUE(d->paintWith(window.get(), windowGraphite.get()));
  EXPECT_EQ(d->graphite(), windowGraphite.get());

  core::hardware::TextureDescription desc;
  desc.width = desc.height = 16;
  const core::hardware::TextureHandle texture = gpu->createTexture(desc);
  ASSERT_TRUE(gpu->isValid(texture));
  skia::waitBeforeSampling(*windowGraphite->recorder(),
                           gate->exportNative(shut).mtlSharedEvent, 1);
  {
    skia::OffscreenSurface surface(*d->graphite(), *gpu, texture);
    ASSERT_NE(surface.canvas(), nullptr);
    surface.canvas()->clear(SK_ColorRED);
    surface.submit();
  }
  d->waitForPainting();
  const core::hardware::FenceHandle behind = gpu->createFence();
  core::hardware::FenceValue queued = core::hardware::kFenceInitialValue;
  {
    const geometry::device::Device::QueueLock lock(*d);
    queued = gpu->signal(behind);
  }
  EXPECT_EQ(gpu->waitCpu(behind, queued, std::chrono::milliseconds(100)),
            core::hardware::FenceWait::TimedOut)
      << "the device's queue ran before the host's painting finished";
  (void)gate->signal(shut);
  EXPECT_EQ(gpu->waitCpu(behind, queued), core::hardware::FenceWait::Reached);

  EXPECT_TRUE(d->paintWith(nullptr, nullptr));
  EXPECT_EQ(d->graphite(), own);
  gpu->destroyFence(behind);
  gpu->destroy(texture);
}
#endif
