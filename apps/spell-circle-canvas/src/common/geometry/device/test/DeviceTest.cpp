/** @file
 * The one device 2D and 3D share: Diligent creates the Vulkan device and
 * SigilCoreHardware adopts it, so Graphite draws on the very queue
 * Diligent submits through, the adopted device names every handle, and
 * Diligent still drives it afterwards.
 */

#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <gpu/graphite/Context.h>
#include <gpu/graphite/Recorder.h>
#include <gpu/graphite/Recording.h>
#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilgeometry/device/Device.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>
#include <sigilskia/graphite/PaintOrder.h>
#ifdef __APPLE__
#include <sigilskia/graphite/TextureImage.h>
#endif

#include <Common/interface/RefCntAutoPtr.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <thread>

#include "GraphiteReadback.h"
#include "OnDevice.h"

using namespace sigil;

namespace {

/** A clear of a fresh render target through Diligent, submitted and
 *  waited out. False when the target could not be made. */
bool clearThroughDiligent(geometry::device::Device& device) {
  using namespace Diligent;
  TextureDesc desc;
  desc.Name = "diligent still drives";
  desc.Type = RESOURCE_DIM_TEX_2D;
  desc.Width = 8;
  desc.Height = 8;
  desc.MipLevels = 1;
  desc.Format = TEX_FORMAT_RGBA8_UNORM;
  desc.BindFlags = BIND_RENDER_TARGET;
  RefCntAutoPtr<ITexture> target;
  device.renderDevice()->CreateTexture(desc, nullptr, &target);
  if (!target) return false;
  ITextureView* view = target->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
  if (!view) return false;
  IDeviceContext* context = device.context();
  ITextureView* views[] = {view};
  context->SetRenderTargets(1, views,
                            /*pDepthStencil=*/nullptr,
                            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  const float green[] = {0, 1, 0, 1};
  context->ClearRenderTarget(view, green,
                             RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  context->Flush();
  context->WaitForIdle();
  return true;
}

/** GRAPHITE ON THE VULKAN QUEUE ITSELF, which the cases here are about:
 *  the device's own `graphite()` where 2D shares that queue, and where it
 *  paints through Metal instead, a Vulkan context made over the adopted
 *  device and kept for the process — the context a host without Metal
 *  stands on. Null where the device was not adopted or this Skia carries
 *  no Vulkan backend. */
skia::GraphiteContext* vulkanGraphite(geometry::device::Device& device) {
  skia::GraphiteContext* own = device.graphite();
  if (own && own->recorder() &&
      own->recorder()->backend() == skgpu::BackendApi::kVulkan)
    return own;
  if (!device.gpu()) return nullptr;
  static const std::unique_ptr<skia::GraphiteContext> made =
      skia::GraphiteContext::create(*device.gpu());
  return made.get();
}

}  // namespace

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

// ---------------------------------------------------------------------------
// The device the whole process shares, adopted from the one Diligent made.
// A Vulkan device is never created here — Diligent owns the API and makes
// it, and the hardware device adopts what it made — so what these arms
// exercise is the adopted path and nothing else. Every one skips, naming
// why, on a machine without a Vulkan runtime (on macOS: brew install
// molten-vk vulkan-loader).

namespace {

using core::hardware::Backend;
using core::hardware::FenceHandle;
using core::hardware::FenceValue;
using core::hardware::FenceWait;
using core::hardware::GpuDevice;
using core::hardware::kFenceInitialValue;
using core::hardware::NativeFence;
using core::hardware::NativeTexture;
using core::hardware::TextureDescription;
using core::hardware::TextureFormat;
using core::hardware::TextureHandle;
using core::hardware::VulkanHandles;

TextureDescription smallTexture() {
  TextureDescription desc;
  desc.width = 8;
  desc.height = 8;
  desc.label = "geometry_device_test";
  return desc;
}

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
  auto adopted = GpuDevice::adopt(owned->native(), &error);
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
  const std::unique_ptr<GpuDevice> metal = GpuDevice::createOwned(&error);
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
  const std::unique_ptr<GpuDevice> metal = GpuDevice::createOwned(&error);
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

// A scene whose painting order the device must keep: an opaque ground, a
// fill that blends with what is under it, and an opaque box described
// after both. Graphite paints out of order and leans on the depth test to
// reject what the original order buried; where the destination read costs
// the depth attachment, the blending fill lands over the box that was
// described after it and the box comes back with a hole in it.
// PaintOrder.h states the whole of it. The claim here is the one a plate
// is judged on: the device picture is the CPU's, pixel for pixel.
TEST(AdoptedGraphite, PaintingOrderHoldsThroughADestinationRead) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  skia::GraphiteContext* ctx = vulkanGraphite(*on);
  if (!ctx) GTEST_SKIP() << "this Skia carries no Vulkan backend";

  const auto describe = [](SkCanvas& c) {
    c.clear(SK_ColorWHITE);
    SkPaint fill;
    fill.setColor(SK_ColorRED);
    c.drawRect(SkRect::MakeXYWH(0, 0, 100, 100), fill);
    fill.setColor(SK_ColorMAGENTA);
    c.drawRect(SkRect::MakeXYWH(20, 20, 60, 60), fill);
    // A blend the hardware cannot express, so the backend reads back what
    // it is blending with.
    SkPaint shade;
    shade.setColor(SkColorSetARGB(255, 128, 128, 128));
    shade.setBlendMode(SkBlendMode::kMultiply);
    c.drawRect(SkRect::MakeXYWH(0, 0, 100, 100), shade);
    fill.setColor(SK_ColorBLUE);
    c.drawRect(SkRect::MakeXYWH(40, 40, 20, 20), fill);
    // and the same again through a layer whose own paint is the blend
    SkPaint layer;
    layer.setBlendMode(SkBlendMode::kMultiply);
    c.saveLayer(nullptr, &layer);
    SkPaint pale;
    pale.setColor(SkColorSetARGB(255, 200, 200, 255));
    c.drawRect(SkRect::MakeXYWH(0, 60, 100, 40), pale);
    c.restore();
    fill.setColor(SK_ColorGREEN);
    c.drawRect(SkRect::MakeXYWH(5, 70, 20, 20), fill);
  };

  const SkImageInfo info = SkImageInfo::MakeN32Premul(100, 100);
  sk_sp<SkSurface> raster = SkSurfaces::Raster(info);
  ASSERT_NE(raster, nullptr);
  describe(*raster->getCanvas());
  SkBitmap cpu;
  cpu.allocPixels(info);
  ASSERT_TRUE(raster->readPixels(cpu, 0, 0));

  sk_sp<SkSurface> target = SkSurfaces::RenderTarget(ctx->recorder(), info);
  ASSERT_NE(target, nullptr);
  skia::PaintOrderCanvas ordered(*ctx, target->getCanvas());
  describe(ordered);
  const SkBitmap device = skia::test::readGraphiteSurface(*ctx, target.get());
  ASSERT_FALSE(device.empty());

  // The blend and the layer are the two reads, so a backend that needs
  // the fence closes the recording exactly twice.
  EXPECT_EQ(ordered.fences(), skia::PaintOrderCanvas::needed(*ctx) ? 2 : 0);
  for (int y = 0; y < info.height(); ++y)
    for (int x = 0; x < info.width(); ++x)
      ASSERT_EQ(cpu.getColor(x, y), device.getColor(x, y)) << x << "," << y;
}

namespace {

/** A star centred on (@p centerX, @p centerY): concave, many-sided and,
 *  at any size, a shape Graphite hands to its multisample renderer rather
 *  than to an analytic one. */
SkPath star(float centerX, float centerY, float outer, float inner) {
  SkPathBuilder path;
  for (int point = 0; point < 10; ++point) {
    const float angle = (float)point * 3.14159265f / 5.0f - 1.5707963f;
    const float radius = (point % 2 == 0) ? outer : inner;
    const float x = centerX + radius * std::cos(angle);
    const float y = centerY + radius * std::sin(angle);
    if (point == 0)
      path.moveTo(x, y);
    else
      path.lineTo(x, y);
  }
  path.close();
  return path.detach();
}

/** The CPU's picture of a scene, the reference a device picture is
 *  judged against. */
SkBitmap rasterise(const SkImageInfo& info,
                   const std::function<void(SkCanvas&)>& describe) {
  SkBitmap pixels;
  sk_sp<SkSurface> raster = SkSurfaces::Raster(info);
  if (!raster) return pixels;
  describe(*raster->getCanvas());
  pixels.allocPixels(info);
  if (!raster->readPixels(pixels, 0, 0)) return {};
  return pixels;
}

/** Whether every pixel @p cpu painted one of @p flat came back from the
 *  device the same colour, and how many pixels that judged.
 *
 *  Antialiased edges are the one thing two rasterisers are allowed to
 *  disagree about, so the claim is made where neither is guessing. A flat
 *  colour is flat on both, but a pixel one of them still resolved through
 *  a coverage value can come back a code value or two off it; the claim is
 *  that the pixel is the colour it was painted, not that two rasterisers
 *  agree bit for bit, so a couple of code values are allowed and a lost
 *  pass — a whole colour away — is not. */
::testing::AssertionResult keepsFlatColours(const SkBitmap& cpu,
                                            const SkBitmap& device,
                                            std::initializer_list<SkColor> flat,
                                            int& judged) {
  const auto worstChannel = [](SkColor first, SkColor second) {
    const auto gap = [](uint32_t left, uint32_t right) {
      return (int)(left > right ? left - right : right - left);
    };
    return std::max({gap(SkColorGetR(first), SkColorGetR(second)),
                     gap(SkColorGetG(first), SkColorGetG(second)),
                     gap(SkColorGetB(first), SkColorGetB(second)),
                     gap(SkColorGetA(first), SkColorGetA(second))});
  };
  judged = 0;
  if (cpu.empty() || device.empty() || cpu.dimensions() != device.dimensions())
    return ::testing::AssertionFailure() << "no picture to compare";
  for (int y = 0; y < cpu.height(); ++y) {
    for (int x = 0; x < cpu.width(); ++x) {
      const SkColor want = cpu.getColor(x, y);
      bool isFlat = false;
      for (const SkColor colour : flat) isFlat = isFlat || colour == want;
      if (!isFlat) continue;
      ++judged;
      const SkColor got = device.getColor(x, y);
      if (worstChannel(want, got) > 4)
        return ::testing::AssertionFailure()
               << "pixel " << x << "," << y << " was painted " << std::hex
               << want << " and came back " << got;
    }
  }
  return ::testing::AssertionSuccess();
}

}  // namespace

// A pass the fence cut in two must keep what the pass before it drew.
// Graphite antialiases a large path by rendering it multisampled and
// resolving the samples onto the target, and a pass that begins where
// another ended reloads the multisample attachment from the target first.
// A backend that resolves the whole attachment whatever area the pass
// names keeps the ground and whatever the first pass drew on it only if
// that reload reaches the whole attachment too. The scene here is the
// smallest one that asks for both: a path big enough to take the
// multisample renderer on either side of a draw whose blend the hardware
// cannot express, which is what ends the pass between them.
//
// Antialiased edges are the one thing two rasterisers are allowed to
// disagree about, so the claim is made where neither is guessing: every
// pixel the CPU painted one of the scene's flat colours, the device
// painted the same colour.
TEST(AdoptedGraphite, AFencedPassKeepsWhatThePassBeforeItDrew) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  skia::GraphiteContext* ctx = vulkanGraphite(*on);
  if (!ctx) GTEST_SKIP() << "this Skia carries no Vulkan backend";

  const auto describe = [](SkCanvas& c) {
    c.clear(SK_ColorWHITE);
    SkPaint fill;
    fill.setAntiAlias(true);
    fill.setColor(SK_ColorRED);
    c.drawPath(star(300, 200, 170, 70), fill);
    // The blend the hardware cannot express: over the white ground alone,
    // so what it leaves is a flat colour too.
    SkPaint shade;
    shade.setColor(SkColorSetARGB(255, 128, 128, 128));
    shade.setBlendMode(SkBlendMode::kMultiply);
    c.drawRect(SkRect::MakeXYWH(0, 0, 120, 60), shade);
    // Described after the read, so it belongs to the pass that begins
    // where the fence ended the one before.
    fill.setColor(SK_ColorBLUE);
    c.drawPath(star(300, 460, 130, 55), fill);
  };

  const SkImageInfo info = SkImageInfo::MakeN32Premul(600, 600);
  const SkBitmap cpu = rasterise(info, describe);
  ASSERT_FALSE(cpu.empty());

  sk_sp<SkSurface> target = SkSurfaces::RenderTarget(ctx->recorder(), info);
  ASSERT_NE(target, nullptr);
  skia::PaintOrderCanvas ordered(*ctx, target->getCanvas());
  describe(ordered);
  const SkBitmap device = skia::test::readGraphiteSurface(*ctx, target.get());
  ASSERT_FALSE(device.empty());
  EXPECT_EQ(ordered.fences(), skia::PaintOrderCanvas::needed(*ctx) ? 1 : 0);

  int judged = 0;
  EXPECT_TRUE(keepsFlatColours(cpu, device,
                               {SK_ColorWHITE, SK_ColorRED, SK_ColorBLUE,
                                SkColorSetARGB(255, 128, 128, 128)},
                               judged));
  // The whole ground, both stars and the multiplied corner: a scene this
  // size has no way to be judged on a handful of pixels.
  EXPECT_GT(judged, 300000);
}

// Every pass that continues a target keeps what the pass before it drew,
// whoever ended that pass. Here Graphite ends it itself, inside one
// recording, because a snapshot of the surface is taken while the surface
// is still being drawn; nothing in this repository sees that pass end, so
// the canvas is the surface's own and the pass continues the target. The
// pass that follows holds one small path, so the area its draws cover is a
// corner of the target and everything the first drawing left outside that
// corner has to come through the multisample attachment's reload.
TEST(AdoptedGraphite, ASnapshotTakenMidSceneKeepsWhatThePassBeforeItDrew) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  skia::GraphiteContext* ctx = vulkanGraphite(*on);
  if (!ctx) GTEST_SKIP() << "this Skia carries no Vulkan backend";

