/** @file
 * Typographic options: last-line alignment, soft hyphens,
 * span restyles, paint layers/effects, and OpenType features.
 */

#include "TestSupport.h"

#include <gtest/gtest.h>

#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkGradient.h>

#include <algorithm>
using namespace textflow;
using namespace textflow::test;

// ── Typographic options (last-line alignment, hyphenation, effects) ───────

TEST(Typography, LastLineAlignmentEnd) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      u8"a justified paragraph whose final line is pushed to the right edge "
      "instead of hanging on the left like usual short last lines do");
  BlockFlow flow(SkRect::MakeWH(260, 600));
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  options.justification.lastLineAlignment = TextAlignment::kEnd;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  ASSERT_GT(layout.lineCount, 1);

  // The last line's last word must end at the right edge, and its first run
  // must not start at x=0.
  float lastLineEnd = 0, lastLineStart = 1e9f;
  for (const PositionedRun &run : layout.runs) {
    if (run.lineIndex != layout.lineCount - 1)
      continue;
    lastLineEnd = std::max(lastLineEnd, runEnd(paragraph, run));
    lastLineStart = std::min(lastLineStart, run.origin.x());
  }
  EXPECT_NEAR(lastLineEnd, 260.0f, 1.0f);
  EXPECT_GT(lastLineStart, 5.0f);
}

// ── Soft hyphens (grouped: the three faces of the discretionary break) ───

TEST(SoftHyphen, RendersHyphenOnBreak) {
  FontContext &fontContext = sharedContext();
  // "extra­ordinarily" fits neither whole nor as "extra" without the
  // discretionary break being taken on a narrow measure.
  Paragraph paragraph = makeParagraph(u8"an extra­ordinarily narrow measure");
  BlockFlow flow(SkRect::MakeWH(90, 300));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  const Word *hyphenWord = nullptr;
  for (const Word &word : paragraph.words())
    if (word.hyphenBreak)
      hyphenWord = &word;
  ASSERT_NE(hyphenWord, nullptr);
  ASSERT_TRUE(hyphenWord->hyphenGlyph);

  // The hyphen glyph's shared blob must appear among the placed runs.
  const SkTextBlob *hyphenBlob = wordBlob(*hyphenWord->hyphenGlyph).get();
  bool found = false;
  for (const PositionedRun &run : layout.runs)
    found |= run.blob.get() == hyphenBlob;
  EXPECT_TRUE(found);
}

TEST(SoftHyphen, InvisibleWhenNotBroken) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"extra­ordinarily");
  BlockFlow flow(SkRect::MakeWH(500, 100)); // plenty of room: no break
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  ASSERT_EQ(paragraph.words().size(), 2u); // "extra·" + "ordinarily"
  const Word &first = paragraph.words()[0];
  EXPECT_TRUE(first.hyphenBreak);
  EXPECT_EQ(first.spaceWidth, 0.0f); // halves join with zero gap
  const SkTextBlob *hyphenBlob = wordBlob(*first.hyphenGlyph).get();
  for (const PositionedRun &run : layout.runs)
    EXPECT_NE(run.blob.get(), hyphenBlob) << "hyphen rendered without break";
  // Both halves sit on one line, adjacent.
  ASSERT_EQ(layout.lineCount, 1);
}

TEST(SoftHyphen, KnuthPlassTakesDiscretionaryBreaks) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph =
      makeParagraph(u8"the as­ton­ish­ing­ly in­com­pre­hen"
                    "­si­ble hy­phen­ation ma­chin­ery works");
  BlockFlow flow(SkRect::MakeWH(120, 600));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  options.alignment = TextAlignment::kJustify;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  EXPECT_FALSE(layout.overflowed());
  EXPECT_GT(layout.lineCount, 2);
}

TEST(Typography, SpanRestyleAcrossLines) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      u8"a long sentence that will certainly wrap across several lines gets "
      "one continuous span of emphasis applied to its middle third and the "
      "styling must follow the words wherever the line breaker puts them");
  BlockFlow flow(SkRect::MakeWH(220, 600));
  ParagraphLayout before = layoutParagraph(fontContext, paragraph, flow);
  ASSERT_GT(before.lineCount, 3);

  fontContext.resetStats();
  // Style the middle third, snapped to word boundaries. (A range cutting
  // *inside* a word splits that word into fragments and costs a one-time
  // reshape of the two boundary words; whole-word ranges cost zero.)
  const std::u16string &text = paragraph.text();
  uint32_t from = static_cast<uint32_t>(text.find(u' ', text.size() / 3)) + 1;
  const uint32_t rangeEnd =
      static_cast<uint32_t>(text.find(u' ', 2 * text.size() / 3));
  paragraph.setPaint(from, rangeEnd, PaintStyle{SK_ColorRED});
  ParagraphLayout after = layoutParagraph(fontContext, paragraph, flow);
  EXPECT_EQ(fontContext.stats().shapeCalls, 0u);

  // Red runs must exist on more than one line.
  const auto &spans = paragraph.spans();
  int firstRedLine = -1, lastRedLine = -1;
  for (const PositionedRun &run : after.runs) {
    if (run.styleIndex < spans.size() &&
        spans[run.styleIndex].style.paint.foreground.getColor() ==
            SK_ColorRED) {
      if (firstRedLine < 0)
        firstRedLine = run.lineIndex;
      lastRedLine = run.lineIndex;
    }
  }
  ASSERT_GE(firstRedLine, 0);
  EXPECT_GT(lastRedLine, firstRedLine);
}

