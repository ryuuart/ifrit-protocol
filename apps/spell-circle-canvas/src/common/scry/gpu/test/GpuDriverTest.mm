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
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <memory>
#include <vector>

#include "GraphiteReadback.h"
#include "metal/MetalDriver.h"

using namespace sigil::scry;
using namespace sigil::scry::test;

namespace {

MetalDriver &sharedDriver() {
  // This binary is built only where a Metal device is guaranteed, so the
  // shared device and its Graphite context are never null here.
  // NOLINTBEGIN(clang-analyzer-core.NonNullParamChecker)
  static std::unique_ptr<MetalDriver> driver =
      MetalDriver::create(*sharedDevice(), *sharedGraphite());
  // NOLINTEND(clang-analyzer-core.NonNullParamChecker)
  EXPECT_NE(driver, nullptr);
  return *driver;
}

/** Draws the wrap of @p texture full-size into a fresh Graphite surface
 *  and reads back its centre. */
SkColor centreOf(sigil::core::hardware::TextureHandle texture, int width, int height) {
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
  const sigil::core::hardware::TextureHandle slot = driver.createImageTexture(32, 32);
  ASSERT_TRUE(slot);
  const std::vector<uint32_t> pixels = solid(32, 32, kOpaqueRedBgra);
  driver.uploadToTexture(slot, pixels.data(), 32, 32, (size_t)32 * 4);
  EXPECT_EQ(centreOf(slot, 32, 32), SK_ColorRED);
  driver.releaseTexture(slot);
}

TEST(ScryGpuDriver, PaintsASlotThroughTheWebRecorder) {
  MetalDriver &driver = sharedDriver();
  const sigil::core::hardware::TextureHandle slot = driver.createImageTexture(48, 24);
  ASSERT_TRUE(slot);
  ASSERT_TRUE(
      driver.paintTexture(slot, 48, 24, [](SkCanvas &canvas) { canvas.clear(SK_ColorGREEN); }));
  EXPECT_EQ(centreOf(slot, 48, 24), SK_ColorGREEN);
  driver.releaseTexture(slot);
}

TEST(ScryGpuDriver, CopiesBetweenDeviceTexturesClampedToTheSmaller) {
  MetalDriver &driver = sharedDriver();
  const sigil::core::hardware::TextureHandle src = driver.createImageTexture(64, 64);
  const sigil::core::hardware::TextureHandle dst = driver.createImageTexture(16, 16);
  const std::vector<uint32_t> pixels = solid(64, 64, kOpaqueGreenBgra);
  driver.uploadToTexture(src, pixels.data(), 64, 64, (size_t)64 * 4);
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
      std::memcpy(static_cast<char *>(dst) + (size_t)y * bitmap->row_bytes(),
                  pixels.data() + (size_t)y * 20, (size_t)20 * 4);
    bitmap->UnlockPixels();
  }
  const uint32_t id = driver.NextTextureId();
  driver.CreateTexture(id, bitmap);
  const sigil::core::hardware::TextureHandle publish = driver.createPublishTexture(20, 10);
  ASSERT_TRUE(publish);
  driver.copyTexture(id, publish, 20, 10);
  EXPECT_EQ(centreOf(publish, 20, 10), SK_ColorRED);
  driver.DestroyTexture(id);
  driver.releaseTexture(publish);
}

TEST(ScryGpuDriver, RegistersExternalTexturesUnderFreshIds) {
  MetalDriver &driver = sharedDriver();
  const sigil::core::hardware::TextureHandle slot = driver.createImageTexture(8, 8);
  const uint32_t a = driver.registerExternalTexture(slot);
  const uint32_t b = driver.NextTextureId();
  EXPECT_NE(a, 0u);
  EXPECT_NE(a, b);
  // Registered textures blit like Ultralight's own.
  const std::vector<uint32_t> pixels = solid(8, 8, kOpaqueGreenBgra);
  driver.uploadToTexture(slot, pixels.data(), 8, 8, (size_t)8 * 4);
  const sigil::core::hardware::TextureHandle publish = driver.createPublishTexture(8, 8);
  driver.copyTexture(a, publish, 8, 8);
  EXPECT_EQ(centreOf(publish, 8, 8), SK_ColorGREEN);
  driver.unregisterExternalTexture(a);
  driver.releaseTexture(publish);
  driver.releaseTexture(slot);
}

TEST(ScryGpuDriver, AReleasedHandleGoesStaleWhileItsWrapStillDraws) {
  MetalDriver &driver = sharedDriver();
  sigil::skia::GraphiteContext &graphite = *sharedGraphite();
  const sigil::core::hardware::TextureHandle slot = driver.createImageTexture(16, 16);
  const std::vector<uint32_t> pixels = solid(16, 16, kOpaqueRedBgra);
  driver.uploadToTexture(slot, pixels.data(), 16, 16, (size_t)16 * 4);
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