  const auto first = [](SkCanvas& c) {
    c.clear(SK_ColorWHITE);
    SkPaint fill;
    fill.setAntiAlias(true);
    fill.setColor(SK_ColorRED);
    c.drawPath(star(300, 250, 200, 80), fill);
  };
  const auto second = [](SkCanvas& c) {
    SkPaint fill;
    fill.setAntiAlias(true);
    fill.setColor(SK_ColorBLUE);
    c.drawPath(star(70, 530, 40, 16), fill);
  };

  const SkImageInfo info = SkImageInfo::MakeN32Premul(600, 600);
  const SkBitmap cpu = rasterise(info, [&](SkCanvas& c) {
    first(c);
    second(c);
  });
  ASSERT_FALSE(cpu.empty());

  sk_sp<SkSurface> target = SkSurfaces::RenderTarget(ctx->recorder(), info);
  ASSERT_NE(target, nullptr);
  first(*target->getCanvas());
  const sk_sp<SkImage> snapshot = target->makeImageSnapshot();
  ASSERT_NE(snapshot, nullptr);
  second(*target->getCanvas());
  const SkBitmap device = skia::test::readGraphiteSurface(*ctx, target.get());
  ASSERT_FALSE(device.empty());

  int judged = 0;
  EXPECT_TRUE(keepsFlatColours(
      cpu, device, {SK_ColorWHITE, SK_ColorRED, SK_ColorBLUE}, judged));
  EXPECT_GT(judged, 300000);
}

