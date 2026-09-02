#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <gpu/graphite/Context.h>
#include <gpu/graphite/Recorder.h>
#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>

#include <sigilgeometry/device/Device.h>

using namespace sigil;

namespace {

/** Device bring-up needs a Vulkan runtime. The tests SKIP rather than
 *  fail when the machine has none, so a machine without a GPU stays
 *  green. */
// `d` names the variable the macro declares, and a declarator cannot be
// parenthesised; every caller passes a plain identifier.
// NOLINTBEGIN(bugprone-macro-parentheses)
#define MAKE_DEVICE_OR_SKIP(d)                                   \
  std::unique_ptr<geometry::device::Device> d;                    \
  {                                                              \
    geometry::device::DeviceConfig config;                        \
    std::string deviceError;                                     \
    d = geometry::device::Device::create(config, &deviceError);   \
    if (!d) GTEST_SKIP() << "no Vulkan device: " << deviceError; \
  }
// NOLINTEND(bugprone-macro-parentheses)

/** The pixels of a Graphite surface, read back through the context that
 *  drew it: snap and insert whatever the recorder still holds, ask for
 *  the read, submit synchronously, then spin until the callback lands.
 *  Empty when the read never completed. */
SkBitmap readGraphiteSurface(skia::GraphiteContext& ctx, SkSurface* surface) {
  SkBitmap bitmap;
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
      surface, info, SkIRect::MakeWH(info.width(), info.height()),
      SkImage::RescaleGamma::kSrc, SkImage::RescaleMode::kNearest,
      [](SkImage::ReadPixelsContext c,
         std::unique_ptr<const SkImage::AsyncReadResult> r) {
        auto* out = static_cast<Read*>(c);
        out->result = std::move(r);
        out->called = true;
      },
      &read);
  skgpu::graphite::SubmitInfo submitInfo;
  submitInfo.fSync = skgpu::graphite::SyncToCpu::kYes;
  ctx.context()->submit(submitInfo);
  for (int spin = 0; spin < 5000 && !read.called; ++spin)
    ctx.context()->checkAsyncWorkCompletion();
  if (!read.result) return bitmap;
  bitmap.allocPixels(info);
  const auto* src = static_cast<const uint8_t*>(read.result->data(0));
  const size_t rowBytes = read.result->rowBytes(0);
  for (int y = 0; y < info.height(); ++y)
    std::memcpy(bitmap.pixmap().writable_addr(0, y), src + (size_t)y * rowBytes,
                (size_t)info.width() * 4);
  return bitmap;
}

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
  MAKE_DEVICE_OR_SKIP(d);

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
    pixels = readGraphiteSurface(*d->graphite(), surface.surface());
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
  MAKE_DEVICE_OR_SKIP(d);
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

/** The one device this process brings up, kept for its lifetime: making
 *  a second Vulkan device costs the driver more the more it has already
 *  made, and every arm here wants the same one anyway. */
geometry::device::Device *sharedDevice(std::string *why) {
  static std::string error;
  static std::unique_ptr<geometry::device::Device> device = [] {
    geometry::device::DeviceConfig config;
    return geometry::device::Device::create(config, &error);
  }();
  if (why) *why = error;
  return device.get();
}

/** …and the adopted device standing on it, null when the adoption
 *  failed. */
GpuDevice *adoptedDevice(std::string *why) {
  geometry::device::Device *made = sharedDevice(why);
  if (!made) return nullptr;
  if (!made->gpu() && why) *why = "the device was created but not adopted";
  return made->gpu();
}

/** …and Graphite on it. */
skia::GraphiteContext *adoptedGraphite() {
  geometry::device::Device *made = sharedDevice(nullptr);
  return made ? made->graphite() : nullptr;
}

TextureDesc smallTexture() {
  TextureDesc desc;
  desc.width = 8;
  desc.height = 8;
  desc.label = "geometry_device_test";
  return desc;
}

// `var` names the variable the macro declares, and a declarator cannot
// be parenthesised; every caller passes a plain identifier.
// NOLINTBEGIN(bugprone-macro-parentheses)
#define NEED_ADOPTED(var)                       \
  std::string adoptError;                       \
  GpuDevice *var = adoptedDevice(&adoptError);  \
  if (!(var)) GTEST_SKIP() << "no adopted device: " << adoptError
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace

TEST(AdoptedDevice, CarriesEveryVulkanHandle) {
  NEED_ADOPTED(device);
  EXPECT_EQ(device->backend(), Backend::Vulkan);
  const VulkanHandles &handles = device->native().vulkan;
  EXPECT_NE(handles.instance, nullptr);
  EXPECT_NE(handles.physicalDevice, nullptr);
  EXPECT_NE(handles.device, nullptr);
  EXPECT_NE(handles.queue, nullptr);
  EXPECT_NE(handles.getInstanceProcAddr, nullptr);
  EXPECT_NE(handles.apiVersion, 0u);
}

TEST(AdoptedDevice, AdoptsItsOwnHandlesAgain) {
  NEED_ADOPTED(owned);
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
  NEED_ADOPTED(device);
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
    EXPECT_EQ(native.mipLevels, 1);
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
  NEED_ADOPTED(device);
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
  NEED_ADOPTED(device);
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

TEST(AdoptedGraphite, WrapsATextureNamedByHandle) {
  NEED_ADOPTED(dev);
  skia::GraphiteContext *ctx = adoptedGraphite();
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

  const SkBitmap pixels = readGraphiteSurface(*ctx, surface.surface());
  ASSERT_FALSE(pixels.empty());
  EXPECT_EQ(pixels.getColor(0, 0), SkColorSetARGB(255, 0, 255, 0));
  EXPECT_EQ(pixels.getColor(7, 7), SkColorSetARGB(255, 0, 255, 0));
  dev->destroy(handle);
}

TEST(AdoptedGraphite, SubmitSignalsAFence) {
  NEED_ADOPTED(dev);
  skia::GraphiteContext *ctx = adoptedGraphite();
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
  NEED_ADOPTED(dev);
  skia::GraphiteContext *ctx = adoptedGraphite();
  if (!ctx) GTEST_SKIP() << "this Skia carries no Vulkan backend";

  // A Graphite-owned target: the context alone, no wrap. The surface
  // lives inside the context's lifetime — its memory is freed through the
  // context — so it is scoped to go first.
  const SkImageInfo info = SkImageInfo::MakeN32Premul(8, 8);
  sk_sp<SkSurface> target = SkSurfaces::RenderTarget(ctx->recorder(), info);
  ASSERT_NE(target, nullptr);
  target->getCanvas()->clear(SkColorSetARGB(255, 0, 255, 0));
  const SkBitmap pixels = readGraphiteSurface(*ctx, target.get());
  ASSERT_FALSE(pixels.empty());
  EXPECT_EQ(pixels.getColor(3, 3), SkColorSetARGB(255, 0, 255, 0));
}
