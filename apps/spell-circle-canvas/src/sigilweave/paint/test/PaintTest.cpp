/** @file
 * Drawing a finished layout: paint layers and shaders take effect without a
 * relayout, a pass carrying a material shades through the installed
 * resolver and draws with its paint alone without one, the free draws are
 * the members, and placeholder rects and selection bands draw where the
 * layout says they landed.
 */

#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkShader.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkGradient.h>
#include <sigilmaterial/kit/TextPaint.h>
#include <sigilweave/paint/Paint.h>
#include <sigilweave/shaders/PaintShaders.h>

#include <memory>
#include <vector>

#include "support/Fonts.h"
#include "support/Layouts.h"
#include "support/Paragraphs.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

TEST(Typography, ShadowAndShaderDrawWithoutRelayout) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"effects are paint-only");
  BlockFlow flow(SkRect::MakeWH(400, 100));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  fontContext.resetStats();
  PaintStyle fancy(SK_ColorWHITE);
  fancy.addUnderlay(PaintLayer::dropShadow(0x80000000, {3, 3}, 2.5f));
  const SkPoint gradientPoints[2] = {{0, 0}, {180, 0}};
  const SkColor4f gradientColors[2] = {SkColor4f::FromColor(SK_ColorRED),
                                       SkColor4f::FromColor(SK_ColorBLUE)};
  fancy.foreground.setShader(SkShaders::LinearGradient(
      gradientPoints,
      SkGradient(SkGradient::Colors({gradientColors, 2}, SkTileMode::kClamp),
                 SkGradient::Interpolation())));
  paragraph.setPaint(0, 7, fancy);

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 100));
  surface->getCanvas()->clear(SK_ColorTRANSPARENT);
  layout.draw(surface->getCanvas(),
              paragraph);  // same layout object, new paint
  EXPECT_EQ(fontContext.stats().shapeCalls, 0u);

  // The shadow must have put ink outside the pure-white fill: sample any
  // non-white, non-transparent pixel.
  SkPixmap pixmap;
  ASSERT_TRUE(surface->peekPixels(&pixmap));
  bool sawShadowInk = false;
  for (int pixelY = 0; pixelY < pixmap.height() && !sawShadowInk; ++pixelY)
    for (int pixelX = 0; pixelX < pixmap.width() && !sawShadowInk; ++pixelX) {
      const SkColor pixelColor = pixmap.getColor(pixelX, pixelY);
      if (SkColorGetA(pixelColor) > 0 && pixelColor != SK_ColorWHITE)
        sawShadowInk = true;
    }
  EXPECT_TRUE(sawShadowInk);
}

TEST(LineMetricsQuery, PlaceholdersAndSelectionBands) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph;
  paragraph.appendText(u8"pill ", basicStyle(14.0f));
  paragraph.appendPlaceholder({60, 50, /*baselineDrop=*/10}, basicStyle(14.0f));
  BlockFlow flow(SkRect::MakeWH(600, 120));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  const std::vector<LineMetrics> lines = layout.lineMetrics(paragraph);
  ASSERT_EQ(lines.size(), 1u);
  // The 50px-tall slot dropped 10px below baseline stretches the band on
  // both sides beyond the 14px text metrics.
  EXPECT_GE(lines[0].ascent, 40.0f);
  EXPECT_GE(lines[0].descent, 10.0f);

  // The headline use case: a selection band behind a whole line is just
  // rect() painted before draw() — verify it covers the placed content.
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

TEST(Typography, MaterialPassShadesThroughTheInstalledResolver) {
  FontContext& fontContext = sharedContext();
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
    std::pair<int, int> counts{0, 0};
    for (int y = 0; y < pixmap.height(); ++y)
      for (int x = 0; x < pixmap.width(); ++x) {
        const SkColor c = pixmap.getColor(x, y);
        if (SkColorGetA(c) == 0) continue;
        ++counts.first;
        if (SkColorGetR(c) != SkColorGetG(c) ||
            SkColorGetG(c) != SkColorGetB(c))
          ++counts.second;
      }
    return counts;
  };

  paint::setMaterialResolver({});
  EXPECT_FALSE(paint::hasMaterialResolver());
  for (bool batched : {false, true}) {
    const auto [inked, coloured] = render(batched);
    EXPECT_GT(inked, 0);
    EXPECT_EQ(coloured, 0) << "without a resolver the pass draws its paint";
  }

  PaintShaders::installMaterialResolver();
  EXPECT_TRUE(paint::hasMaterialResolver());
  for (bool batched : {false, true}) {
    const auto [inked, coloured] = render(batched);
    EXPECT_GT(inked, 0);
    EXPECT_GT(coloured, 0) << "with a resolver the pass shades its material";
  }
  paint::setMaterialResolver({});
}
