/** @file
 * A large paragraph painted through runtime shaders: every word of a
 * two-thousand-word paragraph reaches the surface.
 */

#include <gtest/gtest.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkShader.h>
#include <include/core/SkSurface.h>
#include <sigilweave/shaders/PaintShaders.h>

#include <string>
#include <utility>

#include "support/LayoutSupport.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

TEST(Stress, RuntimeShadersRenderEntire2000WordParagraph) {
  constexpr int kWordCount = 2000;
  const char8_t* words[] = {u8"letters", u8"water", u8"stars", u8"flow",
                            u8"cached",  u8"paint", u8"文字",  u8"波紋",
                            u8"글자",    u8"星光"};
  std::u8string text;
  for (int wordIndex = 0; wordIndex < kWordCount; ++wordIndex) {
    text += words[(wordIndex * 7) % std::size(words)];
    text += ' ';
  }

  const SkRect bounds = SkRect::MakeXYWH(10, 10, 1180, 880);
  sk_sp<SkShader> water = PaintShaders::water(bounds, 1.25f);
  sk_sp<SkShader> mesh = PaintShaders::meshGradient(bounds, 1.25f);
  sk_sp<SkShader> stars = PaintShaders::sparkle(bounds, 1.25f);
  ASSERT_NE(water, nullptr);
  ASSERT_NE(mesh, nullptr);
  ASSERT_NE(stars, nullptr);

  TextStyle textStyle = basicStyle(8.0f);
  textStyle.paint = PaintStyle(SK_ColorWHITE);
  textStyle.paint.addUnderlay(PaintLayer::glow(0x772A77FF, 1.8f))
      .addUnderlay(PaintLayer::outline(0xFF061229, 0.7f));
  textStyle.paint.foreground.setShader(std::move(mesh));
  SkPaint starOverlay;
  starOverlay.setAntiAlias(true);
  starOverlay.setShader(std::move(stars));
  starOverlay.setBlendMode(SkBlendMode::kScreen);
  textStyle.paint.addOverlay(PaintLayer(std::move(starOverlay)));

  FontContext& fontContext = sharedContext();
  Paragraph paragraph;
  paragraph.appendText(text, textStyle);
  BlockFlow flow(bounds);
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  options.lineMetrics.height = 10.0f;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  ASSERT_FALSE(layout.overflowed()) << "the stress paragraph must be whole";

  size_t glyphCount = 0;
  for (const PositionedRun& run : layout.runs)
    if (run.shaped) glyphCount += run.shaped->glyphs.size();
  EXPECT_GT(glyphCount, 7000u);

  constexpr SkColor kBackground = 0xFF050A18;
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1200, 900));
  surface->getCanvas()->clear(kBackground);
  layout.drawBatched(surface->getCanvas(), paragraph);
  SkPixmap pixels;
  ASSERT_TRUE(surface->peekPixels(&pixels));
  bool sawInk = false;
  for (int y = 0; y < pixels.height() && !sawInk; y += 3)
    for (int x = 0; x < pixels.width() && !sawInk; x += 3)
      sawInk = pixels.getColor(x, y) != kBackground;
  EXPECT_TRUE(sawInk);
}
