// Graphite cases for draws a backend can lose without the raster suites
// noticing: direct images and stamped atlases, batched glyph compositing
// with a blurred underlay, a text pass's reach, and the coverage masks a
// decoration holds, which are cut on the device, and a held backdrop,
// which is copied back from the device.

#include <include/effects/SkImageFilters.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Paint.h>

#include <cstring>

#include "paint/HeldCoverage.h"
#include "support/GpuTestSupport.h"

using namespace sigil::compose::graphiteTesting;

namespace {

std::shared_ptr<const sigil::media::Image> whiteTile(int size) {
  sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(size, size));
  surface->getCanvas()->clear(SK_ColorWHITE);
  return sigil::media::Image::of(surface->makeImageSnapshot());
}

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
  sigil::motion::Engine engine;
  Composer composer(engine, fonts());
  composer.setSize({200, 200});
  composer.render(draw.describe());
  SkBitmap bitmap = drawOnGpu(composer, 200, 200);
  ASSERT_FALSE(bitmap.empty());
  for (const SkIPoint &p : draw.lit)
    EXPECT_EQ(bitmap.getColor(p.x(), p.y()), SK_ColorWHITE) << p.x() << "," << p.y();
  for (const SkIPoint &p : draw.dark)
    EXPECT_EQ(bitmap.getColor(p.x(), p.y()), SK_ColorBLACK) << p.x() << "," << p.y();
}

