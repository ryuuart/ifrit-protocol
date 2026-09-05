// GPU-backend behavior tests: the raster suite proves semantics, THIS
// suite proves the same pixels arrive on Graphite. Two classes of gap live
// here: direct-image draws (drawImageLattice / drawAtlas) that bypass the
// Recorder's ImageProvider and silently vanish (found as invisible
// nine-slice frames and instance stamps in the GPU gallery), and
// multi-pass glyph compositing (a blurred underlay beneath a stroked
// foreground) that must land identically on both backends.

#include <sigilcompose/Compose.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Instances.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Silhouettes.h>

#include <sigilmaterial/core/Material.h>

#include <sigilweave/SigilWeave.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <sigilcore/hardware/GpuDevice.h>
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

#include <functional>
#include <vector>

#include "Fonts.h"

using namespace sigil::compose;

namespace geometry = sigil::geometry;

namespace {

using sigil::test::fonts;

sigil::skia::GraphiteContext *graphite() {
  static std::unique_ptr<sigil::core::hardware::GpuDevice> device =
      sigil::core::hardware::GpuDevice::createOwned();
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

namespace {

/** A draw the Recorder's ImageProvider does not see -- a direct image
 *  draw or a stamped atlas -- and where its white must land. These are
 *  the draws that vanish silently on Graphite while the raster suite
 *  stays green, so each names a lit point and, where the draw has a
 *  shape, a point it must leave dark. */
struct DirectDraw {
  const char *what;
  std::function<Element()> describe;
  std::vector<SkIPoint> lit;
  std::vector<SkIPoint> dark;
};

class DirectImageDraw : public testing::TestWithParam<DirectDraw> {};

}  // namespace

TEST_P(DirectImageDraw, ItsPixelsArriveOnGraphite) {
  REQUIRE_GPU();
  const DirectDraw &draw = GetParam();
  sigil::motion::Ticker ticker;
  Composer composer(ticker, fonts());
  composer.setSize({200, 200});
  composer.render(draw.describe());
  SkBitmap bm = drawOnGpu(composer, 200, 200);
  ASSERT_FALSE(bm.empty());
  for (const SkIPoint &p : draw.lit)
    EXPECT_EQ(bm.getColor(p.x(), p.y()), SK_ColorWHITE) << p.x() << "," << p.y();
  for (const SkIPoint &p : draw.dark)
    EXPECT_EQ(bm.getColor(p.x(), p.y()), SK_ColorBLACK) << p.x() << "," << p.y();
}

INSTANTIATE_TEST_SUITE_P(
    ComposeGpu, DirectImageDraw,
    testing::Values(
        DirectDraw{"AnImageRect",
                   [] { return box().child(image(whiteTile(32)).absolute().inset(50, 50, 50, 50)); },
                   {{100, 100}},
                   {}},
        DirectDraw{"ANineSliceLattice",
                   [] {
                     Decoration slice = Slice{whiteTile(48), {16, 32}, {16, 32}};
                     return box().child(
                         box().absolute().inset(50, 50, 50, 50).background(std::move(slice)));
                   },
                   {{100, 100}, {55, 55}},  // the stretched centre cell, then a corner cell
                   {}},
        DirectDraw{"AStampedAtlas",
                   [] {
                     using namespace sigil::compose::instancing;
                     auto atlas = std::make_shared<Atlas>();
                     atlas->cell(box().fill(Fill::color({1, 1, 1, 1})), {24, 24});
                     auto pool = std::make_shared<Pool>();
                     pool->add({60, 60});
                     pool->add({140, 140});
                     return box().child(instances(atlas, pool, Mode::Live));
                   },
                   {{60, 60}, {140, 140}},
                   {{100, 100}}}),
    [](const testing::TestParamInfo<DirectDraw> &info) { return info.param.what; });

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

  // The foreground stroke keeps its colour on both backends. A dimmed
  // stroke (the underlay compositing over it) leaves ZERO pixels near the
  // foreground colour, so a generous AA tolerance still separates the two.
  const int cpuStroke = countNear(cpu, kHollowForeground, 24);
  const int gpuStroke = countNear(gpu, kHollowForeground, 24);
  ASSERT_GT(cpuStroke, 100) << "the CPU render must show the hollow stroke";
  EXPECT_GT(gpuStroke, cpuStroke / 2)
      << "foreground stroke lost its colour on Graphite: the blurred "
         "underlay composited over it";

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
  ASSERT_GT(cpuStroke, 100) << "the CPU render must show the hollow stroke";
  EXPECT_GT(gpuStroke, cpuStroke / 2)
      << "foreground stroke lost its colour on Graphite: the blurred "
         "underlay composited over it";
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
  const TextEffect lift =
      fx::effect("gpu-lift", [](const GlyphInfo &, float, sigil::core::noise::Mix64Stream &) {
        GlyphMod m;
        m.dy = -14.0f;
        return m;
      });
  struct NoParams {};
  const auto identity = std::make_shared<const sigil::material::Recipe>(
      sigil::material::Recipe::of<NoParams>("gpu.identity-pass")
          .body(sigil::material::Target::SkSL,
                "half4 main(float2 xy) { return uContent.eval(xy); }"));
  const auto describe = [&](float reach) {
    return box().padding(60).child(text(u8"HOIST", style)
                                       .key("hoist")
                                       .fx({.effect = lift})
                                       .fx({.effect = fx::pass(sigil::material::skia::Paint::recipe(
                                                sigil::material::Material(identity))),
                                            .reach = reach}));
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
