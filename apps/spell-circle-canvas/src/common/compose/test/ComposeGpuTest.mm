// GPU-backend behavior tests: the raster suite proves semantics, THIS
// suite proves the same pixels arrive on Graphite. Two classes of gap live
// here: direct-image draws (drawImageLattice / drawAtlas) that bypass the
// Recorder's ImageProvider and silently vanish (found as invisible
// nine-slice frames and instance stamps in the GPU gallery), and
// multi-pass glyph compositing (a blurred underlay beneath a stroked
// foreground) that must land identically on both backends.

#include <sigilcompose/Compose.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/instances/Instances.h>
#include <sigilcompose/shape/Shapes.h>
#include <sigilcompose/typography/TextFx.h>

#include <sigilweave/SigilWeave.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <sigilskia/device/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>

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
  static auto *context = new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

sigil::skia::GraphiteContext *graphite() {
  static std::unique_ptr<sigil::skia::GpuDevice> device =
      sigil::skia::GpuDevice::createOwned(sigil::skia::Backend::Metal);
  static std::unique_ptr<sigil::skia::GraphiteContext> ctx =
      device ? sigil::skia::GraphiteContext::create(*device) : nullptr;
  return ctx.get();
}

/** Draws one composer frame on a Graphite surface and reads it back. */
SkBitmap drawOnGpu(Composer &composer, int w, int h) {
  SkBitmap bm;
  sigil::skia::GraphiteContext *ctx = graphite();
  if (!ctx) return bm;
  const SkImageInfo info = SkImageInfo::MakeN32Premul(w, h);
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(ctx->recorder(), info);
  if (!surface) return bm;
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
      surface.get(), info, SkIRect::MakeWH(w, h), SkImage::RescaleGamma::kSrc,
      SkImage::RescaleMode::kNearest,
      [](SkImage::ReadPixelsContext c, std::unique_ptr<const SkImage::AsyncReadResult> r) {
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
  if (!read.result) return bm;
  bm.allocPixels(info);
  const auto *src = static_cast<const uint8_t *>(read.result->data(0));
  const size_t srcRB = read.result->rowBytes(0);
  for (int y = 0; y < h; ++y)
    std::memcpy(bm.pixmap().writable_addr(0, y), src + (size_t)y * srcRB,
                std::min(srcRB, bm.rowBytes()));
  return bm;
}

/** Reads a Graphite surface back to CPU pixels: snap, insert, async read. */
SkBitmap readbackGpu(sigil::skia::GraphiteContext *ctx, SkSurface *surface,
                     const SkImageInfo &info) {
  SkBitmap bm;
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
      surface, info, SkIRect::MakeWH(info.width(), info.height()), SkImage::RescaleGamma::kSrc,
      SkImage::RescaleMode::kNearest,
      [](SkImage::ReadPixelsContext c, std::unique_ptr<const SkImage::AsyncReadResult> r) {
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
  if (!read.result) return bm;
  bm.allocPixels(info);
  const auto *src = static_cast<const uint8_t *>(read.result->data(0));
  const size_t srcRB = read.result->rowBytes(0);
  for (int y = 0; y < info.height(); ++y)
    std::memcpy(bm.pixmap().writable_addr(0, y), src + (size_t)y * srcRB,
                std::min(srcRB, bm.rowBytes()));
  return bm;
}

std::shared_ptr<const sigil::image::ImageAsset> whiteTile(int size) {
  sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(size, size));
  s->getCanvas()->clear(SK_ColorWHITE);
  return std::make_shared<sigil::image::ImageAsset>(
      sigil::image::ImageAsset::wrap(s->makeImageSnapshot()));
}

}  // namespace

#define REQUIRE_GPU()                \
  if (!graphite()) {                 \
    GTEST_SKIP() << "no GPU device"; \
  }

