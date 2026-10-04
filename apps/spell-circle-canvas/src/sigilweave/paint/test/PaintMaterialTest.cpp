/** @file
 * A pass carrying a material: it shades through the installed resolver
 * over the glyph ink bounds of what it covers, keeps every other setting
 * of its paint, draws with its paint alone where no resolver or no shader
 * answers, keeps each span's own source, and releases what it borrowed
 * when a resolver throws. A resolver may draw a paragraph of its own on
 * the same thread without disturbing the draw that called it. All three
 * draws of a finished layout answer alike.
 */

#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkRSXform.h>
#include <include/core/SkShader.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTextBlob.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilweave/fonts/Shaper.h>
#include <sigilweave/paint/Paint.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "GlyphCanvas.h"
#include "TextFields.h"
#include "sigilgeometry/advanced/Skia.h"
#include "support/Faces.h"
#include "support/Layouts.h"
#include "support/Paints.h"
#include "support/Paragraphs.h"
#include "support/Pixels.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

namespace {

/// The resolver is process-wide, so a case that installs one puts it back
/// however it leaves.
class InstalledResolver {
 public:
  explicit InstalledResolver(paint::MaterialResolver resolver) {
    paint::setMaterialResolver(std::move(resolver));
  }
  InstalledResolver(const InstalledResolver&) = delete;
  InstalledResolver& operator=(const InstalledResolver&) = delete;
  ~InstalledResolver() { paint::setMaterialResolver({}); }
};

/// Two renders of one size agree pixel for pixel, and the first inked
/// something, so two empty pictures cannot pass for a match.
void expectSamePixels(const sk_sp<SkSurface>& actual,
                      const sk_sp<SkSurface>& expected) {
  ASSERT_TRUE(actual);
  ASSERT_TRUE(expected);
  SkPixmap actualPixels, expectedPixels;
  ASSERT_TRUE(actual->peekPixels(&actualPixels));
  ASSERT_TRUE(expected->peekPixels(&expectedPixels));
  EXPECT_EQ(sigil::media::difference(actualPixels, expectedPixels).worst, 0);
  EXPECT_TRUE(anyPixel(actualPixels,
                       [](SkColor color) { return SkColorGetA(color) != 0; }));
}

/// The three paths a finished layout draws by.
enum class ForegroundDraw { Blobs, Batches, GlyphStyles };

std::string drawPathName(const testing::TestParamInfo<ForegroundDraw>& test) {
  switch (test.param) {
    case ForegroundDraw::Blobs:
      return "Blobs";
    case ForegroundDraw::Batches:
      return "Batches";
    case ForegroundDraw::GlyphStyles:
      return "GlyphStyles";
  }
  return "Unknown";
}

/// Draws @p layout along @p path. The style-per-glyph path names, for
/// each glyph, the style of the span it was set in, so all three draws
/// put the same ink down.
void drawAlong(ForegroundDraw path, SkCanvas& canvas,
               const ParagraphLayout& layout, const Paragraph& paragraph) {
  if (path == ForegroundDraw::Blobs) {
    layout.draw(&canvas, paragraph);
    return;
  }
  if (path == ForegroundDraw::Batches) {
    layout.drawBatched(&canvas, paragraph);
    return;
  }
  std::vector<PaintStyle> styles;
  std::vector<uint32_t> styleOfGlyph;
  for (const PositionedRun& run : layout.runs) {
    if (!run.shaped) continue;
    styles.push_back(paragraph.spans()[run.styleIndex].style.paint);
    styleOfGlyph.insert(styleOfGlyph.end(), run.shaped->glyphs.size(),
                        static_cast<uint32_t>(styles.size() - 1));
  }
  layout.drawBatched(&canvas, paragraph,
                     ParagraphLayout::GlyphStyles{styleOfGlyph, styles});
}

class ForegroundMaterial : public testing::TestWithParam<ForegroundDraw> {
 protected:
  void SetUp() override {
    paragraph = makeParagraph(u8"HH", 40.0f);
    BlockFlow flow(sigil::geometry::path::Rect::of({20, 8}, {180, 90}));
    layout = layoutParagraph(sigil::test::fonts(), paragraph, flow);
    ASSERT_FALSE(layout.runs.empty());
    ASSERT_TRUE(layout.runs.front().shaped);
    source = std::make_shared<const sigil::material::Material>(
        text_fields::meshGradient(SkRect::MakeWH(200, 100), 0));
  }

