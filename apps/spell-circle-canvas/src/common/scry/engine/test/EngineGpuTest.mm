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

#include <sigilskia/device/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkImage.h>
#include <include/core/SkRect.h>
#include <include/core/SkSurface.h>
#include <include/core/SkUnPreMultiply.h>
#include <include/gpu/graphite/BackendTexture.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <include/gpu/graphite/mtl/MtlGraphiteTypes_cpp.h>

#include <gtest/gtest.h>

#include <chrono>
#include <mutex>
#include <thread>

using namespace sigil::scry;

namespace {

/** The device this process owns, shared by the engine and every test. */
sigil::skia::GpuDevice *sharedDevice() {
  static std::unique_ptr<sigil::skia::GpuDevice> device =
      sigil::skia::GpuDevice::createOwned(sigil::skia::Backend::Metal);
  return device.get();
}

/** The Graphite context the tests draw with and the engine shares: the
 *  web thread records on its own recorder over it, so every context call
 *  here holds lockContext(). */
sigil::skia::GraphiteContext *sharedGraphite() {
  static std::unique_ptr<sigil::skia::GraphiteContext> graphite =
      sharedDevice() ? sigil::skia::GraphiteContext::create(*sharedDevice()) : nullptr;
  return graphite.get();
}

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

}  // namespace