TEST(ComposeGpu, ImageRectDrawsOnGraphite) {
  REQUIRE_GPU();
  sigil::motion::Ticker ticker;
  Composer composer(ticker, fonts());
  composer.setSize({200, 200});
  composer.render(box().child(image(whiteTile(32)).absolute().inset(50, 50, 50, 50)));
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
  composer.render(box().child(box().absolute().inset(50, 50, 50, 50).background(std::move(slice))));
  SkBitmap bm = drawOnGpu(composer, 200, 200);
  ASSERT_FALSE(bm.empty());
  EXPECT_EQ(bm.getColor(100, 100), SK_ColorWHITE);  // center cell stretched
  EXPECT_EQ(bm.getColor(55, 55), SK_ColorWHITE);    // corner cell
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
  sigil::skia::GraphiteContext *ctx = graphite();
  const SkImageInfo info = SkImageInfo::MakeN32Premul(300, 100);
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(ctx->recorder(), info);
  ASSERT_TRUE(surface);
  SkCanvas &c = *surface->getCanvas();
  c.clear(SK_ColorBLACK);

  sk_sp<SkSurface> rasterSrc = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(48, 48));
  rasterSrc->getCanvas()->clear(SK_ColorWHITE);
  sk_sp<SkImage> raster = rasterSrc->makeImageSnapshot();
  sk_sp<SkImage> texture = SkImages::TextureFromImage(ctx->recorder(), raster.get(), {});
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
  c.drawImageLattice(raster.get(), lattice, SkRect::MakeXYWH(10, 10, 80, 80), SkFilterMode::kLinear,
                     nullptr);
  c.restore();
  c.save();
  c.clipRect(SkRect::MakeXYWH(100, 0, 100, 100));
  c.drawImageLattice(texture.get(), lattice, SkRect::MakeXYWH(110, 10, 80, 80),
                     SkFilterMode::kLinear, nullptr);
  c.restore();
  const SkRSXform xf = SkRSXform::Make(1, 0, 250, 50);
  const SkRect tex = SkRect::MakeWH(48, 48);
  c.drawAtlas(texture.get(), SkSpan(&xf, 1), SkSpan(&tex, 1), {}, SkBlendMode::kSrcOver,
              SkSamplingOptions(SkFilterMode::kLinear), nullptr, nullptr);

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
      surface.get(), info, SkIRect::MakeWH(300, 100), SkImage::RescaleGamma::kSrc,
      SkImage::RescaleMode::kNearest,
      [](SkImage::ReadPixelsContext p, std::unique_ptr<const SkImage::AsyncReadResult> r) {
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
    std::memcpy(bm.pixmap().writable_addr(0, y), src + (size_t)y * read.result->rowBytes(0),
                std::min(read.result->rowBytes(0), bm.rowBytes()));
  std::printf("lattice+raster  (50,50):  %08x\n", bm.getColor(50, 50));
  std::printf("lattice+texture (150,50): %08x\n", bm.getColor(150, 50));
  std::printf("atlas+texture   (250,50): %08x\n", bm.getColor(250, 50));
}

namespace {

// A "hollow" letter: a light stroked foreground over a dark blurred stroke
// underlay. The underlay must land BENEATH the stroke — a halo that keeps
// the letterform's own colour intact — on every backend.
constexpr SkColor kHollowForeground = 0xFFDED8CC;

sigil::weave::Paragraph hollowParagraph() {
  using namespace sigil::weave;
  TextStyle style;
  style.shaping.fontSize = 96.0f;
  style.paint.foreground.setAntiAlias(true);
  style.paint.foreground.setStyle(SkPaint::kStroke_Style);
  style.paint.foreground.setStrokeWidth(4.0f);
  style.paint.foreground.setColor(kHollowForeground);
  SkPaint halo;
  halo.setAntiAlias(true);
  halo.setStyle(SkPaint::kStroke_Style);
  halo.setStrokeWidth(7.0f);
  halo.setColor(0xA6000000);
  style.paint.underlays.push_back(PaintLayer::blurred(halo, 3.5f));
  ParagraphBuilder builder(style);
  builder.addText(u8"O");
  return builder.build();
}

/** Batches every glyph of `layout` at its rest pose, whole style. */
sigil::weave::GlyphRSXformBatches batchAtRest(const sigil::weave::ParagraphLayout &layout,
                                              const sigil::weave::Paragraph &paragraph) {
  sigil::weave::GlyphRSXformBatches batches;
  sigil::weave::forEachPlacedGlyph(layout, paragraph, [&](const sigil::weave::PlacedGlyph &glyph) {
    batches.addGlyph(glyph, glyph.rest + SkVector{glyph.advance * 0.5f, 0});
  });
  return batches;
}

/// Pixels within `tolerance` per channel of `color`.
int countNear(const SkBitmap &bm, SkColor color, int tolerance) {
  int count = 0;
  for (int y = 0; y < bm.height(); ++y)
    for (int x = 0; x < bm.width(); ++x) {
      const SkColor c = bm.getColor(x, y);
      if (std::abs((int)SkColorGetR(c) - (int)SkColorGetR(color)) <= tolerance &&
          std::abs((int)SkColorGetG(c) - (int)SkColorGetG(color)) <= tolerance &&
          std::abs((int)SkColorGetB(c) - (int)SkColorGetB(color)) <= tolerance)
        ++count;
    }
  return count;
}

}  // namespace

