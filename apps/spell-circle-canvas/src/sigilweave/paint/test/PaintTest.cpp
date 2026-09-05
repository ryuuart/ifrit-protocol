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
#include <sigilweave/paint/Paint.h>

#include <memory>
#include <utility>
#include <vector>

#include "support/Faces.h"

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

TEST(PaintPasses, ShadowAndShaderDrawWithoutRelayout) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"effects are paint-only");
  BlockFlow flow(SkRect::MakeWH(400, 100));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  PaintStyle fancy(SK_ColorWHITE);
  fancy.addUnderlay(PaintLayer::dropShadow(0x80000000, {3, 3}, 2.5f));
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
  sigil::material::skia::install();
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
  sigil::material::skia::install();
  EXPECT_NE(sigil::material::skia::shader(GetParam().build(bounds, 1.25f), {}),
            nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    Presets, TextPaintPresets,
    ::testing::Values(TextPaintPreset{"Water", sigil::material::kit::water},
                      TextPaintPreset{"MeshGradient",
                                      sigil::material::kit::meshGradient},
                      TextPaintPreset{"Sparkle",
                                      sigil::material::kit::sparkle}),
    [](const ::testing::TestParamInfo<TextPaintPreset>& info) {
      return std::string(info.param.name);
    });