  void draw(SkCanvas& canvas) const {
    drawAlong(GetParam(), canvas, layout, paragraph);
  }

  sk_sp<SkSurface> render(const PaintStyle& style) {
    paragraph.setPaint(0, 2, style);
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 100));
    if (!surface) return nullptr;
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    draw(*surface->getCanvas());
    return surface;
  }

  void expectConfiguredFallback() {
    PaintStyle style(SK_ColorWHITE);
    style.foreground.setShader(SkShaders::Color(SK_ColorRED));
    style.foregroundMaterial = source;
    PaintLayer layer(SK_ColorWHITE, {12, 0});
    layer.paint.setShader(SkShaders::Color(SK_ColorBLUE));
    layer.material = source;
    style.addOverlay(layer);
    const auto actual = render(style);
    style.foregroundMaterial.reset();
    style.overlays.front().material.reset();
    expectSamePixels(actual, render(style));
  }

  Paragraph paragraph;
  ParagraphLayout layout;
  std::shared_ptr<const sigil::material::Material> source;
};

}  // namespace

TEST_P(ForegroundMaterial, ResolvedShaderPreservesLayersAndPaintSettings) {
  PaintStyle style(SK_ColorWHITE);
  style.foreground.setShader(SkShaders::Color(SK_ColorBLUE));
  style.foreground.setAlphaf(.5f);
  style.foreground.setStyle(SkPaint::kStroke_Style);
  style.foreground.setStrokeWidth(2);
  style.foregroundMaterial = source;
  style.addUnderlay(PaintLayer(SK_ColorGREEN, {-12, 0}));
  style.addOverlay(PaintLayer(SK_ColorCYAN, {12, 0}));
  int calls = 0;
  const InstalledResolver installed(
      [&](const sigil::material::Material& material, const SkRect& bounds) {
        ++calls;
        EXPECT_EQ(&material, source.get());
        EXPECT_GT(bounds.width(), 0);
        EXPECT_GT(bounds.height(), 0);
        return SkShaders::Color(SK_ColorRED);
      });
  const auto actual = render(style);
  EXPECT_GT(calls, 0);
  style.foregroundMaterial.reset();
  style.foreground.setShader(SkShaders::Color(SK_ColorRED));
  expectSamePixels(actual, render(style));
}

