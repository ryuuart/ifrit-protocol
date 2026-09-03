/** @file
 * scry_engine_gpu_test — the GPU-mode engine end to end: booted on a
 * Metal device, a page rendered through the Metal driver, the published
 * texture wrapped as a Graphite SkImage, composited into a Graphite
 * surface on the same device and queue, and the pixels read back.
 * A binary of its own because Ultralight allows one Renderer per process
 * and scry_engine_test owns the CPU-mode engine.
 */

#import <Metal/Metal.h>

#include <sigilscry/engine/WebEngine.h>
#include <sigilscry/engine/WebImage.h>
#include <sigilscry/engine/WebView.h>

#include <sigilcore/hardware/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkImage.h>
#include <include/core/SkRect.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/BackendTexture.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <include/gpu/graphite/mtl/MtlGraphiteTypes_cpp.h>

#include <gtest/gtest.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "GraphiteReadback.h"
#include "Wait.h"

using namespace sigil::scry;
using namespace sigil::scry::test;

namespace {

WebEngine &sharedEngine() {
  static std::shared_ptr<WebEngine> engine = [] {
    WebEngineConfig config;
    config.gpuDevice = sharedDevice();
    config.graphite = sharedGraphite();
    return WebEngine::create(config);
  }();
  EXPECT_NE(engine, nullptr);
  return *engine;
}

/** Snap the test recorder and insert + submit under the context's lock. */
void submit(sigil::skia::GraphiteContext &graphite) {
  auto recording = graphite.recorder()->snap();
  ASSERT_NE(recording, nullptr);
  const std::unique_lock<std::mutex> lock = graphite.lockContext();
  skgpu::graphite::InsertRecordingInfo info;
  info.fRecording = recording.get();
  graphite.context()->insertRecording(info);
  graphite.context()->submit();
}

// ── The ways a page-visible slot gets its pixels ──────────────────────

/** Graphite renders straight into the slot's own texture, on the test's
 *  own recorder: the handle named it, the device handed the object out. */
void fillByGraphiteRender(WebImage &image) {
  ASSERT_TRUE(image.texture());
  const sigil::core::hardware::NativeTexture slot = sharedDevice()->exportNative(image.texture());
  ASSERT_TRUE(slot);
  sigil::skia::GraphiteContext &graphite = *sharedGraphite();
  skgpu::graphite::BackendTexture backendTexture = skgpu::graphite::BackendTextures::MakeMetal(
      SkISize::Make(64, 64), (CFTypeRef)slot.mtlTexture);
  sk_sp<SkSurface> imageSurface = SkSurfaces::WrapBackendTexture(
      graphite.recorder(), backendTexture, SkColorSpace::MakeSRGB(), nullptr);
  ASSERT_NE(imageSurface, nullptr);
  imageSurface->getCanvas()->clear(SK_ColorMAGENTA);
  submit(graphite);
  image.invalidate();
}

/** CPU pixels uploaded into the slot's texture. */
void fillByRasterUpdate(WebImage &image) {
  SkBitmap swatch;
  ASSERT_TRUE(swatch.tryAllocPixels(SkImageInfo::MakeN32Premul(32, 32)));
  SkCanvas canvas(swatch);
  canvas.clear(SK_ColorCYAN);
  image.update(swatch.pixmap());
}

/** The one-call paint() API: the engine's own recorder wraps the slot
 *  texture, the callback draws, and flush + invalidate happen with it. */
void fillByPaintCallback(WebImage &image) {
  ASSERT_TRUE(image.paint([](SkCanvas &canvas) { canvas.clear(SK_ColorYELLOW); }));
}

/** A texture some other API produced, entering the engine by being named
 *  on the shared device — the path content rendered elsewhere takes. */
void fillByNativeTexture(WebImage &image) {
  MTLTextureDescriptor *desc =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                         width:16
                                                        height:16
                                                     mipmapped:NO];
  desc.storageMode = MTLStorageModeShared;
  id<MTLDevice> mtl = (__bridge id<MTLDevice>)sharedDevice()->native().mtlDevice;
  // The import is borrowed, so the object has to outlive the slot that
  // samples it.
  static id<MTLTexture> external = nil;
  external = [mtl newTextureWithDescriptor:desc];
  std::vector<uint32_t> pixels(static_cast<size_t>(16 * 16), 0xffff0000);  // opaque red, BGRA
  [external replaceRegion:MTLRegionMake2D(0, 0, 16, 16)
              mipmapLevel:0
                withBytes:pixels.data()
              bytesPerRow:static_cast<NSUInteger>(16 * 4)];

  sigil::core::hardware::NativeTexture native;
  native.backend = sigil::core::hardware::Backend::Metal;
  native.mtlTexture = (__bridge void *)external;
  native.width = 16;
  native.height = 16;
  const sigil::core::hardware::TextureHandle handle = sharedDevice()->importNative(native);
  ASSERT_TRUE(handle);
  ASSERT_TRUE(image.updateTexture(handle));
}

/** One way of filling a slot, and the colour the page must then show. */
struct SlotFilling {
  const char *slot;
  int size;
  SkColor expected;
  void (*fill)(WebImage &);
};

}  // namespace