TEST(Typography, ShadowAndShaderDrawWithoutRelayout) {
  FontContext &fontContext = sharedContext();
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
  layout.draw(surface->getCanvas(), paragraph); // same layout object, new paint
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

TEST(Typography, PaintLayersExposeCompletePaintAndExplicitOrder) {
  PaintStyle style(SK_ColorWHITE);
  style.addUnderlay(PaintLayer::dropShadow(0x66000000, {3, 4}, 2.0f))
      .addUnderlay(PaintLayer::glow(0x550000FF, 5.0f))
      .addUnderlay(PaintLayer::outline(SK_ColorBLACK, 3.0f));

  SkPaint customOverlay;
  customOverlay.setAntiAlias(true);
  customOverlay.setColor(SK_ColorGREEN);
  customOverlay.setStyle(SkPaint::kStroke_Style);
  customOverlay.setStrokeWidth(1.0f);
  customOverlay.setBlendMode(SkBlendMode::kScreen);
  style.addOverlay(PaintLayer(customOverlay, {-1, -1}));

  ASSERT_EQ(style.underlays.size(), 3u);
  EXPECT_EQ(style.underlays[0].offset, (SkVector{3, 4}));
  EXPECT_NE(style.underlays[0].paint.getMaskFilter(), nullptr);
  EXPECT_NE(style.underlays[1].paint.getMaskFilter(), nullptr);
  EXPECT_EQ(style.underlays[2].paint.getStyle(), SkPaint::kStroke_Style);
  EXPECT_FLOAT_EQ(style.underlays[2].paint.getStrokeWidth(), 3.0f);
  ASSERT_EQ(style.overlays.size(), 1u);
  EXPECT_EQ(style.overlays[0].paint.getBlendMode_or(SkBlendMode::kSrcOver),
            SkBlendMode::kScreen);

  PaintStyle identical = style;
  EXPECT_EQ(identical, style);
  identical.overlays[0].offset.set(0, 0);
  EXPECT_FALSE(identical == style);
}
// ── OpenType features ─────────────────────────────────────────────────────

TEST(Features, LigatureToggleChangesGlyphCount) {
  FontContext &fontContext = sharedContext();
  sk_sp<SkTypeface> hoefler = fontContext.fontManager()->matchFamilyStyle(
      "Hoefler Text", SkFontStyle());
  if (!hoefler)
    GTEST_SKIP() << "Hoefler Text not installed";

  TextStyle ligaturesEnabledStyle = basicStyle();
  ligaturesEnabledStyle.shaping.typeface = hoefler;
  TextStyle ligaturesDisabledStyle = ligaturesEnabledStyle;
  ligaturesDisabledStyle.shaping.fontFeatures.push_back({"liga", 0});
  ligaturesDisabledStyle.shaping.fontFeatures.push_back({"clig", 0});

  Paragraph ligaturesEnabledParagraph;
  Paragraph ligaturesDisabledParagraph;
  ligaturesEnabledParagraph.appendText(u8"official", ligaturesEnabledStyle);
  ligaturesDisabledParagraph.appendText(u8"official", ligaturesDisabledStyle);
  ligaturesEnabledParagraph.ensureShaped(fontContext);
  ligaturesDisabledParagraph.ensureShaped(fontContext);
  const size_t enabledGlyphCount =
      ligaturesEnabledParagraph.words()[0].segments[0].shaped->glyphs.size();
  const size_t disabledGlyphCount =
      ligaturesDisabledParagraph.words()[0].segments[0].shaped->glyphs.size();
  EXPECT_LT(enabledGlyphCount, disabledGlyphCount)
      << "'ffi' must ligate when liga is on";
  // Features are part of the shape-cache key: both variants coexist.
  EXPECT_NE(ligaturesEnabledParagraph.words()[0].segments[0].shaped.get(),
            ligaturesDisabledParagraph.words()[0].segments[0].shaped.get());
}