TEST_P(ForegroundMaterial, GlyphBoundsCoverTheMaterialInEveryPaintBand) {
  struct Sample {
    std::u8string_view text;
    float scale;
  };
  for (const Sample sample : {Sample{u8"H", .6f},
                              {u8"H", 1.6f},
                              {u8"HH", 1},
                              {u8"jH", 1},
                              {u8"H", 1}}) {
    SCOPED_TRACE(sample.scale);
    SCOPED_TRACE(sample.text.size());
    TextStyle text = basicStyle(40);
    text.shaping.scaleX = sample.scale;
    paragraph = paragraphIn(sample.text, text);
    BlockFlow flow(sigil::geometry::path::Rect::of({20, 8}, {180, 90}));
    layout = layoutParagraph(sigil::test::fonts(), paragraph, flow);
    ASSERT_EQ(layout.runs.size(), 1u);
    ASSERT_TRUE(layout.runs.front().blob);
    PaintStyle style(SK_ColorWHITE);
    style.foregroundMaterial = source;
    PaintLayer layer(SK_ColorWHITE);
    layer.material = source;
    style.addUnderlay(layer);
    style.addOverlay(layer);
    paragraph.setPaint(0, static_cast<uint32_t>(sample.text.size()), style);

    std::vector<SkRect> resolvedBounds;
    const InstalledResolver installed(
        [&](const sigil::material::Material&, const SkRect& bounds) {
          resolvedBounds.push_back(bounds);
          EXPECT_TRUE(bounds.isFinite());
          EXPECT_GT(bounds.width(), 0);
          EXPECT_GT(bounds.height(), 0);
          return horizontalGradient(bounds.left(), bounds.right(), SK_ColorRED,
                                    SK_ColorBLUE);
        });
    auto actual = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 100));
    ASSERT_TRUE(actual);
    actual->getCanvas()->clear(SK_ColorTRANSPARENT);
    draw(*actual->getCanvas());
    ASSERT_EQ(resolvedBounds.size(), 3u);
    EXPECT_EQ(resolvedBounds[0], resolvedBounds[1]);
    EXPECT_EQ(resolvedBounds[1], resolvedBounds[2]);
    SkPixmap pixels;
    ASSERT_TRUE(actual->peekPixels(&pixels));
    int red = 0, blue = 0;
    const SkRect extent = resolvedBounds.front().makeOutset(1, 1);
    for (int y = 0; y < pixels.height(); ++y) {
      for (int x = 0; x < pixels.width(); ++x) {
        const SkColor color = pixels.getColor(x, y);
        if (SkColorGetA(color) == 0) continue;
        EXPECT_TRUE(extent.contains(x + .5f, y + .5f));
        if (SkColorGetA(color) < 240) continue;
        red += SkColorGetR(color) > 170 && SkColorGetB(color) < 85;
        blue += SkColorGetB(color) > 170 && SkColorGetR(color) < 85;
      }
    }
    EXPECT_GT(red, 0);
    EXPECT_GT(blue, 0);

    auto blobs = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 100));
    ASSERT_TRUE(blobs);
    blobs->getCanvas()->clear(SK_ColorTRANSPARENT);
    layout.draw(blobs->getCanvas(), paragraph);
    expectSamePixels(actual, blobs);
  }
}

TEST_P(ForegroundMaterial, MissingResolverKeepsConfiguredShaders) {
  ASSERT_FALSE(paint::hasMaterialResolver());
  expectConfiguredFallback();
}

TEST_P(ForegroundMaterial, NullResolverKeepsConfiguredShaders) {
  int calls = 0;
  const InstalledResolver installed(
      [&](const sigil::material::Material&, const SkRect&) -> sk_sp<SkShader> {
        ++calls;
        return nullptr;
      });
  expectConfiguredFallback();
  EXPECT_GT(calls, 0);
}

TEST_P(ForegroundMaterial, SourceReplacementUsesExistingLayoutWithoutShaping) {
  const auto replacement = std::make_shared<const sigil::material::Material>(
      text_fields::meshGradient(SkRect::MakeWH(200, 100), 1));
  const InstalledResolver installed(
      [&](const sigil::material::Material& material, const SkRect&) {
        return SkShaders::Color(&material == source.get() ? SK_ColorRED
                                                          : SK_ColorBLUE);
      });
  PaintStyle style(SK_ColorWHITE);
  style.foregroundMaterial = source;
  const auto red = render(style);
  ASSERT_TRUE(red);
  sigil::test::GlyphCanvas originalPositions(200, 100);
  draw(originalPositions);
  const auto* shaped = layout.runs.front().shaped;
  sigil::test::fonts().resetStats();
  style.foregroundMaterial = replacement;
  paragraph.setPaint(0, 2, style);
  paragraph.ensureShaped(sigil::test::fonts());
  EXPECT_EQ(sigil::test::fonts().stats().shapeCalls, 0u);
  EXPECT_EQ(layout.runs.front().shaped, shaped);
  const auto blue = render(style);
  ASSERT_TRUE(blue);
  SkPixmap before, after;
  ASSERT_TRUE(red->peekPixels(&before));
  ASSERT_TRUE(blue->peekPixels(&after));
  EXPECT_TRUE(anyPixel(before, [](SkColor color) {
    return SkColorGetR(color) > 0 && SkColorGetB(color) == 0;
  }));
  EXPECT_TRUE(anyPixel(after, [](SkColor color) {
    return SkColorGetB(color) > 0 && SkColorGetR(color) == 0;
  }));
  sigil::test::GlyphCanvas positions(200, 100);
  draw(positions);
  ASSERT_EQ(positions.glyphs.size(), originalPositions.glyphs.size());
  ASSERT_EQ(positions.glyphs.size(), 2u);
  for (size_t index = 0; index < positions.glyphs.size(); ++index) {
    EXPECT_EQ(positions.glyphs[index].glyph,
              originalPositions.glyphs[index].glyph);
    EXPECT_EQ(positions.glyphs[index].position,
              originalPositions.glyphs[index].position);
  }
  style.foregroundMaterial = source;
  expectSamePixels(render(style), red);
}

