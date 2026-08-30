/** @file
 * scry_gpu_test — the Metal driver driven directly, with no renderer
 * and no page: a slot texture uploaded, painted through the web-thread
 * recorder and wrapped for another; an Ultralight-side texture created
 * from a bitmap and blitted into a publish texture; a copy between
 * device textures; a released handle gone stale while its wrap still
 * draws; and an empty flush. Every pixel is proved by reading it back
 * through Graphite on the same device and queue.
 */

#import <Metal/Metal.h>

#include <Ultralight/Bitmap.h>
#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkSurface.h>
#include <include/core/SkUnPreMultiply.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilskia/device/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "metal/MetalDriver.h"

using namespace sigil::scry;

namespace {

/** The device this process owns, shared by the driver and every test. */
sigil::skia::GpuDevice *sharedDevice() {
  static std::unique_ptr<sigil::skia::GpuDevice> device =
      sigil::skia::GpuDevice::createOwned(sigil::skia::Backend::Metal);
  return device.get();
}

/** The Graphite context the tests draw with and the driver shares: the
 *  driver's paint path records on its own recorder over it, so every
 *  context call here holds lockContext(). */
sigil::skia::GraphiteContext *sharedGraphite() {
  static std::unique_ptr<sigil::skia::GraphiteContext> graphite =
      sharedDevice() ? sigil::skia::GraphiteContext::create(*sharedDevice()) : nullptr;
  return graphite.get();
}

MetalDriver &sharedDriver() {
  static std::unique_ptr<MetalDriver> driver =
      MetalDriver::create(*sharedDevice(), *sharedGraphite());
  EXPECT_NE(driver, nullptr);
  return *driver;
}

/** Renders the Graphite surface's pending work and reads back the pixel
 *  at (x, y) as an unpremultiplied SkColor. */
SkColor readbackPixel(sigil::skia::GraphiteContext &graphite, SkSurface *surface, int x, int y) {
  std::unique_ptr<skgpu::graphite::Recording> recording = graphite.recorder()->snap();
  const std::unique_lock<std::mutex> lock = graphite.lockContext();
  if (recording) {
    skgpu::graphite::InsertRecordingInfo info;
    info.fRecording = recording.get();
    graphite.context()->insertRecording(info);
  }

  struct ReadContext {
    std::unique_ptr<const SkImage::AsyncReadResult> result;
    bool called = false;
  } readContext;

  graphite.context()->asyncRescaleAndReadPixels(
      surface, SkImageInfo::MakeN32Premul(1, 1), SkIRect::MakeXYWH(x, y, 1, 1),
      SkImage::RescaleGamma::kSrc, SkImage::RescaleMode::kNearest,
      [](SkImage::ReadPixelsContext context,
         std::unique_ptr<const SkImage::AsyncReadResult> result) {
        auto *read = static_cast<ReadContext *>(context);
        read->result = std::move(result);
        read->called = true;
      },
      &readContext);

  skgpu::graphite::SubmitInfo submitInfo;
  submitInfo.fSync = skgpu::graphite::SyncToCpu::kYes;
  graphite.context()->submit(submitInfo);

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!readContext.called && std::chrono::steady_clock::now() < deadline) {
    graphite.context()->checkAsyncWorkCompletion();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if (!readContext.result) return SK_ColorTRANSPARENT;

  const uint32_t *pixels = static_cast<const uint32_t *>(readContext.result->data(0));
  SkPMColor pm = pixels[0];
  return SkUnPreMultiply::PMColorToColor(pm);
}

/** Draws the wrap of @p texture full-size into a fresh Graphite surface
 *  and reads back its centre. */
SkColor centreOf(sigil::skia::TextureHandle texture, int width, int height) {
  sigil::skia::GraphiteContext &graphite = *sharedGraphite();
  sk_sp<SkImage> image = sharedDriver().wrapTexture(graphite.recorder(), texture, width, height);
  EXPECT_NE(image, nullptr);
  if (!image) return SK_ColorTRANSPARENT;
  sk_sp<SkSurface> surface =
      SkSurfaces::RenderTarget(graphite.recorder(), SkImageInfo::MakeN32Premul(width, height));
  surface->getCanvas()->clear(SK_ColorTRANSPARENT);
  surface->getCanvas()->drawImage(image, 0, 0);
  return readbackPixel(graphite, surface.get(), width / 2, height / 2);
}

/** Premultiplied BGRA pixels of one colour. */
std::vector<uint32_t> solid(int width, int height, uint32_t bgra) {
  return std::vector<uint32_t>((size_t)width * height, bgra);
}

// Premultiplied BGRA bytes in memory, read as little-endian words.
constexpr uint32_t kOpaqueRedBgra = 0xffff0000u;    // B=00 G=00 R=ff A=ff
constexpr uint32_t kOpaqueGreenBgra = 0xff00ff00u;  // B=00 G=ff R=00 A=ff

}  // namespace

TEST(ScryGpuDriver, UploadsAndWrapsASlotTexture) {
  ASSERT_NE(sharedDevice(), nullptr);
  ASSERT_NE(sharedGraphite(), nullptr);
  MetalDriver &driver = sharedDriver();
  const sigil::skia::TextureHandle slot = driver.createImageTexture(32, 32);
  ASSERT_TRUE(slot);
  const std::vector<uint32_t> pixels = solid(32, 32, kOpaqueRedBgra);
  driver.uploadToTexture(slot, pixels.data(), 32, 32, 32 * 4);
  EXPECT_EQ(centreOf(slot, 32, 32), SK_ColorRED);
  driver.releaseTexture(slot);
}