// A batched glyph's blurred underlay must composite BENEATH its stroked
// foreground on Graphite exactly as it does on the CPU: the stroke keeps
// its colour, and the two backends agree pixel for pixel within blur and
// AA tolerance.
TEST(ComposeGpu, BatchedBlurredUnderlayStaysBeneathForeground) {
  REQUIRE_GPU();
  using namespace sigil::weave;
  FontContext &fontContext = fonts();
  Paragraph paragraph = hollowParagraph();
  ParagraphLayout layout = layoutSingleLine(fontContext, paragraph, {40, 120});
  GlyphRSXformBatches batches = batchAtRest(layout, paragraph);
  ASSERT_EQ(batches.batches.size(), 2u) << "underlay pass + foreground pass";

  const SkImageInfo info = SkImageInfo::MakeN32Premul(180, 160);

  sk_sp<SkSurface> raster = SkSurfaces::Raster(info);
  raster->getCanvas()->clear(SK_ColorBLACK);
  batches.draw(raster->getCanvas());
  SkBitmap cpu;
  cpu.allocPixels(info);
  ASSERT_TRUE(raster->readPixels(cpu, 0, 0));

  sigil::skia::GraphiteContext *ctx = graphite();
  sk_sp<SkSurface> gpuSurface = SkSurfaces::RenderTarget(ctx->recorder(), info);
  ASSERT_TRUE(gpuSurface);
  gpuSurface->getCanvas()->clear(SK_ColorBLACK);
  batches.draw(gpuSurface->getCanvas());
  SkBitmap gpu = readbackGpu(ctx, gpuSurface.get(), info);
  ASSERT_FALSE(gpu.empty());

  // (a) The foreground stroke keeps its colour on both backends. A dimmed
  // stroke (the underlay compositing over it) leaves ZERO pixels near the
  // foreground colour, so a generous AA tolerance still separates the two.
  const int cpuStroke = countNear(cpu, kHollowForeground, 24);
  const int gpuStroke = countNear(gpu, kHollowForeground, 24);
  ASSERT_GT(cpuStroke, 100) << "the CPU render must show the hollow stroke";
  EXPECT_GT(gpuStroke, cpuStroke / 2)
      << "foreground stroke lost its colour on Graphite: the blurred "
         "underlay composited over it";

  // (b) The backends agree within blur/AA tolerance everywhere.
  int mismatched = 0;
  for (int y = 0; y < info.height(); ++y)
    for (int x = 0; x < info.width(); ++x) {
      const SkColor a = cpu.getColor(x, y);
      const SkColor b = gpu.getColor(x, y);
      if (std::abs((int)SkColorGetR(a) - (int)SkColorGetR(b)) > 48 ||
          std::abs((int)SkColorGetG(a) - (int)SkColorGetG(b)) > 48 ||
          std::abs((int)SkColorGetB(a) - (int)SkColorGetB(b)) > 48)
        ++mismatched;
    }
  std::printf("stroke-coloured pixels cpu=%d gpu=%d, mismatched=%d\n", cpuStroke, gpuStroke,
              mismatched);
  EXPECT_LT(mismatched, info.width() * info.height() / 200)
      << "GPU render diverges from the CPU render of the same batches";
}

