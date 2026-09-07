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
#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilgeometry/device/Device.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>
#include <sigilskia/graphite/PaintOrder.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

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
  ASSERT_NE(d->graphite(), nullptr);
  EXPECT_EQ(gpu->backend(), core::hardware::Backend::Vulkan);
  EXPECT_NE(gpu->native().vulkan.device, nullptr);
  EXPECT_NE(gpu->native().vulkan.queue, nullptr);

  core::hardware::TextureDesc desc;
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
    skia::OffscreenSurface surface(*d->graphite(), *gpu, texture);
    ASSERT_NE(surface.canvas(), nullptr);
    surface.canvas()->clear(painted);
    const core::hardware::FenceValue done = surface.submit(*gpu, fence);
    EXPECT_GT(done, core::hardware::kFenceInitialValue);
    EXPECT_EQ(gpu->waitCpu(fence, done), core::hardware::FenceWait::Reached);
    EXPECT_EQ(gpu->completedValue(fence), done);
    pixels = skia::test::readGraphiteSurface(*d->graphite(), surface.surface());
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
using core::hardware::NativeTexture;
using core::hardware::TextureDesc;
using core::hardware::TextureFormat;
using core::hardware::TextureHandle;
using core::hardware::VulkanHandles;

TextureDesc smallTexture() {
  TextureDesc desc;
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
    TextureDesc desc = smallTexture();
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

  TextureDesc cpu = smallTexture();
  cpu.cpuAccessible = true;
  const TextureHandle hostVisible = device->createTexture(cpu);
  EXPECT_TRUE(device->isValid(hostVisible));
  device->destroy(hostVisible);

  // A prefiltered environment is one image per level, so a Vulkan image
  // has to be created with the whole chain rather than have one
  // generated from level 0.
  TextureDesc chained = smallTexture();
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
  metal.mtlTexture = &metal;
  EXPECT_FALSE(device->importNative(metal)) << "another API's texture";
}

TEST(AdoptedDevice, TimelineFenceSignalsAndWaits) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  GpuDevice* device = on->gpu();
  if (!device) GTEST_SKIP() << "the device was created but not adopted";
  const FenceHandle fence = device->createFence();
  ASSERT_TRUE(device->isValid(fence));
  EXPECT_EQ(device->completedValue(fence), kFenceInitialValue);
  EXPECT_NE(device->exportNative(fence), nullptr);

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
  EXPECT_EQ(device->exportNative(fence), nullptr);
  EXPECT_EQ(device->waitCpu(fence, 1), FenceWait::Invalid);
}

TEST(AdoptedGraphite, WrapsATextureNamedByHandle) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  GpuDevice* dev = on->gpu();
  if (!dev) GTEST_SKIP() << "the device was created but not adopted";
  skia::GraphiteContext* ctx = on->graphite();
  if (!ctx) GTEST_SKIP() << "this Skia carries no Vulkan backend";

  TextureDesc desc = smallTexture();
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
  skia::GraphiteContext* ctx = on->graphite();
  if (!ctx) GTEST_SKIP() << "this Skia carries no Vulkan backend";

  TextureDesc desc = smallTexture();
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
  skia::GraphiteContext* ctx = on->graphite();
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
  skia::GraphiteContext* ctx = on->graphite();
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

// A pass the fence cut in two must keep what the pass before it drew.
// Graphite antialiases a large path by rendering it multisampled and
// resolving the samples onto the target; a backend that cannot bring the
// target's pixels back into the multisample attachment starts the second
// pass from undefined samples and resolves them over the first pass's
// work, so the ground and everything on it comes back as garbage. The
// scene here is the smallest one that asks for both: a path big enough to
// take the multisample renderer on either side of a draw whose blend the
// hardware cannot express, which is what ends the pass between them.
//
// Antialiased edges are the one thing two rasterisers are allowed to
// disagree about, so the claim is made where neither is guessing: every
// pixel the CPU painted one of the scene's flat colours, the device
// painted the same colour.
TEST(AdoptedGraphite, AFencedPassKeepsWhatThePassBeforeItDrew) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  skia::GraphiteContext* ctx = on->graphite();
  if (!ctx) GTEST_SKIP() << "this Skia carries no Vulkan backend";

  // A star: concave, many-sided and large, which is the shape Graphite
  // hands to its multisample renderer rather than to an analytic one.
  const auto star = [](float cx, float cy, float outer, float inner) {
    SkPathBuilder path;
    for (int i = 0; i < 10; ++i) {
      const float angle = (float)i * 3.14159265f / 5.0f - 1.5707963f;
      const float r = (i % 2 == 0) ? outer : inner;
      const float x = cx + r * std::cos(angle);
      const float y = cy + r * std::sin(angle);
      if (i == 0)
        path.moveTo(x, y);
      else
        path.lineTo(x, y);
    }
    path.close();
    return path.detach();
  };

  const auto describe = [&star](SkCanvas& c) {
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
  EXPECT_EQ(ordered.fences(), skia::PaintOrderCanvas::needed(*ctx) ? 1 : 0);

  const SkColor flat[] = {SK_ColorWHITE, SK_ColorRED, SK_ColorBLUE,
                          SkColorSetARGB(255, 128, 128, 128)};
  // A flat colour is flat on both rasterisers, but a pixel one of them
  // still resolved through a coverage value can come back a code value or
  // two off it. The claim is that the pixel is the colour it was painted,
  // not that two rasterisers agree bit for bit, so a couple of code values
  // are allowed and a lost pass — a whole colour away — is not.
  const auto worstChannel = [](SkColor a, SkColor b) {
    const auto gap = [](uint32_t l, uint32_t r) {
      return (int)(l > r ? l - r : r - l);
    };
    return std::max({gap(SkColorGetR(a), SkColorGetR(b)),
                     gap(SkColorGetG(a), SkColorGetG(b)),
                     gap(SkColorGetB(a), SkColorGetB(b)),
                     gap(SkColorGetA(a), SkColorGetA(b))});
  };
  int judged = 0;
  for (int y = 0; y < info.height(); ++y) {
    for (int x = 0; x < info.width(); ++x) {
      const SkColor want = cpu.getColor(x, y);
      bool isFlat = false;
      for (const SkColor c : flat) isFlat = isFlat || c == want;
      if (!isFlat) continue;
      ++judged;
      ASSERT_LE(worstChannel(want, device.getColor(x, y)), 4) << x << "," << y;
    }
  }
  // The whole ground, both stars and the multiplied corner: a scene this
  // size has no way to be judged on a handful of pixels.
  EXPECT_GT(judged, 300000);
}