TEST(ScryGpuDriver, PaintsASlotThroughTheWebRecorder) {
  MetalDriver &driver = sharedDriver();
  const sigil::skia::TextureHandle slot = driver.createImageTexture(48, 24);
  ASSERT_TRUE(slot);
  ASSERT_TRUE(
      driver.paintTexture(slot, 48, 24, [](SkCanvas &canvas) { canvas.clear(SK_ColorGREEN); }));
  EXPECT_EQ(centreOf(slot, 48, 24), SK_ColorGREEN);
  driver.releaseTexture(slot);
}

TEST(ScryGpuDriver, CopiesBetweenDeviceTexturesClampedToTheSmaller) {
  MetalDriver &driver = sharedDriver();
  const sigil::skia::TextureHandle src = driver.createImageTexture(64, 64);
  const sigil::skia::TextureHandle dst = driver.createImageTexture(16, 16);
  const std::vector<uint32_t> pixels = solid(64, 64, kOpaqueGreenBgra);
  driver.uploadToTexture(src, pixels.data(), 64, 64, 64 * 4);
  EXPECT_TRUE(driver.copyDeviceTexture(src, dst, 64, 64));
  EXPECT_EQ(centreOf(dst, 16, 16), SK_ColorGREEN);
  // A stale handle refuses.
  driver.releaseTexture(src);
  EXPECT_FALSE(driver.copyDeviceTexture(src, dst, 16, 16));
  driver.releaseTexture(dst);
}

TEST(ScryGpuDriver, PublishesAnUltralightTextureByBlit) {
  MetalDriver &driver = sharedDriver();
  // What a page's render target looks like from the driver's side: a
  // texture Ultralight created from a bitmap, under an id it chose.
  ultralight::RefPtr<ultralight::Bitmap> bitmap =
      ultralight::Bitmap::Create(20, 10, ultralight::BitmapFormat::BGRA8_UNORM_SRGB);
  {
    void *dst = bitmap->LockPixels();
    const std::vector<uint32_t> pixels = solid(20, 10, kOpaqueRedBgra);
    for (uint32_t y = 0; y < 10; ++y)
      std::memcpy(static_cast<char *>(dst) + y * bitmap->row_bytes(), pixels.data() + y * 20,
                  20 * 4);
    bitmap->UnlockPixels();
  }
  const uint32_t id = driver.NextTextureId();
  driver.CreateTexture(id, bitmap);
  const sigil::skia::TextureHandle publish = driver.createPublishTexture(20, 10);
  ASSERT_TRUE(publish);
  driver.copyTexture(id, publish, 20, 10);
  EXPECT_EQ(centreOf(publish, 20, 10), SK_ColorRED);
  driver.DestroyTexture(id);
  driver.releaseTexture(publish);
}

TEST(ScryGpuDriver, RegistersExternalTexturesUnderFreshIds) {
  MetalDriver &driver = sharedDriver();
  const sigil::skia::TextureHandle slot = driver.createImageTexture(8, 8);
  const uint32_t a = driver.registerExternalTexture(slot);
  const uint32_t b = driver.NextTextureId();
  EXPECT_NE(a, 0u);
  EXPECT_NE(a, b);
  // Registered textures blit like Ultralight's own.
  const std::vector<uint32_t> pixels = solid(8, 8, kOpaqueGreenBgra);
  driver.uploadToTexture(slot, pixels.data(), 8, 8, 8 * 4);
  const sigil::skia::TextureHandle publish = driver.createPublishTexture(8, 8);
  driver.copyTexture(a, publish, 8, 8);
  EXPECT_EQ(centreOf(publish, 8, 8), SK_ColorGREEN);
  driver.unregisterExternalTexture(a);
  driver.releaseTexture(publish);
  driver.releaseTexture(slot);
}

TEST(ScryGpuDriver, AReleasedHandleGoesStaleWhileItsWrapStillDraws) {
  MetalDriver &driver = sharedDriver();
  sigil::skia::GraphiteContext &graphite = *sharedGraphite();
  const sigil::skia::TextureHandle slot = driver.createImageTexture(16, 16);
  const std::vector<uint32_t> pixels = solid(16, 16, kOpaqueRedBgra);
  driver.uploadToTexture(slot, pixels.data(), 16, 16, 16 * 4);
  sk_sp<SkImage> wrap = driver.wrapTexture(graphite.recorder(), slot, 16, 16);
  ASSERT_NE(wrap, nullptr);
  driver.releaseTexture(slot);
  EXPECT_FALSE(sharedDevice()->exportNative(slot)) << "the handle is stale";
  EXPECT_EQ(driver.wrapTexture(graphite.recorder(), slot, 16, 16), nullptr);
  // The wrap took its own reference: it still draws the texture.
  sk_sp<SkSurface> surface =
      SkSurfaces::RenderTarget(graphite.recorder(), SkImageInfo::MakeN32Premul(16, 16));
  surface->getCanvas()->drawImage(wrap, 0, 0);
  EXPECT_EQ(readbackPixel(graphite, surface.get(), 8, 8), SK_ColorRED);
}

TEST(ScryGpuDriver, AnEmptyFlushPublishesNothing) { EXPECT_TRUE(sharedDriver().flush().empty()); }