TEST_P(ForegroundMaterial, DistinctSourcesKeepTheirSpanOwnership) {
  const auto other = std::make_shared<const sigil::material::Material>(
      text_fields::meshGradient(SkRect::MakeWH(200, 100), 1));
  PaintStyle first(SK_ColorWHITE), second(SK_ColorWHITE);
  first.foregroundMaterial = source;
  second.foregroundMaterial = other;
  paragraph.setPaint(0, 1, first);
  paragraph.setPaint(1, 2, second);
  BlockFlow flow(sigil::geometry::path::Rect::of({20, 8}, {180, 90}));
  layout = layoutParagraph(sigil::test::fonts(), paragraph, flow);
  ASSERT_EQ(paragraph.spans().size(), 2u);
  bool sawFirst = false, sawSecond = false;
  const InstalledResolver installed(
      [&](const sigil::material::Material& material, const SkRect&) {
        sawFirst |= &material == source.get();
        sawSecond |= &material == other.get();
        return SkShaders::Color(&material == source.get() ? SK_ColorRED
                                                          : SK_ColorBLUE);
      });
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 100));
  ASSERT_TRUE(surface);
  surface->getCanvas()->clear(SK_ColorTRANSPARENT);
  draw(*surface->getCanvas());
  EXPECT_TRUE(sawFirst);
  EXPECT_TRUE(sawSecond);
  SkPixmap pixels;
  ASSERT_TRUE(surface->peekPixels(&pixels));
  EXPECT_TRUE(anyPixel(pixels, [](SkColor color) {
    return SkColorGetR(color) > 0 && SkColorGetB(color) == 0;
  }));
  EXPECT_TRUE(anyPixel(pixels, [](SkColor color) {
    return SkColorGetB(color) > 0 && SkColorGetR(color) == 0;
  }));
}

INSTANTIATE_TEST_SUITE_P(DrawPaths, ForegroundMaterial,
                         testing::Values(ForegroundDraw::Blobs,
                                         ForegroundDraw::Batches,
                                         ForegroundDraw::GlyphStyles),
                         drawPathName);

namespace {

class BatchedForegroundLifetime : public testing::TestWithParam<bool> {};

PaintStyle materialStyleWithLayers(const sk_sp<SkShader>& ink) {
  PaintStyle style(SK_ColorWHITE);
  style.foreground.setShader(ink);
  style.foregroundMaterial = std::make_shared<const sigil::material::Material>(
      text_fields::meshGradient(SkRect::MakeWH(200, 100), 0));
  PaintLayer layer(SK_ColorWHITE);
  layer.paint.setShader(ink);
  layer.material = style.foregroundMaterial;
  style.addUnderlay(layer).addOverlay(layer);
  Decoration decoration;
  decoration.paint.emplace();
  decoration.paint->setShader(ink);
  style.addDecoration(decoration);
  return style;
}

}  // namespace

TEST_P(BatchedForegroundLifetime, DrawingReleasesTemporaryMaterialOwners) {
  std::weak_ptr<const sigil::material::Material> released;
  const sk_sp<SkShader> ink = SkShaders::Color(SK_ColorBLUE);
  ASSERT_TRUE(ink->unique());
  bool resolved = false;
  const InstalledResolver installed(
      [&](const sigil::material::Material&, const SkRect&) {
        resolved = true;
        if (GetParam()) throw std::runtime_error("material resolution failed");
        return SkShaders::Color(SK_ColorBLUE);
      });
  {
    PaintStyle style = materialStyleWithLayers(ink);
    released = style.foregroundMaterial;
    Paragraph paragraph = makeParagraph(u8"HH", 40);
    paragraph.setPaint(0, 2, style);
    BlockFlow flow(sigil::geometry::path::Rect::of({0, 0}, {200, 100}));
    const ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow);
    sigil::test::GlyphCanvas canvas(200, 100);
    if (GetParam())
      EXPECT_THROW(layout.drawBatched(&canvas, paragraph), std::runtime_error);
    else
      EXPECT_NO_THROW(layout.drawBatched(&canvas, paragraph));
    EXPECT_TRUE(resolved);
    EXPECT_FALSE(released.expired());
  }
  EXPECT_TRUE(released.expired());
  EXPECT_TRUE(ink->unique());
}