// The same guarantee through the fx track path: a text node whose style
// carries the blurred underlay, drawn mid-cascade (per-glyph scale and
// fade in flight), must composite underlay -> foreground identically on
// both backends.
TEST(ComposeGpu, FxTrackKeepsBlurredUnderlayBeneathForeground) {
  REQUIRE_GPU();
  using namespace sigil::weave;
  TextStyle style;
  style.shaping.fontSize = 64.0f;
  style.paint.foreground.setAntiAlias(true);
  style.paint.foreground.setStyle(SkPaint::kStroke_Style);
  style.paint.foreground.setStrokeWidth(3.0f);
  style.paint.foreground.setColor(kHollowForeground);
  SkPaint halo;
  halo.setAntiAlias(true);
  halo.setStyle(SkPaint::kStroke_Style);
  halo.setStrokeWidth(7.0f);
  halo.setColor(0xA6000000);
  style.paint.underlays.push_back(PaintLayer::blurred(halo, 3.5f));

  const int w = 420, h = 160;
  auto tree = [&] {
    return box().padding(20).child(text(u8"VERTIGO", style)
                                       .key("word")
                                       .fx({.effect = fx::pop(),
                                            .stagger = {.eachMs = 30, .durationMs = 480},
                                            .progress = 0.55f}));
  };

  sigil::motion::Ticker ticker;
  Composer composer(ticker, fonts());
  composer.setSize({(float)w, (float)h});
  composer.render(tree());

  const SkImageInfo info = SkImageInfo::MakeN32Premul(w, h);
  sk_sp<SkSurface> raster = SkSurfaces::Raster(info);
  raster->getCanvas()->clear(SK_ColorBLACK);
  composer.draw(*raster->getCanvas());
  SkBitmap cpu;
  cpu.allocPixels(info);
  ASSERT_TRUE(raster->readPixels(cpu, 0, 0));

  SkBitmap gpu = drawOnGpu(composer, w, h);
  ASSERT_FALSE(gpu.empty());

  const int cpuStroke = countNear(cpu, kHollowForeground, 24);
  const int gpuStroke = countNear(gpu, kHollowForeground, 24);
  int mismatched = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const SkColor a = cpu.getColor(x, y);
      const SkColor b = gpu.getColor(x, y);
      if (std::abs((int)SkColorGetR(a) - (int)SkColorGetR(b)) > 48 ||
          std::abs((int)SkColorGetG(a) - (int)SkColorGetG(b)) > 48 ||
          std::abs((int)SkColorGetB(a) - (int)SkColorGetB(b)) > 48)
        ++mismatched;
    }
  std::printf("fx-track stroke pixels cpu=%d gpu=%d, mismatched=%d\n", cpuStroke, gpuStroke,
              mismatched);
  ASSERT_GT(cpuStroke, 100) << "the CPU render must show the hollow stroke";
  EXPECT_GT(gpuStroke, cpuStroke / 2)
      << "foreground stroke lost its colour on Graphite: the blurred "
         "underlay composited over it";
  EXPECT_LT(mismatched, w * h / 200) << "GPU render diverges from the CPU render of the same node";
}

// `Track::reach` on an fx::pass grows the pass's painted bounds and
// nothing else, on Graphite exactly as on the CPU. The GPU backend turns
// the pass's layer into an image through its own picture-to-image door, so
// the CPU regression alone does not cover it: the layer's pixels must land
// exactly where the glyphs were placed, and the band beyond the box must
// hold the lifted glyph tops a zero reach clips at the box edge.
TEST(ComposeGpu, TextPassReachKeepsContentInPlaceOnGraphite) {
  REQUIRE_GPU();
  sigil::weave::TextStyle style;
  style.shaping.fontSize = 34.0f;
  style.paint.foreground.setColor(SK_ColorWHITE);
  const TextEffect lift = fx::effect("gpu-lift", [](const GlyphInfo &, float, Rng &) {
    GlyphMod m;
    m.dy = -14.0f;
    return m;
  });
  const char *identity = "half4 main(float2 xy) { return uContent.eval(xy); }";
  const auto describe = [&](float reach) {
    return box().padding(60).child(
        text(u8"HOIST", style)
            .key("hoist")
            .fx({.effect = lift})
            .fx({.effect = fx::pass(Material::sksl(identity)), .reach = reach}));
  };
  const int w = 200, h = 200;
  sigil::motion::Ticker snugTicker;
  Composer snug(snugTicker, fonts());
  snug.setSize({(float)w, (float)h});
  snug.render(describe(0.0f));
  const SkBitmap snugPx = drawOnGpu(snug, w, h);
  ASSERT_FALSE(snugPx.empty());
  sigil::motion::Ticker wideTicker;
  Composer wide(wideTicker, fonts());
  wide.setSize({(float)w, (float)h});
  wide.render(describe(40.0f));
  const SkBitmap widePx = drawOnGpu(wide, w, h);
  ASSERT_FALSE(widePx.empty());
  const std::optional<SkRect> laidOut = wide.bounds("hoist");
  ASSERT_TRUE(laidOut.has_value());
  const SkRect box = laidOut.value_or(SkRect::MakeEmpty());

  // Inside the box the two renders agree pixel for pixel (an AA-width
  // tolerance, same as every backend comparison here) — and glyph pixels
  // must exist there, or the agreement proved nothing.
  SkIRect inside = box.round();
  inside.inset(1, 1);
  int mismatched = 0;
  int glyphPixels = 0;
  for (int y = inside.top(); y < inside.bottom(); ++y)
    for (int x = inside.left(); x < inside.right(); ++x) {
      const SkColor a = snugPx.getColor(x, y);
      const SkColor b = widePx.getColor(x, y);
      if (std::abs((int)SkColorGetR(a) - (int)SkColorGetR(b)) > 8 ||
          std::abs((int)SkColorGetG(a) - (int)SkColorGetG(b)) > 8 ||
          std::abs((int)SkColorGetB(a) - (int)SkColorGetB(b)) > 8)
        ++mismatched;
      if (SkColorGetR(a) > 200 && SkColorGetG(a) > 200 && SkColorGetB(a) > 200) ++glyphPixels;
    }
  EXPECT_EQ(mismatched, 0) << "reach moved the pass's layer on Graphite";
  EXPECT_GT(glyphPixels, 0) << "no glyph pixels inside the box";

  // The band above the box: lifted tops under the wide reach, bare under
  // the snug one.
  int bandWide = 0, bandSnug = 0;
  for (int y = (int)box.top() - 16; y < (int)box.top() - 1; ++y)
    for (int x = (int)box.left(); x < (int)box.right(); ++x) {
      if (SkColorGetR(widePx.getColor(x, y)) > 200) ++bandWide;
      if (SkColorGetR(snugPx.getColor(x, y)) > 200) ++bandSnug;
    }
  EXPECT_GT(bandWide, 0) << "the reach band was not painted";
  EXPECT_EQ(bandSnug, 0) << "a zero reach painted beyond the box";
}