// The same promise across a recording a caller closes itself: the first
// drawing is snapped and inserted by the test rather than through this
// repository, and the recording after it begins with no clear, so the
// pass it opens continues the target and the pass continues the target.
TEST(AdoptedGraphite, ACallersOwnSnapKeepsWhatThePassBeforeItDrew) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  skia::GraphiteContext* ctx = vulkanGraphite(*on);
  if (!ctx) GTEST_SKIP() << "this Skia carries no Vulkan backend";

  const auto first = [](SkCanvas& c) {
    c.clear(SK_ColorWHITE);
    SkPaint fill;
    fill.setAntiAlias(true);
    fill.setColor(SK_ColorRED);
    c.drawPath(star(300, 250, 200, 80), fill);
  };
  const auto second = [](SkCanvas& c) {
    SkPaint fill;
    fill.setAntiAlias(true);
    fill.setColor(SK_ColorBLUE);
    c.drawPath(star(70, 530, 40, 16), fill);
  };

  const SkImageInfo info = SkImageInfo::MakeN32Premul(600, 600);
  const SkBitmap cpu = rasterise(info, [&](SkCanvas& c) {
    first(c);
    second(c);
  });
  ASSERT_FALSE(cpu.empty());

  sk_sp<SkSurface> target = SkSurfaces::RenderTarget(ctx->recorder(), info);
  ASSERT_NE(target, nullptr);
  first(*target->getCanvas());
  {
    // A snapped recording must be inserted: the recorder replays in
    // order, and one dropped recording stops it for good.
    std::unique_ptr<skgpu::graphite::Recording> recording =
        ctx->recorder()->snap();
    ASSERT_NE(recording, nullptr);
    const auto lock = ctx->lockContext();
    skgpu::graphite::InsertRecordingInfo insert;
    insert.fRecording = recording.get();
    ASSERT_TRUE(ctx->context()->insertRecording(insert));
  }
  second(*target->getCanvas());
  const SkBitmap device = skia::test::readGraphiteSurface(*ctx, target.get());
  ASSERT_FALSE(device.empty());

  int judged = 0;
  EXPECT_TRUE(keepsFlatColours(
      cpu, device, {SK_ColorWHITE, SK_ColorRED, SK_ColorBLUE}, judged));
  EXPECT_GT(judged, 300000);
}
