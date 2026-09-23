/** @file
 * Drawing a finished layout: paint layers and shaders take effect without
 * a relayout, a selection band drawn behind a line covers what the layout
 * said the line covers, a pass carrying a material shades through the
 * installed resolver and draws with its paint alone without one, and every
 * preset text paint resolves to a shader over the bounds it is given. What
 * one of them looks like on a page of type is a picture, and a picture is
 * the plate ledger's to judge rather than an assertion's.
 */

#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkShader.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkGradient.h>
#include <sigilmaterial/kit/TextPaint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilweave/kit/PaintLayers.h>
#include <sigilweave/paint/Paint.h>

#include <memory>
#include <utility>
#include <vector>

#include "GlyphCanvas.h"
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

}  // namespace

namespace {

enum class LineFit { Letters, Glyphs };
class FittedPaint : public ::testing::TestWithParam<LineFit> {};

}  // namespace

TEST_P(FittedPaint, BatchedDrawingKeepsThePositionsAndScaleOfTheFittedBlobs) {
  Paragraph paragraph = makeParagraph(u8"Alone.\nAlone.", 20.0f);
  JustificationOptions fit;
  fit.justifyLastLine = true;
  if (GetParam() == LineFit::Letters) {
    fit.singleWord = JustificationOptions::SingleWord::kJustify;
  } else {
    fit.glyphScale = fit.glyphScaleMinimum = fit.glyphScaleMaximum = 0.6f;
  }
  ParagraphLayoutOptions options;
  options.blocks = {
      {.alignment = TextAlignment::kJustify, .justification = fit},
      {.alignment = TextAlignment::kStart}};
  BlockFlow flow(SkRect::MakeWH(200, 100));
  const ParagraphLayout layout =
      layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
  ASSERT_EQ(layout.runs.size(), 2u);
  ASSERT_FALSE(layout.runs.front().fit.plain());
  ASSERT_TRUE(layout.runs.back().fit.plain());

  sigil::test::GlyphCanvas direct(240, 100), batched(240, 100);
  layout.draw(&direct, paragraph);
  layout.drawBatched(&batched, paragraph);
  ASSERT_FALSE(direct.glyphs.empty());
  ASSERT_EQ(batched.glyphs.size(), direct.glyphs.size());
  for (size_t index = 0; index < direct.glyphs.size(); ++index) {
    SCOPED_TRACE(index);
    const auto& expected = direct.glyphs[index];
    const auto& actual = batched.glyphs[index];
    EXPECT_EQ(actual.glyph, expected.glyph);
    EXPECT_FLOAT_EQ(actual.position.x(), expected.position.x());
    EXPECT_FLOAT_EQ(actual.position.y(), expected.position.y());
    EXPECT_EQ(actual.font, expected.font);
  }
}

INSTANTIATE_TEST_SUITE_P(Justification, FittedPaint,
                         ::testing::Values(LineFit::Letters, LineFit::Glyphs),
                         [](const ::testing::TestParamInfo<LineFit>& row) {
                           return row.param == LineFit::Letters
                                      ? "LetterSpacing"
                                      : "GlyphScale";
                         });

TEST(PaintPasses, ShadowAndShaderDrawWithoutRelayout) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"effects are paint-only");
  BlockFlow flow(SkRect::MakeWH(400, 100));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  PaintStyle fancy(SK_ColorWHITE);
  fancy.addUnderlay(sigil::weave::kit::dropShadow(0x80000000, {3, 3}, 2.5f));
  fancy.foreground.setShader(
      horizontalGradient(0, 180, SK_ColorRED, SK_ColorBLUE));
  paragraph.setPaint(0, 7, fancy);

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 100));
  surface->getCanvas()->clear(SK_ColorTRANSPARENT);
  layout.draw(surface->getCanvas(),
              paragraph);  // same layout object, new paint

  // The shadow must have put ink outside the pure-white fill: sample any
  // non-white, non-transparent pixel.
  SkPixmap pixmap;
  ASSERT_TRUE(surface->peekPixels(&pixmap));
  EXPECT_TRUE(anyPixel(pixmap, [](SkColor color) {
    return SkColorGetA(color) > 0 && color != SK_ColorWHITE;
  })) << "the shadow pass put no ink outside the fill";
}