INSTANTIATE_TEST_SUITE_P(
    ComposeGpu, DirectImageDraw,
    testing::Values(
        DirectDraw{"AnImageRect",
                   [] {
                     return box().children(
                         {image(whiteTile(32))
                              .absolute()
                              .inset({.top = 50, .right = 50, .bottom = 50, .left = 50})});
                   },
                   {{100, 100}},
                   {}},
        DirectDraw{"ANineSliceLattice",
                   [] {
                     Decoration slice = Slice{whiteTile(48), {16, 32}, {16, 32}};
                     return box().children(
                         {box()
                              .absolute()
                              .inset({.top = 50, .right = 50, .bottom = 50, .left = 50})
                              .background(std::move(slice))});
                   },
                   {{100, 100}, {55, 55}},  // the stretched centre cell, then a corner cell
                   {}},
        DirectDraw{"AStampedAtlas",
                   [] {
                     using namespace sigil::compose::instancing;
                     auto atlas = std::make_shared<CellSheet>();
                     atlas->cell(box().fill(Fill::color({1, 1, 1, 1})), {24, 24});
                     auto pool = std::make_shared<Pool>();
                     pool->add({60, 60});
                     pool->add({140, 140});
                     return box().children({instances(atlas, pool, Mode::Live)});
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
    batches.addGlyph(glyph, glyph.rest + glm::vec2{glyph.advance * 0.5f, 0});
  });
  return batches;
}

/// Pixels within `tolerance` per channel of `color`.
int countNear(const SkBitmap &bitmap, SkColor color, int tolerance) {
  int count = 0;
  for (int y = 0; y < bitmap.height(); ++y)
    for (int x = 0; x < bitmap.width(); ++x) {
      const SkColor c = bitmap.getColor(x, y);
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

  sigil::skia::GraphiteContext *context = graphite();
  sk_sp<SkSurface> gpuSurface = SkSurfaces::RenderTarget(context->recorder(), info);
  ASSERT_TRUE(gpuSurface);
  gpuSurface->getCanvas()->clear(SK_ColorBLACK);
  batches.draw(gpuSurface->getCanvas());
  SkBitmap gpu = readbackGpu(*context, *gpuSurface, info);
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

// The same guarantee through the textFx track path: a text node whose style
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

  const int width = 420, height = 160;
  auto tree = [&] {
    return box().padding(20).children(
        {text(u8"VERTIGO", style)
             .key("word")
             .textFx({.effect = textFx::enter(textFx::pop()),
                      .tween = {.duration = std::chrono::milliseconds(480),
                                .delay = sigil::motion::stagger(std::chrono::milliseconds(30))},
                      .progress = 0.55f})});
  };

  sigil::motion::Engine engine;
  Composer composer(engine, fonts());
  composer.setSize({(float)width, (float)height});
  composer.render(tree());

  const SkImageInfo info = SkImageInfo::MakeN32Premul(width, height);
  sk_sp<SkSurface> raster = SkSurfaces::Raster(info);
  raster->getCanvas()->clear(SK_ColorBLACK);
  composer.draw(*raster->getCanvas());
  SkBitmap cpu;
  cpu.allocPixels(info);
  ASSERT_TRUE(raster->readPixels(cpu, 0, 0));

  SkBitmap gpu = drawOnGpu(composer, width, height);
  ASSERT_FALSE(gpu.empty());

  const int cpuStroke = countNear(cpu, kHollowForeground, 24);
  const int gpuStroke = countNear(gpu, kHollowForeground, 24);
  ASSERT_GT(cpuStroke, 100) << "the CPU render must show the hollow stroke";
  EXPECT_GT(gpuStroke, cpuStroke / 2)
      << "foreground stroke lost its colour on Graphite: the blurred "
         "underlay composited over it";
}

// `Track::reach` on an textFx::pass grows the pass's painted bounds and
// nothing else, on Graphite exactly as on the CPU. The GPU backend turns
// the pass's layer into an image through its own picture-to-image door, so
// the CPU claim alone does not cover it: the layer's pixels must land
// exactly where the glyphs were placed, and the band beyond the box must
// hold the lifted glyph tops a zero reach clips at the box edge.
TEST(ComposeGpu, TextPassReachKeepsContentInPlaceOnGraphite) {
  REQUIRE_GPU();
  sigil::weave::TextStyle style;
  style.shaping.fontSize = 34.0f;
  style.paint.foreground.setColor(SK_ColorWHITE);
  const TextEffect lift =
      textFx::effect("gpu-lift", [](const GlyphInfo &, float, sigil::core::noise::Mix64Stream &) {
        GlyphModifier m;
        m.dy = -14.0f;
        return m;
      });
  struct NoParameters {};
  const auto identity = std::make_shared<const sigil::material::Recipe>(
      sigil::material::Recipe::of<NoParameters>("gpu.identity-pass")
          .body(sigil::material::Target::SkSL,
                "half4 main(float2 xy) { return uContent.eval(xy); }"));
  const auto describe = [&](float reach) {
    return box().padding(60).children(
        {text(u8"HOIST", style)
             .key("hoist")
             .textFx({.effect = lift})
             .textFx(
                 {.effect = textFx::pass(sigil::material::Material(identity)), .reach = reach})});
  };
  const int width = 200, height = 200;
  sigil::motion::Engine snugEngine;
  Composer snug(snugEngine, fonts());
  snug.setSize({(float)width, (float)height});
  snug.render(describe(0.0f));
  const SkBitmap snugPx = drawOnGpu(snug, width, height);
  ASSERT_FALSE(snugPx.empty());
  sigil::motion::Engine wideEngine;
  Composer wide(wideEngine, fonts());
  wide.setSize({(float)width, (float)height});
  wide.render(describe(40.0f));
  const SkBitmap widePx = drawOnGpu(wide, width, height);
  ASSERT_FALSE(widePx.empty());
  const std::optional<geometry::path::Rect> laidOut = wide.bounds("hoist");
  ASSERT_TRUE(laidOut.has_value());
  const SkRect box = geometry::path::toSk(laidOut.value_or(geometry::path::Rect{}));

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

namespace {

/** A sheet dressed by its coverage — a bevel inside it, a shadow beneath
 *  — over a bar that turns, so its silhouette is composed from held
 *  parts. */
Element coverageSheetOverATurningBar(const sigil::motion::Animatable<float> &turn) {
  Element bar = box()
                    .absolute()
                    .left(40)
                    .top(100)
                    .width(160)
                    .height(40)
                    .transformOrigin(percent(50), percent(50))
                    .rotate(sigil::motion::bind(turn, {.to = {0.0f, 360.0f}}))
                    .fill(Fill::color({0.9f, 0.7f, 0.3f, 1}));
  return box().children(
      {box()
           .absolute()
           .left(0)
           .top(0)
           .width(240)
           .height(240)
           .decorationOutline(Boundary::Coverage)
           .fill(sigil::material::from(sigil::material::Color{0, 0, 0, 0})
                     .effects(sigil::material::Filter::bevel({.depth = 2,
                                                              .size = 3,
                                                              .angleDegrees = 125,
                                                              .highlight = {1, 0.9f, 0.7f, 0.6f},
                                                              .shadow = {0.1f, 0.05f, 0, 0.6f}})))
           .background(shadow(sigil::material::Color{0, 0, 0, 0.6f}, {6, 8}, 6))
           .children({bar})});
}

}  // namespace

// A held mask is drawn where the mark is drawn: on a device, a change of
// the density it is cut at — a zoom settling — cuts it again on the
// device and leaves the processor nothing to raster.
TEST(ComposeGpu, AHeldCoverageMaskIsCutOnTheDevice) {
  REQUIRE_GPU();
  sigil::skia::GraphiteContext &context = *graphite();
  sigil::motion::Animatable<float> turn = sigil::motion::animatable(0.0f);
  sigil::motion::Engine engine;
  Composer composer(engine, fonts());
  composer.setSize({240, 240});
  composer.render(coverageSheetOverATurningBar(turn));
  ASSERT_FALSE(drawOnGpu(composer, context, 240, 240).empty());
  const detail::CoverageCastsDrawn first = detail::coverageCastsDrawn();
  for (int frame = 1; frame <= 4; ++frame) {
    turn = (float)frame / 100.0f;
    composer.setBakeDensity(frame % 2 == 0 ? 1.0f : 1.5f);
    ASSERT_FALSE(drawOnGpu(composer, context, 240, 240).empty());
  }
  const detail::CoverageCastsDrawn settled = detail::coverageCastsDrawn();
  EXPECT_GT(settled.device, first.device) << "each density cut its masks";
  EXPECT_EQ(settled.processor, first.processor) << "and none of them on the processor";
}

// A backdrop held for a ground the host keeps is copied back on Graphite as
// it is on the CPU: a held composer and a live one draw the same pixels on
// every frame, each on a surface of its own as a window's frames are, and
// the held one filters only until nothing beneath it has changed.
TEST(ComposeGpu, AHeldBackdropIsCopiedBackOnGraphite) {
  REQUIRE_GPU();
  const auto frostedPane = [] {
    return stack().children(
        {box().inset(0).fill(Fill::color({1, 0, 0, 1})),
         box()
             .absolute()
             .left(0)
             .top(0)
             .width(90)
             .height(200)
             .fill(Fill::color({0, 1, 0, 1})),
         box()
             .absolute()
             .left(40)
             .top(40)
             .width(120)
             .height(120)
             .borderRadius({18})
             .backdropFilter(sigil::material::skia::filter(
                 SkImageFilters::Blur(6, 6, nullptr)))
             .fill(Fill::color({1, 1, 1, 0.15f}))});
  };
  sigil::motion::Engine heldEngine, liveEngine;
  Composer held(heldEngine, fonts()), live(liveEngine, fonts());
  for (Composer* composer : {&held, &live}) {
    composer->setSize({200, 200});
    composer->render(frostedPane());
  }
  held.setGroundHeld(true);
  for (int frame = 0; frame < 4; ++frame) {
    const SkBitmap heldPixels = drawOnGpu(held, 200, 200);
    const SkBitmap livePixels = drawOnGpu(live, 200, 200);
    ASSERT_FALSE(heldPixels.empty());
    ASSERT_FALSE(livePixels.empty());
    for (int y = 0; y < 200; ++y)
      ASSERT_EQ(std::memcmp(heldPixels.getAddr32(0, y),
                            livePixels.getAddr32(0, y), 200 * 4),
                0)
          << "frame " << frame << ", row " << y;
  }
  EXPECT_EQ(held.stats().backdropsFiltered, 0u)
      << "nothing beneath the pane changed: its pixels are copied back";
}

namespace {

/** A KEYLINE THAT STATES HOW FAR IT REACHES and draws the same line
 *  whatever it states, so two nodes that differ only in the statement
 *  paint the same marks. */
struct StatedKeyline {
  float statedBleed = 1;
  bool operator==(const StatedKeyline&) const = default;
  float bleed() const { return statedBleed; }
  void paint(sigil::draw::Pen& pen, const PaintContext& context) const {
    SkPaint line;
    line.setAntiAlias(true);
    line.setStyle(SkPaint::kStroke_Style);
    line.setStrokeWidth(2);
    line.setColor(SK_ColorWHITE);
    pen.canvas()->drawRect(SkRect::MakeWH(context.size.x, context.size.y),
                           line);
  }
};

/** A panel under @p filter, kept as @p cache and turned by @p degrees,
 *  holding a keyline that states @p bleed. */
Element filteredPanel(const std::optional<sigil::material::Filter>& filter,
                      float bleed, Cache cache, float degrees) {
  Element panel = box()
                      .absolute()
                      .left(37)
                      .top(29)
                      .width(300)
                      .height(200)
                      .cache(cache)
                      .rotate(degrees)
                      .foreground(StatedKeyline{bleed})
                      .children({box()
                                     .absolute()
                                     .left(20)
                                     .top(16)
                                     .width(180)
                                     .height(120)
                                     .fill(Fill::color({0.2f, 0.6f, 1, 1})),
                                 text(u8"PANEL")
                                     .font({.face = sigil::test::instrument::sans(),
                                            .size = 23})
                                     .ink(sigil::material::Color{1, 1, 1, 1})
                                     .absolute()
                                     .left(40)
                                     .top(150)});
  if (filter) panel.filter(*filter);
  return box().width(420).height(320).children({std::move(panel)});
}

/** The panel drawn on the device at @p scale device pixels a unit. */
SkBitmap panelOnGpu(const std::optional<sigil::material::Filter>& filter,
                    float bleed, float scale, Cache cache, float degrees) {
  sigil::motion::Engine engine;
  Composer composer(engine, fonts());
  composer.setSize({420, 320});
  composer.render(filteredPanel(filter, bleed, cache, degrees));
  sigil::skia::GraphiteContext& context = *graphite();
  const int width = (int)(420 * scale), height = (int)(320 * scale);
  const SkImageInfo info = SkImageInfo::MakeN32Premul(width, height);
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(context.recorder(), info);
  if (!surface) return {};
  SkCanvas& canvas = *surface->getCanvas();
  canvas.clear(SK_ColorBLACK);
  canvas.scale(scale, scale);
  composer.draw(canvas);
  return readbackGpu(context, *surface, info);
}

}  // namespace

TEST(ComposeGpu, AFilterGivesOnePictureWhateverTheBleedInsideIt) {
  REQUIRE_GPU();
  using sigil::material::Filter;
  const std::pair<const char*, std::optional<Filter>> filters[] = {
      {"no filter", std::nullopt},
      {"blur 3", Filter::blur(3)},
      {"blur 14", Filter::blur(14)},
      {"blur 44", Filter::blur(44)},
      {"glow 7", Filter::glow({1, 0.6f, 0.2f, 0.8f}, 7)},
      {"directional blur", Filter::directionalBlur(18, 90, 12)},
      {"bloom", Filter::bloom({.sigma = 2.5f,
                               .strength = 0.5f,
                               .spread = 2.2f,
                               .tail = 0.3f,
                               .threshold = 0.12f,
                               .knee = 0.18f,
                               .softness = 0.85f,
                               .whitening = 0.2f,
                               .dilation = 1.0f,
                               .deepening = 2.0f})},
      {"phosphor bloom", Filter::phosphorBloom(9)},
      {"a program sampling the layer",
       Filter::of(sigil::material::field::ripple(10, 60), 12)},
      {"a bloom then a program",
       Filter::bloom({.sigma = 2.5f, .spread = 2.2f})
           .then(Filter::of(sigil::material::field::ripple(4, 90), 6))},
  };
  for (const Cache cache : {Cache::None, Cache::Texture})
    for (const float degrees : {0.0f, 7.0f})
      for (const float scale : {1.0f, 2.0f})
        for (const auto& [name, filter] : filters) {
          const SkBitmap narrow = panelOnGpu(filter, 1, scale, cache, degrees);
          ASSERT_FALSE(narrow.empty());
          for (const float bleed : {4.2426f, 10.0f}) {
            const SkBitmap wide =
                panelOnGpu(filter, bleed, scale, cache, degrees);
            ASSERT_FALSE(wide.empty());
            EXPECT_EQ(mismatchedPixels(narrow, wide), 0u)
                << name << (cache == Cache::Texture ? ", baked" : ", live")
                << ", turned " << degrees << " degrees, at scale " << scale
                << ": another picture over a keyline stating a bleed of "
                << bleed << " than over one stating a bleed of one";
          }
        }
}
