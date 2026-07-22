// GPU-backend behavior tests: the raster suite proves semantics, THIS
// suite proves the same pixels arrive on Graphite — the class of gap it
// exists for is direct-image draws (drawImageLattice / drawAtlas) that
// bypass the Recorder's ImageProvider and silently vanish (found as
// invisible nine-slice frames and instance stamps in the GPU gallery).

#import <Metal/Metal.h>

#include <sigilcompose/Compose.h>
#include <sigilcompose/Decorations.h>
#include <sigilcompose/Instances.h>

#include <sigilweave/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include "SkiaGraphiteContext.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>

#include <gtest/gtest.h>

using namespace sigil::compose;

namespace {

sigil::weave::FontContext &fonts() {
  static auto *context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

SkiaGraphiteContext *graphite() {
  static std::unique_ptr<SkiaGraphiteContext> ctx = [] {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> queue = [device newCommandQueue];
    return SkiaGraphiteContext::createMetal((__bridge void *)device,
                                            (__bridge void *)queue);
  }();
  return ctx.get();
}

/** Draws one composer frame on a Graphite surface and reads it back. */
SkBitmap drawOnGpu(Composer &composer, int w, int h) {
  SkBitmap bm;
  SkiaGraphiteContext *ctx = graphite();
  if (!ctx)
    return bm;
  const SkImageInfo info = SkImageInfo::MakeN32Premul(w, h);
  sk_sp<SkSurface> surface =
      SkSurfaces::RenderTarget(ctx->recorder(), info);
  if (!surface)
    return bm;
  surface->getCanvas()->clear(SK_ColorBLACK);
  composer.draw(*surface->getCanvas());
  if (auto recording = ctx->recorder()->snap()) {
    skgpu::graphite::InsertRecordingInfo insert;
    insert.fRecording = recording.get();
    ctx->context()->insertRecording(insert);
  }
  struct Read {
    std::unique_ptr<const SkImage::AsyncReadResult> result;
    bool called = false;
  } read;
  ctx->context()->asyncRescaleAndReadPixels(
      surface.get(), info, SkIRect::MakeWH(w, h),
      SkImage::RescaleGamma::kSrc, SkImage::RescaleMode::kNearest,
      [](SkImage::ReadPixelsContext c,
         std::unique_ptr<const SkImage::AsyncReadResult> r) {
        auto *read = static_cast<Read *>(c);
        read->result = std::move(r);
        read->called = true;
      },
      &read);
  skgpu::graphite::SubmitInfo submitInfo;
  submitInfo.fSync = skgpu::graphite::SyncToCpu::kYes;
  ctx->context()->submit(submitInfo);
  for (int spin = 0; spin < 5000 && !read.called; ++spin)
    ctx->context()->checkAsyncWorkCompletion();
  if (!read.result)
    return bm;
  bm.allocPixels(info);
  const auto *src = static_cast<const uint8_t *>(read.result->data(0));
  const size_t srcRB = read.result->rowBytes(0);
  for (int y = 0; y < h; ++y)
    std::memcpy(bm.pixmap().writable_addr(0, y), src + (size_t)y * srcRB,
                std::min(srcRB, bm.rowBytes()));
  return bm;
}

std::shared_ptr<const sigil::image::ImageAsset> whiteTile(int size) {
  sk_sp<SkSurface> s =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(size, size));
  s->getCanvas()->clear(SK_ColorWHITE);
  return std::make_shared<sigil::image::ImageAsset>(
      sigil::image::ImageAsset::wrap(s->makeImageSnapshot()));
}

} // namespace

#define REQUIRE_GPU()                                                        \
  if (!graphite()) {                                                         \
    GTEST_SKIP() << "no Metal device";                                       \
  }

TEST(ComposeGpu, ImageRectDrawsOnGraphite) {
  REQUIRE_GPU();
  sigil::motion::Ticker ticker;
  Composer composer(ticker, fonts());
  composer.setSize({200, 200});
  composer.render(box().child(
      image(whiteTile(32)).absolute().inset(50, 50, 50, 50)));
  SkBitmap bm = drawOnGpu(composer, 200, 200);
  ASSERT_FALSE(bm.empty());
  EXPECT_EQ(bm.getColor(100, 100), SK_ColorWHITE);
}

TEST(ComposeGpu, NineSliceLatticeDrawsOnGraphite) {
  REQUIRE_GPU();
  sigil::motion::Ticker ticker;
  Composer composer(ticker, fonts());
  composer.setSize({200, 200});
  Decoration slice = Slice{whiteTile(48), {16, 32}, {16, 32}};
  composer.render(box().child(
      box().absolute().inset(50, 50, 50, 50).background(std::move(slice))));
  SkBitmap bm = drawOnGpu(composer, 200, 200);
  ASSERT_FALSE(bm.empty());
  EXPECT_EQ(bm.getColor(100, 100), SK_ColorWHITE); // center cell stretched
  EXPECT_EQ(bm.getColor(55, 55), SK_ColorWHITE);   // corner cell
}

