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
#include <cstring>
#include <memory>
#include <string>

#include "sigilworld/diligent/Device.h"

using namespace sigil;

namespace {

/** Device bring-up needs a Vulkan runtime. The tests SKIP rather than
 *  fail when the machine has none, so a machine without a GPU stays
 *  green. */
// `d` names the variable the macro declares, and a declarator cannot be
// parenthesised; every caller passes a plain identifier.
// NOLINTBEGIN(bugprone-macro-parentheses)
#define MAKE_DEVICE_OR_SKIP(d)                                   \
  std::unique_ptr<world::diligent::Device> d;                    \
  {                                                              \
    world::diligent::DeviceConfig config;                        \
    std::string deviceError;                                     \
    d = world::diligent::Device::create(config, &deviceError);   \
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
bool clearThroughDiligent(world::diligent::Device& device) {
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
    world::diligent::Device::QueueLock lock(*d);
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