TEST(WebViewGpuTest, RendersThroughMetalAndGraphite) {
  ASSERT_NE(sharedDevice(), nullptr);
  sigil::skia::GraphiteContext *graphite = sharedGraphite();
  ASSERT_NE(graphite, nullptr);

  auto view = sharedEngine().createView(64, 64, {.transparent = false});
  ASSERT_NE(view, nullptr);
  view->loadHTML("<html><body style='background:#ff0000;margin:0'>"
                 "</body></html>");

  sk_sp<SkSurface> surface =
      SkSurfaces::RenderTarget(graphite->recorder(), SkImageInfo::MakeN32Premul(128, 128));
  ASSERT_NE(surface, nullptr);

  // The page lands inside the rect it was given, so the canvas's own
  // colour still stands outside it.
  ASSERT_TRUE(waitForColour("the page's colour in the Graphite composite", SK_ColorRED, [&] {
    if (view->frameVersion() == 0) return SK_ColorTRANSPARENT;
    surface->getCanvas()->clear(SK_ColorGREEN);
    view->draw(*surface->getCanvas(), SkRect::MakeXYWH(32, 32, 64, 64));
    return readbackPixel(*graphite, surface.get(), 64, 64);
  }));
  EXPECT_EQ(readbackPixel(*graphite, surface.get(), 8, 8), SK_ColorGREEN);
}

// Skia -> Ultralight -> Skia without leaving the GPU, whichever door the
// pixels came through: the page samples the slot texture and the
// composited page comes back onto a Graphite canvas.
class SlotFillingTest : public ::testing::TestWithParam<SlotFilling> {};

TEST_P(SlotFillingTest, ThePageShowsWhatFilledTheSlot) {
  const SlotFilling &how = GetParam();
  auto image = sharedEngine().createImage(how.slot, how.size, how.size);
  ASSERT_NE(image, nullptr);
  how.fill(*image);

  auto view = sharedEngine().createView(64, 64, {.transparent = false});
  ASSERT_NE(view, nullptr);
  view->loadHTML(std::string("<html><body style='margin:0;background:#000'>"
                             "<img src='") +
                 how.slot +
                 ".imgsrc' style='display:block;width:64px;height:64px'>"
                 "</body></html>");

  sigil::skia::GraphiteContext *graphite = sharedGraphite();
  ASSERT_NE(graphite, nullptr);
  sk_sp<SkSurface> surface =
      SkSurfaces::RenderTarget(graphite->recorder(), SkImageInfo::MakeN32Premul(64, 64));
  ASSERT_NE(surface, nullptr);

  EXPECT_TRUE(waitForColour("the slot's colour in the composited page", how.expected, [&] {
    if (view->frameVersion() == 0) return SK_ColorTRANSPARENT;
    surface->getCanvas()->clear(SK_ColorBLACK);
    view->draw(*surface->getCanvas(), SkRect::MakeWH(64, 64));
    return readbackPixel(*graphite, surface.get(), 32, 32);
  }));
}

INSTANTIATE_TEST_SUITE_P(
    SlotDoors, SlotFillingTest,
    ::testing::Values(SlotFilling{"gpu_swatch", 64, SK_ColorMAGENTA, fillByGraphiteRender},
                      SlotFilling{"gpu_raster_swatch", 32, SK_ColorCYAN, fillByRasterUpdate},
                      SlotFilling{"gpu_paint_swatch", 32, SK_ColorYELLOW, fillByPaintCallback},
                      SlotFilling{"gpu_ext_texture", 16, SK_ColorRED, fillByNativeTexture}),
    [](const ::testing::TestParamInfo<SlotFilling> &info) { return std::string(info.param.slot); });

TEST(WebViewGpuTest, ASlotRefusesANullTextureHandle) {
  auto image = sharedEngine().createImage("gpu_null_handle", 8, 8);
  ASSERT_NE(image, nullptr);
  EXPECT_FALSE(image->updateTexture(sigil::core::hardware::TextureHandle{}));
}

TEST(WebViewGpuTest, WrapsOnePublishedTextureOncePerVersion) {
  auto view = sharedEngine().createView(32, 32, {.transparent = false});
  ASSERT_NE(view, nullptr);
  view->loadHTML("<html><body style='background:#00ff00'></body></html>");
  ASSERT_TRUE(waitForFrame(*view, 0));

  sigil::skia::GraphiteContext *graphite = sharedGraphite();
  ASSERT_NE(graphite, nullptr);

  sk_sp<SkImage> image = view->frame(graphite->recorder()).image;
  ASSERT_NE(image, nullptr);
  EXPECT_TRUE(image->isTextureBacked());
  EXPECT_EQ(image->width(), 32);
  EXPECT_EQ(image->height(), 32);

  // Wraps are cached per version: acquiring the same frame again returns
  // the same SkImage identity, so Skia-side caches stay warm.
  sk_sp<SkImage> again = view->frame(graphite->recorder()).image;
  EXPECT_EQ(again.get(), image.get());

  // Without a recorder there is no wrap, but the metadata still flows.
  WebView::Frame bare = view->frame();
  EXPECT_EQ(bare.image, nullptr);
  EXPECT_TRUE(bare.texture);
  EXPECT_TRUE(sharedDevice()->isValid(bare.texture));
  EXPECT_TRUE(sharedDevice()->exportNative(bare.texture));
  EXPECT_EQ(bare.width, 32);
  EXPECT_EQ(bare.height, 32);
  EXPECT_TRUE(static_cast<bool>(bare));
}

// A page brought up and torn down while the engine renders on the
// device. A view's teardown crosses to the web thread and the publish
// pass runs there too, so an engine that publishes through a page it has
// already torn down faults here rather than once in a few hundred runs.
TEST(WebViewGpuTest, PagesComeAndGoUnderTheRenderLoop) {
  for (int round = 0; round < 12; ++round) {
    auto view = sharedEngine().createView(64, 64, {.transparent = false});
    ASSERT_NE(view, nullptr) << "round " << round;
    view->loadHTML("<html><body style='background:#0000ff;margin:0'></body></html>");
    EXPECT_TRUE(waitForFrame(*view, 0)) << "round " << round;
  }
}