TEST(WebViewGpuTest, RendersThroughMetalAndGraphite) {
  ASSERT_NE(sharedDevice(), nullptr);

  auto view = sharedEngine().createView(64, 64, {.transparent = false});
  ASSERT_NE(view, nullptr);
  view->loadHTML("<html><body style='background:#ff0000;margin:0'>"
                 "</body></html>");

  sigil::skia::GraphiteContext *graphite = sharedGraphite();
  ASSERT_NE(graphite, nullptr);

  sk_sp<SkSurface> surface =
      SkSurfaces::RenderTarget(graphite->recorder(), SkImageInfo::MakeN32Premul(128, 128));
  ASSERT_NE(surface, nullptr);

  // The page may publish blank frames before the styled document paints;
  // keep compositing the latest GPU frame until the pixels turn red.
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
  SkColor center = 0;
  SkColor corner = 0;
  while (std::chrono::steady_clock::now() < deadline) {
    if (view->frameVersion() == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    WebView::Frame gpuFrame = view->frame();
    ASSERT_TRUE(gpuFrame.texture);
    ASSERT_TRUE(sharedDevice()->exportNative(gpuFrame.texture));
    ASSERT_EQ(gpuFrame.width, 64);
    ASSERT_EQ(gpuFrame.height, 64);
    EXPECT_EQ(gpuFrame.image, nullptr);  // no recorder given, no wrap

    surface->getCanvas()->clear(SK_ColorGREEN);
    view->draw(*surface->getCanvas(), SkRect::MakeXYWH(32, 32, 64, 64));

    center = readbackPixel(*graphite, surface.get(), 64, 64);
    corner = readbackPixel(*graphite, surface.get(), 8, 8);
    if (center == SK_ColorRED && corner == SK_ColorGREEN) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  EXPECT_EQ(center, SK_ColorRED);
  EXPECT_EQ(corner, SK_ColorGREEN);
}

// The reverse direction, all on the GPU: Graphite renders into the
// WebImage's MTLTexture, the page samples that texture through
// Ultralight's Metal pipeline, and the composited page comes back onto a
// Graphite canvas — Skia -> Ultralight -> Skia without leaving the GPU.
TEST(WebViewGpuTest, CompositesGraphiteContentIntoPage) {
  auto image = sharedEngine().createImage("gpu_swatch", 64, 64);
  ASSERT_NE(image, nullptr);
  ASSERT_TRUE(image->texture());
  const sigil::skia::NativeTexture slot = sharedDevice()->exportNative(image->texture());
  ASSERT_TRUE(slot);

  sigil::skia::GraphiteContext *graphite = sharedGraphite();
  ASSERT_NE(graphite, nullptr);

  // Render into the page-visible texture with Graphite, on the test's own
  // recorder — the handle named it, the device handed the object out.
  skgpu::graphite::BackendTexture backendTexture = skgpu::graphite::BackendTextures::MakeMetal(
      SkISize::Make(64, 64), (CFTypeRef)slot.mtlTexture);
  sk_sp<SkSurface> imageSurface = SkSurfaces::WrapBackendTexture(
      graphite->recorder(), backendTexture, SkColorSpace::MakeSRGB(), nullptr);
  ASSERT_NE(imageSurface, nullptr);
  imageSurface->getCanvas()->clear(SK_ColorMAGENTA);
  submit(*graphite);
  image->invalidate();

  auto view = sharedEngine().createView(64, 64, {.transparent = false});
  ASSERT_NE(view, nullptr);
  view->loadHTML("<html><body style='margin:0;background:#000'>"
                 "<img src='gpu_swatch.imgsrc' "
                 "style='display:block;width:64px;height:64px'>"
                 "</body></html>");

  sk_sp<SkSurface> surface =
      SkSurfaces::RenderTarget(graphite->recorder(), SkImageInfo::MakeN32Premul(64, 64));
  ASSERT_NE(surface, nullptr);

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
  SkColor center = 0;
  while (std::chrono::steady_clock::now() < deadline) {
    if (view->frameVersion() == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }
    surface->getCanvas()->clear(SK_ColorBLACK);
    view->draw(*surface->getCanvas(), SkRect::MakeWH(64, 64));
    center = readbackPixel(*graphite, surface.get(), 32, 32);
    if (center == SK_ColorMAGENTA) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  EXPECT_EQ(center, SK_ColorMAGENTA);
}

// Same direction via the raster path: update() uploads CPU pixels into
// the image texture on a GPU engine.
TEST(WebViewGpuTest, CompositesRasterUpdateIntoPage) {
  auto image = sharedEngine().createImage("gpu_raster_swatch", 32, 32);
  ASSERT_NE(image, nullptr);

  SkBitmap swatch;
  ASSERT_TRUE(swatch.tryAllocPixels(SkImageInfo::MakeN32Premul(32, 32)));
  SkCanvas swatchCanvas(swatch);
  swatchCanvas.clear(SK_ColorCYAN);
  image->update(swatch.pixmap());

  auto view = sharedEngine().createView(64, 64, {.transparent = false});
  ASSERT_NE(view, nullptr);
  view->loadHTML("<html><body style='margin:0;background:#000'>"
                 "<img src='gpu_raster_swatch.imgsrc' "
                 "style='display:block;width:64px;height:64px'>"
                 "</body></html>");

  sigil::skia::GraphiteContext *graphite = sharedGraphite();
  ASSERT_NE(graphite, nullptr);
  sk_sp<SkSurface> surface =
      SkSurfaces::RenderTarget(graphite->recorder(), SkImageInfo::MakeN32Premul(64, 64));
  ASSERT_NE(surface, nullptr);

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
  SkColor center = 0;
  while (std::chrono::steady_clock::now() < deadline) {
    if (view->frameVersion() == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }
    surface->getCanvas()->clear(SK_ColorBLACK);
    view->draw(*surface->getCanvas(), SkRect::MakeWH(64, 64));
    center = readbackPixel(*graphite, surface.get(), 32, 32);
    if (center == SK_ColorCYAN) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  EXPECT_EQ(center, SK_ColorCYAN);
}

// The one-call paint() API on the GPU: the engine's own Graphite
// recorder wraps the slot texture, the callback draws, and flush +
// invalidate happen in the same step.
TEST(WebViewGpuTest, PaintsSlotWithCallback) {
  auto image = sharedEngine().createImage("gpu_paint_swatch", 32, 32);
  ASSERT_NE(image, nullptr);
  ASSERT_TRUE(image->paint([](SkCanvas &canvas) { canvas.clear(SK_ColorYELLOW); }));

  auto view = sharedEngine().createView(64, 64, {.transparent = false});
  ASSERT_NE(view, nullptr);
  view->loadHTML("<html><body style='margin:0;background:#000'>"
                 "<img src='gpu_paint_swatch.imgsrc' "
                 "style='display:block;width:64px;height:64px'>"
                 "</body></html>");

  sigil::skia::GraphiteContext *graphite = sharedGraphite();
  ASSERT_NE(graphite, nullptr);
  sk_sp<SkSurface> surface =
      SkSurfaces::RenderTarget(graphite->recorder(), SkImageInfo::MakeN32Premul(64, 64));
  ASSERT_NE(surface, nullptr);

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
  SkColor center = 0;
  while (std::chrono::steady_clock::now() < deadline) {
    if (view->frameVersion() == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }
    surface->getCanvas()->clear(SK_ColorBLACK);
    view->draw(*surface->getCanvas(), SkRect::MakeWH(64, 64));
    center = readbackPixel(*graphite, surface.get(), 32, 32);
    if (center == SK_ColorYELLOW) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  EXPECT_EQ(center, SK_ColorYELLOW);
}

// Feeding a slot from an externally-produced native texture (the path
// for content rendered by another engine, e.g. a Syphon feed).
TEST(WebViewGpuTest, UpdatesSlotFromNativeTexture) {
  auto image = sharedEngine().createImage("gpu_ext_texture", 16, 16);
  ASSERT_NE(image, nullptr);

  MTLTextureDescriptor *desc =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                         width:16
                                                        height:16
                                                     mipmapped:NO];
  desc.storageMode = MTLStorageModeShared;
  id<MTLDevice> mtl = (__bridge id<MTLDevice>)sharedDevice()->native().mtlDevice;
  id<MTLTexture> external = [mtl newTextureWithDescriptor:desc];
  std::vector<uint32_t> pixels(static_cast<size_t>(16 * 16), 0xffff0000);  // opaque red, BGRA
  [external replaceRegion:MTLRegionMake2D(0, 0, 16, 16)
              mipmapLevel:0
                withBytes:pixels.data()
              bytesPerRow:static_cast<NSUInteger>(16 * 4)];

  // The external texture enters the engine by being named on the shared
  // device — borrowed, so the test keeps it alive.
  sigil::skia::NativeTexture native;
  native.backend = sigil::skia::Backend::Metal;
  native.mtlTexture = (__bridge void *)external;
  native.width = 16;
  native.height = 16;
  const sigil::skia::TextureHandle externalHandle = sharedDevice()->importNative(native);
  ASSERT_TRUE(externalHandle);
  ASSERT_TRUE(image->updateTexture(externalHandle));
  EXPECT_FALSE(image->updateTexture(sigil::skia::TextureHandle{})) << "a null handle";

  auto view = sharedEngine().createView(64, 64, {.transparent = false});
  ASSERT_NE(view, nullptr);
  view->loadHTML("<html><body style='margin:0;background:#000'>"
                 "<img src='gpu_ext_texture.imgsrc' "
                 "style='display:block;width:64px;height:64px'>"
                 "</body></html>");

  sigil::skia::GraphiteContext *graphite = sharedGraphite();
  ASSERT_NE(graphite, nullptr);
  sk_sp<SkSurface> surface =
      SkSurfaces::RenderTarget(graphite->recorder(), SkImageInfo::MakeN32Premul(64, 64));
  ASSERT_NE(surface, nullptr);

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
  SkColor center = 0;
  while (std::chrono::steady_clock::now() < deadline) {
    if (view->frameVersion() == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }
    surface->getCanvas()->clear(SK_ColorBLACK);
    view->draw(*surface->getCanvas(), SkRect::MakeWH(64, 64));
    center = readbackPixel(*graphite, surface.get(), 32, 32);
    if (center == SK_ColorRED) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  EXPECT_EQ(center, SK_ColorRED);
}

TEST(WebViewGpuTest, FrameImageWrapsTexture) {
  auto view = sharedEngine().createView(32, 32, {.transparent = false});
  ASSERT_NE(view, nullptr);
  view->loadHTML("<html><body style='background:#00ff00'></body></html>");

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (view->frameVersion() == 0 && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  ASSERT_GT(view->frameVersion(), 0u);

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
  EXPECT_TRUE(static_cast<bool>(bare));
}