INSTANTIATE_TEST_SUITE_P(ResolvePaths, BatchedForegroundLifetime,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& test) {
                           return test.param ? "Exception" : "Complete";
                         });

namespace {

class PaintScratch : public testing::TestWithParam<bool> {
 protected:
  void draw(SkCanvas& canvas, const Paragraph& paragraph,
            const ParagraphLayout& layout) const {
    drawAlong(
        GetParam() ? ForegroundDraw::GlyphStyles : ForegroundDraw::Batches,
        canvas, layout, paragraph);
  }
};

}  // namespace

TEST_P(PaintScratch, GrowingAndShrinkingBucketsPreservesEveryPaint) {
  for (int groups : {128, 128, 8, 128}) {
    SCOPED_TRACE(groups);
    std::u8string copy;
    for (int index = 0; index < groups; ++index) copy += u8"H ";
    Paragraph paragraph = makeParagraph(copy, 14);
    for (int index = 0; index < groups; ++index) {
      PaintStyle style(SkColorSetARGB(255, index * 37 % 256, index * 53 % 256,
                                      index * 71 % 256));
      style.addUnderlay(PaintLayer(0x804080C0, {1, 1}));
      paragraph.setPaint(index * 2, index * 2 + 1, style);
    }
    BlockFlow flow(sigil::geometry::path::Rect::of({0, 0}, {800, 200}));
    const ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow);
    ASSERT_EQ(glyphCount(layout), groups);
    auto actual = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(800, 200));
    auto expected = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(800, 200));
    ASSERT_TRUE(actual);
    ASSERT_TRUE(expected);
    actual->getCanvas()->clear(SK_ColorTRANSPARENT);
    expected->getCanvas()->clear(SK_ColorTRANSPARENT);
    layout.draw(expected->getCanvas(), paragraph);
    draw(*actual->getCanvas(), paragraph, layout);
    expectSamePixels(actual, expected);
  }
}

TEST_P(PaintScratch, AResolverExceptionReleasesSourcesAndAllowsTheNextDraw) {
  std::weak_ptr<const sigil::material::Material> released;
  const sk_sp<SkShader> ink = SkShaders::Color(SK_ColorBLUE);
  ASSERT_TRUE(ink->unique());
  const InstalledResolver installed(
      [](const sigil::material::Material&, const SkRect&) -> sk_sp<SkShader> {
        throw std::runtime_error("material resolution failed");
      });
  {
    PaintStyle style = materialStyleWithLayers(ink);
    released = style.foregroundMaterial;
    Paragraph paragraph = makeParagraph(u8"HH", 40);
    paragraph.setPaint(0, 2, style);
    BlockFlow flow(sigil::geometry::path::Rect::of({0, 0}, {200, 100}));
    const ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow);
    sigil::test::GlyphCanvas canvas(200, 100);
    EXPECT_THROW(draw(canvas, paragraph, layout), std::runtime_error);
    EXPECT_FALSE(released.expired());
  }
  EXPECT_TRUE(released.expired());
  EXPECT_TRUE(ink->unique());

  Paragraph paragraph = makeParagraph(u8"OK", 40);
  BlockFlow flow(sigil::geometry::path::Rect::of({0, 0}, {200, 100}));
  const ParagraphLayout layout =
      layoutParagraph(sigil::test::fonts(), paragraph, flow);
  auto actual = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 100));
  auto expected = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 100));
  ASSERT_TRUE(actual);
  ASSERT_TRUE(expected);
  actual->getCanvas()->clear(SK_ColorTRANSPARENT);
  expected->getCanvas()->clear(SK_ColorTRANSPARENT);
  EXPECT_NO_THROW(draw(*actual->getCanvas(), paragraph, layout));
  layout.draw(expected->getCanvas(), paragraph);
  expectSamePixels(actual, expected);
}