TEST(ComposeGpu, InstanceStampsDrawOnGraphite) {
  REQUIRE_GPU();
  using namespace sigil::compose::instancing;
  sigil::motion::Ticker ticker;
  Composer composer(ticker, fonts());
  composer.setSize({200, 200});
  auto atlas = std::make_shared<Atlas>();
  atlas->cell(box().fill(Fill::color({1, 1, 1, 1})), {24, 24});
  auto pool = std::make_shared<Pool>();
  pool->add({60, 60});
  pool->add({140, 140});
  composer.render(box().child(instances(atlas, pool, Mode::Live)));
  SkBitmap bm = drawOnGpu(composer, 200, 200);
  ASSERT_FALSE(bm.empty());
  EXPECT_EQ(bm.getColor(60, 60), SK_ColorWHITE);
  EXPECT_EQ(bm.getColor(140, 140), SK_ColorWHITE);
  EXPECT_EQ(bm.getColor(100, 100), SK_ColorBLACK);
}

// Diagnostic: raw SkCanvas calls on a Graphite surface, no composer —
// which primitive × image-kind combinations actually land?
TEST(ComposeGpu, DirectPrimitiveMatrix) {
  REQUIRE_GPU();
  SkiaGraphiteContext *ctx = graphite();
  const SkImageInfo info = SkImageInfo::MakeN32Premul(300, 100);
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(ctx->recorder(), info);
  ASSERT_TRUE(surface);
  SkCanvas &c = *surface->getCanvas();
  c.clear(SK_ColorBLACK);

  sk_sp<SkSurface> rasterSrc =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(48, 48));
  rasterSrc->getCanvas()->clear(SK_ColorWHITE);
  sk_sp<SkImage> raster = rasterSrc->makeImageSnapshot();
  sk_sp<SkImage> texture =
      SkImages::TextureFromImage(ctx->recorder(), raster.get(), {});
  ASSERT_TRUE(texture);

  SkCanvas::Lattice lattice{};
  const int divs[2] = {16, 32};
  lattice.fXDivs = divs;
  lattice.fYDivs = divs;
  lattice.fXCount = 2;
  lattice.fYCount = 2;

  // x 0..100: lattice with raster; 100..200: lattice with texture;
  // 200..300: drawAtlas with texture.
  c.save();
  c.clipRect(SkRect::MakeXYWH(0, 0, 100, 100));
  c.drawImageLattice(raster.get(), lattice, SkRect::MakeXYWH(10, 10, 80, 80),
                     SkFilterMode::kLinear, nullptr);
  c.restore();
  c.save();
  c.clipRect(SkRect::MakeXYWH(100, 0, 100, 100));
  c.drawImageLattice(texture.get(), lattice,
                     SkRect::MakeXYWH(110, 10, 80, 80),
                     SkFilterMode::kLinear, nullptr);
  c.restore();
  const SkRSXform xf = SkRSXform::Make(1, 0, 250, 50);
  const SkRect tex = SkRect::MakeWH(48, 48);
  c.drawAtlas(texture.get(), SkSpan(&xf, 1), SkSpan(&tex, 1), {},
              SkBlendMode::kSrcOver, SkSamplingOptions(SkFilterMode::kLinear),
              nullptr, nullptr);

  // Read back through the same async path as drawOnGpu.
  if (auto recording = ctx->recorder()->snap()) {
    skgpu::graphite::InsertRecordingInfo insert;
    insert.fRecording = recording.get();
    ctx->context()->insertRecording(insert);
  }
  struct Read {
    std::unique_ptr<const SkImage::AsyncReadResult> result;
    bool called = false;
  } read;
  ctx->context()->asyncRescaleAndReadPixels(
      surface.get(), info, SkIRect::MakeWH(300, 100),
      SkImage::RescaleGamma::kSrc, SkImage::RescaleMode::kNearest,
      [](SkImage::ReadPixelsContext p,
         std::unique_ptr<const SkImage::AsyncReadResult> r) {
        auto *read = static_cast<Read *>(p);
        read->result = std::move(r);
        read->called = true;
      },
      &read);
  skgpu::graphite::SubmitInfo submitInfo;
  submitInfo.fSync = skgpu::graphite::SyncToCpu::kYes;
  ctx->context()->submit(submitInfo);
  for (int spin = 0; spin < 5000 && !read.called; ++spin)
    ctx->context()->checkAsyncWorkCompletion();
  ASSERT_TRUE(read.result);
  SkBitmap bm;
  bm.allocPixels(info);
  const auto *src = static_cast<const uint8_t *>(read.result->data(0));
  for (int y = 0; y < 100; ++y)
    std::memcpy(bm.pixmap().writable_addr(0, y),
                src + (size_t)y * read.result->rowBytes(0),
                std::min(read.result->rowBytes(0), bm.rowBytes()));
  std::printf("lattice+raster  (50,50):  %08x\n", bm.getColor(50, 50));
  std::printf("lattice+texture (150,50): %08x\n", bm.getColor(150, 50));
  std::printf("atlas+texture   (250,50): %08x\n", bm.getColor(250, 50));
}