TEST(PaintPasses, APassWhoseColourIsTransparentDrawsInTheForegrounds) {
  // An underlay stated with no colour of its own — transparent — is a
  // copy of the glyphs in the text's colour on the pass's own offset: red
  // ink lands where only the offset copy reaches. Black stated outright
  // stays black.
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"halo");
  BlockFlow flow(SkRect::MakeWH(200, 60));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
  const auto inkOf = [&](SkColor layerColour) {
    PaintStyle style(SK_ColorRED);
    style.addUnderlay(PaintLayer(layerColour, {0, 120}));
    paragraph.setPaint(0, 4, style);
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 200));
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    layout.draw(surface->getCanvas(), paragraph);
    SkPixmap pixmap;
    EXPECT_TRUE(surface->peekPixels(&pixmap));
    int red = 0, black = 0;
    for (int y = 100; y < 200; ++y)
      for (int x = 0; x < 200; ++x) {
        const SkColor c = pixmap.getColor(x, y);
        if (SkColorGetA(c) == 0) continue;
        if (SkColorGetR(c) > 200 && SkColorGetG(c) < 60) ++red;
        if (SkColorGetR(c) < 60 && SkColorGetG(c) < 60 && SkColorGetB(c) < 60)
          ++black;
      }
    return std::pair{red, black};
  };
  const auto [redUnset, blackUnset] = inkOf(SK_ColorTRANSPARENT);
  EXPECT_GT(redUnset, 0) << "the transparent pass took the foreground's red";
  EXPECT_EQ(blackUnset, 0);
  const auto [redBlack, blackBlack] = inkOf(SK_ColorBLACK);
  EXPECT_EQ(redBlack, 0);
  EXPECT_GT(blackBlack, 0) << "a colour stated outright is kept";
}

TEST(PaintPasses, ASelectionBandBehindALineCoversItsInterior) {
  // The headline use case: a band behind a whole line is the line's own
  // rect() painted before draw().
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph;
  paragraph.appendText(u8"pill ", basicStyle(14.0f));
  paragraph.appendPlaceholder({60, 50, /*baselineDrop=*/10}, basicStyle(14.0f));
  BlockFlow flow(SkRect::MakeWH(600, 120));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  const std::vector<LineMetrics> lines = layout.lineMetrics(paragraph);
  ASSERT_EQ(lines.size(), 1u);

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(600, 120));
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SK_ColorWHITE);
  SkPaint selection;
  selection.setColor(0x5533AAFF);
  canvas->drawRect(lines[0].rect(), selection);
  layout.draw(canvas, paragraph);
  SkPixmap pixmap;
  ASSERT_TRUE(surface->peekPixels(&pixmap));
  const int probeX = static_cast<int>((lines[0].left + lines[0].right) / 2);
  const int probeY = static_cast<int>(lines[0].baseline - 2);
  EXPECT_NE(pixmap.getColor(probeX, probeY), SK_ColorWHITE)
      << "selection band must cover the line interior";
}

TEST(PaintPasses, MaterialPassShadesThroughTheInstalledResolver) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"material pass");
  BlockFlow flow(SkRect::MakeWH(300, 80));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  // A white pass: on its own it inks pure white; with a material and a
  // resolver its shader replaces the colour and the ink is the material's.
  PaintLayer pass(SK_ColorWHITE);
  pass.material = std::make_shared<const sigil::material::Material>(
      sigil::material::kit::meshGradient(SkRect::MakeWH(300, 80), 0.0f));
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