INSTANTIATE_TEST_SUITE_P(DrawPaths, PaintScratch, testing::Bool(),
                         [](const testing::TestParamInfo<bool>& test) {
                           return test.param ? "GlyphStyles" : "Batches";
                         });

TEST(PaintPasses, MaterialPassShadesThroughTheInstalledResolver) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"material pass");
  BlockFlow flow(sigil::geometry::path::Rect::of({0, 0}, {300, 80}));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  // A white pass: on its own it inks pure white; with a material and a
  // resolver its shader replaces the colour and the ink is the material's.
  PaintLayer pass(SK_ColorWHITE);
  pass.material = std::make_shared<const sigil::material::Material>(
      text_fields::meshGradient(SkRect::MakeWH(300, 80), 0.0f));
  PaintStyle style(SK_ColorTRANSPARENT);
  style.addOverlay(pass);
  paragraph.setPaint(0, 13, style);

  // Inked pixels, and how many of them are not pure white.
  const auto render = [&](bool batched) {
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(300, 80));
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    if (batched)
      paint::drawBatched(surface->getCanvas(), layout, paragraph);
    else
      paint::draw(surface->getCanvas(), layout, paragraph);
    SkPixmap pixmap;
    EXPECT_TRUE(surface->peekPixels(&pixmap));
    const int inked =
        countPixels(pixmap, [](SkColor c) { return SkColorGetA(c) != 0; });
    const int coloured = countPixels(pixmap, [](SkColor c) {
      return SkColorGetA(c) != 0 && (SkColorGetR(c) != SkColorGetG(c) ||
                                     SkColorGetG(c) != SkColorGetB(c));
    });
    return std::pair<int, int>{inked, coloured};
  };

  EXPECT_FALSE(paint::hasMaterialResolver());
  for (bool batched : {false, true}) {
    const auto [inked, coloured] = render(batched);
    EXPECT_GT(inked, 0);
    EXPECT_EQ(coloured, 0) << "without a resolver the pass draws its paint";
  }

  // The resolver a host installs: SigilMaterial's Skia backend, with the
  // pass's bounds as the material's resolution.
  const InstalledResolver installed(
      [](const sigil::material::Material& m, const SkRect& bounds) {
        return sigil::material::skia::shader(
            m, {.resolution = {bounds.width(), bounds.height()}});
      });
  EXPECT_TRUE(paint::hasMaterialResolver());
  for (bool batched : {false, true}) {
    const auto [inked, coloured] = render(batched);
    EXPECT_GT(inked, 0);
    EXPECT_GT(coloured, 0) << "with a resolver the pass shades its material";
  }
}

namespace {

class MixedPaintBands : public testing::TestWithParam<bool> {};

}  // namespace