// A TURNING RING ON GRAPHITE moves as smoothly as it does on the raster
// backend. The rounding a glyph's device origin takes is decided by the
// strike rather than by the backend, so this is the same measure the raster
// suite makes and it must reach the same verdict: one letter on a ring, its
// ink centroid tracked across consecutive frames, and the step between them
// — which goes exactly to zero on whole-pixel origins, the letter standing
// still for a frame before hopping a whole pixel.
TEST(ComposeGpu, ATurningRingAdvancesSmoothly) {
  REQUIRE_GPU();
  constexpr int kField = 400;
  constexpr float kPhaseStep = 1.0f / 2400.0f;
  constexpr int kFrames = 60;
  for (const float size : {14.0f, 44.0f}) {
    sigil::motion::Ticker ticker;
    Composer composer(ticker, fonts());
    composer.setSize({kField, kField});
    sigil::weave::TextStyle style;
    style.shaping.fontSize = size;
    style.paint.foreground.setColor(SK_ColorWHITE);
    // Bound, which is how a marquee declares that it is turning.
    choreograph::Output<float> phase{0.05f};
    std::vector<SkPoint> track;
    for (int i = 0; i < kFrames; ++i) {
      phase = 0.05f + kPhaseStep * (float)i;
      composer.render(box().child(
          text(u8"H", style)
              .key("ring")
              .width(kField)
              .height(kField)
              .absolute()
              .left(0)
              .top(0)
              .onPath({.path = shapes::circle(), .at = &phase, .align = TextPath::Align::Center})));
      SkBitmap bm = drawOnGpu(composer, kField, kField);
      ASSERT_FALSE(bm.isNull());
      double sx = 0, sy = 0, sw = 0;
      for (int y = 0; y < kField; ++y)
        for (int x = 0; x < kField; ++x) {
          const double w = SkColorGetG(bm.getColor(x, y)) / 255.0;
          sx += w * x;
          sy += w * y;
          sw += w;
        }
      ASSERT_GT(sw, 0);
      track.push_back({(float)(sx / sw), (float)(sy / sw)});
    }
    double sumSq = 0, meanStep = 0, minStep = 1e9;
    for (size_t i = 1; i + 1 < track.size(); ++i) {
      const double jerk = std::hypot(track[i + 1].x() - 2 * track[i].x() + track[i - 1].x(),
                                     track[i + 1].y() - 2 * track[i].y() + track[i - 1].y());
      sumSq += jerk * jerk;
    }
    for (size_t i = 1; i < track.size(); ++i) {
      const double d = std::hypot(track[i].x() - track[i - 1].x(), track[i].y() - track[i - 1].y());
      minStep = std::min(minStep, d);
      meanStep += d;
    }
    meanStep /= (double)(track.size() - 1);
    const double rmsJerk = std::sqrt(sumSq / (double)(track.size() - 2));
    SCOPED_TRACE(testing::Message() << "size " << size << " meanStep " << meanStep << " minStep "
                                    << minStep << " rmsJerk " << rmsJerk);
    ASSERT_GT(meanStep, 0.1) << "the ring did not turn";
    EXPECT_GT(minStep, 0.25 * meanStep) << "a frame the letter stood still";
    EXPECT_LT(rmsJerk, 0.5 * meanStep) << "the letter's advance ticks";
  }
}