TEST(PaintPasses, AStylePerGlyphDrawsEachGlyphInTheStyleItNames) {
  // Two glyphs, two styles: the first glyph red, the second blue, and a
  // shadow under each that lands beneath BOTH foregrounds — the passes draw
  // band by band, so the second glyph's shadow never covers the first
  // glyph's fill. The glyphs land exactly where the plain draw puts them.
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"HH", 60.0f);
  BlockFlow flow(SkRect::MakeWH(200, 100));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  PaintStyle red(SK_ColorRED), blue(SK_ColorBLUE);
  red.addUnderlay(PaintLayer(SK_ColorGREEN, {-40, 0}));
  blue.addUnderlay(PaintLayer(SK_ColorGREEN, {-40, 0}));
  const std::vector<PaintStyle> styles = {red, blue};
  const std::vector<uint32_t> styleOfGlyph = {0, 1};
  const ParagraphLayout::GlyphStyles perGlyph{styleOfGlyph, styles};

  sigil::test::GlyphCanvas plain(200, 100), styled(200, 100);
  layout.drawBatched(&plain, paragraph);
  layout.drawBatched(&styled, paragraph, perGlyph);
  ASSERT_EQ(plain.glyphs.size(), 2u);
  // Each glyph once per pass: the two shadows, then the two fills.
  ASSERT_EQ(styled.glyphs.size(), 4u);
  for (size_t index = 0; index < 2; ++index) {
    EXPECT_EQ(styled.glyphs[2 + index].glyph, plain.glyphs[index].glyph);
    EXPECT_FLOAT_EQ(styled.glyphs[2 + index].position.x(),
                    plain.glyphs[index].position.x());
  }

  // Red and blue ink either side of where the second glyph begins.
  const float split = plain.glyphs[1].position.x();
  struct Ink {
    int redLeft = 0, redRight = 0, blueRight = 0;
  };
  const auto inkOf = [&](const std::vector<PaintStyle>& drawn) {
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 100));
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    layout.drawBatched(surface->getCanvas(), paragraph,
                       ParagraphLayout::GlyphStyles{styleOfGlyph, drawn});
    SkPixmap pixmap;
    EXPECT_TRUE(surface->peekPixels(&pixmap));
    Ink ink;
    for (int y = 0; y < 100; ++y)
      for (int x = 0; x < 200; ++x) {
        const SkColor c = pixmap.getColor(x, y);
        const bool left = (float)x < split;
        if (SkColorGetR(c) > 200 && SkColorGetB(c) < 60 && SkColorGetG(c) < 60)
          (left ? ink.redLeft : ink.redRight)++;
        if (SkColorGetB(c) > 200 && SkColorGetR(c) < 60 &&
            SkColorGetG(c) < 60 && !left)
          ink.blueRight++;
      }
    return ink;
  };
  const Ink shadowed = inkOf(styles);
  EXPECT_GT(shadowed.redLeft, 50) << "the first glyph took the first style";
  EXPECT_GT(shadowed.blueRight, 50) << "the second took the second style";
  EXPECT_EQ(shadowed.redRight, 0);
  const Ink bare = inkOf({PaintStyle(SK_ColorRED), PaintStyle(SK_ColorBLUE)});
  EXPECT_EQ(shadowed.redLeft, bare.redLeft)
      << "the second glyph's shadow covered the first glyph's fill";
}

// ── The preset text paints a paint style can carry ───────────────────────

namespace {

/// One preset, named by the word a caller spells it with.
struct TextPaintPreset {
  const char* name;
  sigil::material::Material (*build)(const SkRect&, float);
};

class TextPaintPresets : public ::testing::TestWithParam<TextPaintPreset> {};

}  // namespace

TEST_P(TextPaintPresets, EachResolvesToAShaderOverTheBoundsItIsGiven) {
  const SkRect bounds = SkRect::MakeXYWH(10, 10, 1180, 880);
  EXPECT_NE(sigil::material::skia::shader(GetParam().build(bounds, 1.25f), {}),
            nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    Presets, TextPaintPresets,
    ::testing::Values(
        TextPaintPreset{"Water", sigil::material::kit::water},
        TextPaintPreset{"MeshGradient", sigil::material::kit::meshGradient},
        TextPaintPreset{"Sparkle", sigil::material::kit::sparkle}),
    [](const ::testing::TestParamInfo<TextPaintPreset>& info) {
      return std::string(info.param.name);
    });