TEST_P(MixedPaintBands, OverlappingStylesShareBandsAcrossEveryDrawPath) {
  Paragraph paragraph = makeParagraph(u8"HHH", 60);
  const auto source = std::make_shared<const sigil::material::Material>(
      text_fields::meshGradient(SkRect::MakeWH(200, 110), 0));
  const sk_sp<SkShader> redInk = SkShaders::Color(SK_ColorRED);
  const InstalledResolver installed(
      [&](const sigil::material::Material& material, const SkRect&) {
        EXPECT_EQ(&material, source.get());
        return redInk;
      });
  std::vector<PaintStyle> styles = {PaintStyle(SK_ColorWHITE),
                                    PaintStyle(SK_ColorBLUE),
                                    PaintStyle(SK_ColorYELLOW)};
  styles[0].foregroundMaterial = source;
  styles[0].addUnderlay(PaintLayer(SK_ColorGREEN, {40, 0}));
  styles[0].addOverlay(PaintLayer(SK_ColorMAGENTA, {40, 0}));
  styles[0].addOverlay(PaintLayer(SK_ColorCYAN, {40, 0}));
  styles[1].addUnderlay(PaintLayer(SK_ColorBLACK, {-40, 0}));
  styles[1].addUnderlay(PaintLayer(SK_ColorGREEN, {-40, 0}));
  styles[1].addOverlay(PaintLayer(0x80FFFFFF, {40, 0}));
  styles[2].addUnderlay(PaintLayer(SK_ColorBLACK, {-80, 0}));
  styles[2].addOverlay(PaintLayer(0x80FF0000, {-80, 0}));
  for (uint32_t index = 0; index < styles.size(); ++index)
    paragraph.setPaint(index, index + 1, styles[index]);
  BlockFlow flow(sigil::geometry::path::Rect::of({0, 0}, {200, 110}));
  ParagraphLayout layout =
      layoutParagraph(sigil::test::fonts(), paragraph, flow);
  ASSERT_EQ(layout.runs.size(), 3u);
  for (size_t index = 0; index < layout.runs.size(); ++index) {
    ASSERT_TRUE(layout.runs[index].blob);
    ASSERT_TRUE(layout.runs[index].shaped);
    ASSERT_EQ(layout.runs[index].shaped->glyphs.size(), 1u);
    layout.runs[index].origin = {20.0f + 40.0f * index, 80};
  }
  if (GetParam()) {
    PositionedRun& run = layout.runs[1];
    const ShapedWord& word = *run.shaped;
    SkTextBlobBuilder builder;
    const auto& buffer = builder.allocRunRSXform(
        makeFont(word.typeface, word.fontSize, word.scaleX, word.aliased), 1);
    buffer.glyphs[0] = word.glyphs[0];
    buffer.xforms()[0] =
        SkRSXform::MakeFromRadians(1, .12f, run.origin.x, run.origin.y, 0, 0);
    run.blob = builder.make();
    run.origin = {0, 0};
    run.transformed = true;
  }
  const auto surface = [] {
    auto result = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 110));
    result->getCanvas()->clear(SK_ColorTRANSPARENT);
    return result;
  };
  // The reference states the three bands directly, without calling a
  // paragraph painter. A per-run reference proves the overlap is visible.
  const auto reference = [&](bool banded) {
    auto result = surface();
    const auto drawBand = [&](size_t index, int band) {
      const PositionedRun& run = layout.runs[index];
      const PaintStyle& style = styles[index];
      const auto drawPass = [&](const SkPaint& paint, glm::vec2 offset) {
        result->getCanvas()->drawTextBlob(run.blob.get(),
                                          run.origin.x + offset.x,
                                          run.origin.y + offset.y, paint);
      };
      if (band == 1) {
        SkPaint paint = style.foreground;
        if (style.foregroundMaterial) paint.setShader(redInk);
        drawPass(paint, {0, 0});
      } else {
        const auto& layers = band == 0 ? style.underlays : style.overlays;
        for (const PaintLayer& layer : layers)
          drawPass(layer.resolvedPaint(style.foreground), layer.offset);
      }
    };
    if (banded) {
      for (int band = 0; band < 3; ++band)
        for (size_t index = 0; index < layout.runs.size(); ++index)
          drawBand(index, band);
    } else {
      for (size_t index = 0; index < layout.runs.size(); ++index)
        for (int band = 0; band < 3; ++band) drawBand(index, band);
    }
    return result;
  };
  const auto expected = reference(true);
  const auto perRun = reference(false);
  SkPixmap expectedPixels, perRunPixels;
  ASSERT_TRUE(expected->peekPixels(&expectedPixels));
  ASSERT_TRUE(perRun->peekPixels(&perRunPixels));
  EXPECT_GT(
      sigil::media::difference(expectedPixels, perRunPixels).differingPixels,
      100);

  const std::vector<uint32_t> styleOfGlyph = {0, 1, 2};
  for (ForegroundDraw path : {ForegroundDraw::Blobs, ForegroundDraw::Batches,
                              ForegroundDraw::GlyphStyles}) {
    SCOPED_TRACE(static_cast<int>(path));
    auto actual = surface();
    if (path == ForegroundDraw::Blobs)
      layout.draw(actual->getCanvas(), paragraph);
    else if (path == ForegroundDraw::Batches)
      layout.drawBatched(actual->getCanvas(), paragraph);
    else
      layout.drawBatched(actual->getCanvas(), paragraph,
                         ParagraphLayout::GlyphStyles{styleOfGlyph, styles});
    expectSamePixels(actual, expected);
  }
}

INSTANTIATE_TEST_SUITE_P(RunPlacements, MixedPaintBands, testing::Bool(),
                         [](const testing::TestParamInfo<bool>& row) {
                           return row.param ? "TransformedFallback"
                                            : "Horizontal";
                         });

namespace {

class NestedDraw : public testing::TestWithParam<ForegroundDraw> {};

}  // namespace

// A RESOLVER MAY DRAW TYPE OF ITS OWN: a material whose shader is a
// picture of a paragraph draws that paragraph on the same thread while the
// draw that asked is part-way through its bands. The asking draw finishes
// every band it began, exactly as under a resolver that draws nothing.
TEST_P(NestedDraw, AResolverThatDrawsAParagraphLeavesTheOuterDrawWhole) {
  const auto source = std::make_shared<const sigil::material::Material>(
      text_fields::meshGradient(SkRect::MakeWH(200, 100), 0));
  Paragraph outer = makeParagraph(u8"HHH", 40.0f);
  const SkColor colours[] = {SK_ColorWHITE, SK_ColorBLUE, SK_ColorGREEN};
  for (uint32_t index = 0; index < 3; ++index) {
    PaintStyle style(colours[index]);
    if (index == 0) style.foregroundMaterial = source;
    style.addUnderlay(PaintLayer(SK_ColorBLACK, {2, 2}));
    style.addOverlay(PaintLayer(0x80FF00FF, {-2, 0}));
    outer.setPaint(index, index + 1, style);
  }
  BlockFlow outerFlow(sigil::geometry::path::Rect::of({20, 8}, {180, 90}));
  const ParagraphLayout outerLayout =
      layoutParagraph(sigil::test::fonts(), outer, outerFlow);

  // The resolver's own paragraph carries more styles than the outer one,
  // so its buckets and entries outnumber the outer draw's.
  std::u8string copy;
  for (int index = 0; index < 12; ++index) copy += u8"m ";
  Paragraph inner = makeParagraph(copy, 12.0f);
  for (int index = 0; index < 12; ++index) {
    PaintStyle style(SkColorSetARGB(255, index * 20, 255 - index * 20, 128));
    style.addUnderlay(PaintLayer(SK_ColorBLACK, {1, 1}));
    inner.setPaint(index * 2, index * 2 + 1, style);
  }
  BlockFlow innerFlow(sigil::geometry::path::Rect::of({0, 0}, {200, 100}));
  const ParagraphLayout innerLayout =
      layoutParagraph(sigil::test::fonts(), inner, innerFlow);

  const auto render = [&](bool drawsInside) {
    int innerDraws = 0;
    const InstalledResolver installed([&](const sigil::material::Material&,
                                          const SkRect&) -> sk_sp<SkShader> {
      if (drawsInside) {
        auto picture = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 100));
        picture->getCanvas()->clear(SK_ColorTRANSPARENT);
        drawAlong(GetParam(), *picture->getCanvas(), innerLayout, inner);
        ++innerDraws;
      }
      return SkShaders::Color(SK_ColorRED);
    });
    auto result = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 100));
    result->getCanvas()->clear(SK_ColorTRANSPARENT);
    drawAlong(GetParam(), *result->getCanvas(), outerLayout, outer);
    EXPECT_EQ(innerDraws > 0, drawsInside);
    return result;
  };
  const sk_sp<SkSurface> nested = render(true);
  const sk_sp<SkSurface> alone = render(false);
  expectSamePixels(nested, alone);
}

INSTANTIATE_TEST_SUITE_P(DrawPaths, NestedDraw,
                         testing::Values(ForegroundDraw::Blobs,
                                         ForegroundDraw::Batches,
                                         ForegroundDraw::GlyphStyles),
                         drawPathName);
