/** @file
 * Decoration bands resolved without drawing: metrics fill in what the
 * style left at zero, skip-ink cuts a band around descenders, a paint
 * override is applied verbatim, and a highlight defaults to a translucent
 * foreground.
 */

#include <gtest/gtest.h>
#include <include/core/SkBlendMode.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkPaint.h>
#include <include/core/SkShader.h>
#include <sigilweave/decoration/Decoration.h>

#include "support/Fonts.h"
#include "support/Layouts.h"
#include "support/Paragraphs.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

TEST(DecorationTest, BandResolvesFromMetricsWithFloors) {
  SkFontMetrics metrics = {};
  metrics.fFlags = 0;  // face reports no underline/strikeout metrics
  metrics.fAscent = -20.0f;
  metrics.fXHeight = 10.0f;

  const detail::ResolvedDecorationBand underline =
      detail::resolveDecorationBand({}, metrics, SK_ColorRED);
  EXPECT_FLOAT_EQ(underline.thickness, 1.0f) << "1px floor without metrics";
  EXPECT_GT(underline.position, 0.0f) << "underline sits below the baseline";
  EXPECT_EQ(underline.color, SK_ColorRED) << "transparent → foreground color";

  Decoration strike;
  strike.kind = Decoration::Kind::kStrikethrough;
  const detail::ResolvedDecorationBand strikeBand =
      detail::resolveDecorationBand(strike, metrics, SK_ColorRED);
  EXPECT_LT(strikeBand.position, 0.0f) << "strikethrough sits above baseline";

  Decoration overline;
  overline.kind = Decoration::Kind::kOverline;
  overline.color = SK_ColorBLUE;
  const detail::ResolvedDecorationBand overBand =
      detail::resolveDecorationBand(overline, metrics, SK_ColorRED);
  EXPECT_FLOAT_EQ(overBand.position, -20.0f) << "overline rides the ascent";
  EXPECT_EQ(overBand.color, SK_ColorBLUE) << "explicit color wins";

  // Explicit thickness/offset override the metrics entirely.
  Decoration custom;
  custom.thickness = 3.5f;
  custom.offset = 7.0f;
  const detail::ResolvedDecorationBand customBand =
      detail::resolveDecorationBand(custom, metrics, SK_ColorRED);
  EXPECT_FLOAT_EQ(customBand.thickness, 3.5f);
  EXPECT_FLOAT_EQ(customBand.position, 7.0f);

  // When the face DOES report metrics, they win over the floor.
  metrics.fFlags = SkFontMetrics::kUnderlineThicknessIsValid_Flag |
                   SkFontMetrics::kUnderlinePositionIsValid_Flag;
  metrics.fUnderlineThickness = 2.25f;
  metrics.fUnderlinePosition = 4.0f;
  const detail::ResolvedDecorationBand metricBand =
      detail::resolveDecorationBand({}, metrics, SK_ColorRED);
  EXPECT_FLOAT_EQ(metricBand.thickness, 2.25f);
  EXPECT_FLOAT_EQ(metricBand.position, 4.0f);
}

TEST(DecorationTest, SkipInkBreaksAroundDescenders) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"gjpqy", 48.0f);
  ParagraphLayout layout = layoutSingleLine(fontContext, paragraph, {10, 100});
  ASSERT_FALSE(layout.runs.empty());
  const PositionedRun& run = layout.runs.front();

  const SkFont font = makeFont(run.shaped->typeface, run.shaped->fontSize);
  SkFontMetrics metrics;
  font.getMetrics(&metrics);

  Decoration skipping;  // default underline, skipInk = true
  const detail::ResolvedDecorationBand band =
      detail::resolveDecorationBand(skipping, metrics, SK_ColorBLACK);
  const auto skippedSegments = detail::decorationSegments(run, skipping, band);
  EXPECT_GT(skippedSegments.size(), 1u)
      << "five descenders must interrupt the underline";

  Decoration solid;
  solid.skipInk = false;
  const auto solidSegments = detail::decorationSegments(run, solid, band);
  ASSERT_EQ(solidSegments.size(), 1u);
  EXPECT_FLOAT_EQ(solidSegments[0].first, run.origin.x());
  EXPECT_FLOAT_EQ(solidSegments[0].second,
                  run.origin.x() + run.shaped->advance);

  // Total skipped coverage is strictly less than the solid line.
  float skippedLength = 0;
  for (const auto& [start, end] : skippedSegments) skippedLength += end - start;
  EXPECT_LT(skippedLength, solidSegments[0].second - solidSegments[0].first);
}

TEST(DecorationTest, BandPaintOverrideAppliesVerbatim) {
  SkFontMetrics metrics = {};
  metrics.fAscent = -20.0f;
  metrics.fDescent = 6.0f;

  // Default fill: an anti-aliased solid of the band's resolved color.
  Decoration plain;
  plain.color = SK_ColorBLUE;
  const SkPaint fallback = detail::decorationBandPaint(
      plain, detail::resolveDecorationBand(plain, metrics, SK_ColorRED));
  EXPECT_TRUE(fallback.isAntiAlias());
  EXPECT_EQ(fallback.getColor(), SK_ColorBLUE);
  EXPECT_EQ(fallback.getShader(), nullptr);

  // Override fill: the caller's paint verbatim — shader, blend mode,
  // alpha — taking precedence over `color`.
  Decoration shaded;
  shaded.color = SK_ColorBLUE;  // ignored once `paint` is set
  SkPaint bandPaint;
  bandPaint.setShader(SkShaders::Color(SK_ColorGREEN));
  bandPaint.setBlendMode(SkBlendMode::kScreen);
  bandPaint.setAlphaf(0.5f);
  shaded.paint = bandPaint;
  EXPECT_EQ(
      detail::decorationBandPaint(
          shaded, detail::resolveDecorationBand(shaded, metrics, SK_ColorRED)),
      bandPaint);
}

TEST(DecorationTest, HighlightDefaultColorIsTranslucentForeground) {
  SkFontMetrics metrics = {};
  metrics.fAscent = -20.0f;
  metrics.fDescent = 6.0f;
  Decoration highlight;
  highlight.kind = Decoration::Kind::kHighlight;
  const detail::ResolvedDecorationBand band =
      detail::resolveDecorationBand(highlight, metrics, SK_ColorBLUE);
  EXPECT_EQ(SkColorGetB(band.color), 0xFFu) << "hue follows the foreground";
  EXPECT_LT(SkColorGetA(band.color), 0x80u)
      << "default highlight must not hide the text behind it";
  EXPECT_FLOAT_EQ(band.position, -20.0f) << "band top at the ascent line";
  EXPECT_FLOAT_EQ(band.thickness, 26.0f) << "ascent + descent tall";
}

TEST(DecorationTest, AColumnBandIsMeasuredFromTheColumnAxis) {
  // Down a column there is no baseline: the band is placed from the axis
  // the glyphs centre on, and the em box's half-depth is what puts it
  // clear of them. Ascent 20 and descent 6 make that half-depth 13.
  SkFontMetrics metrics = {};
  metrics.fFlags = SkFontMetrics::kUnderlineThicknessIsValid_Flag |
                   SkFontMetrics::kUnderlinePositionIsValid_Flag;
  metrics.fAscent = -20.0f;
  metrics.fDescent = 6.0f;
  metrics.fUnderlineThickness = 2.0f;
  metrics.fUnderlinePosition = 4.0f;
  constexpr float kHalfEm = 13.0f;

  const detail::ResolvedDecorationBand underline =
      detail::resolveDecorationBand({}, metrics, SK_ColorRED,
                                    /*alongColumn=*/true);
  EXPECT_FLOAT_EQ(underline.thickness, 2.0f) << "the face's own thickness";
  EXPECT_FLOAT_EQ(underline.position, kHalfEm)
      << "an emphasis line stands clear of the em box on the column's RIGHT";

  Decoration overline;
  overline.kind = Decoration::Kind::kOverline;
  const detail::ResolvedDecorationBand overBand = detail::resolveDecorationBand(
      overline, metrics, SK_ColorRED, /*alongColumn=*/true);
  EXPECT_FLOAT_EQ(overBand.position, -kHalfEm - overBand.thickness)
      << "and its opposite stands clear on the LEFT";

  Decoration strike;
  strike.kind = Decoration::Kind::kStrikethrough;
  const detail::ResolvedDecorationBand strikeBand =
      detail::resolveDecorationBand(strike, metrics, SK_ColorRED,
                                    /*alongColumn=*/true);
  EXPECT_FLOAT_EQ(strikeBand.position, -strikeBand.thickness * 0.5f)
      << "a strikethrough runs DOWN the axis it crosses out";

  Decoration highlight;
  highlight.kind = Decoration::Kind::kHighlight;
  const detail::ResolvedDecorationBand highlightBand =
      detail::resolveDecorationBand(highlight, metrics, SK_ColorRED,
                                    /*alongColumn=*/true);
  EXPECT_FLOAT_EQ(highlightBand.position, -kHalfEm);
  EXPECT_FLOAT_EQ(highlightBand.thickness, 2 * kHalfEm)
      << "a highlight covers the whole column pitch";

  // An explicit offset still wins, and is then read across the column.
  Decoration nudged;
  nudged.offset = -9.0f;
  EXPECT_FLOAT_EQ(detail::resolveDecorationBand(nudged, metrics, SK_ColorRED,
                                                /*alongColumn=*/true)
                      .position,
                  -9.0f);
}

TEST(DecorationTest, AColumnBandRunsUncutDownItsRun) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"縦書きの傍線", 32.0f);
  paragraph.setWritingMode(WritingMode::kVerticalRL);
  VerticalBlockFlow flow(SkRect::MakeWH(120, 400));
  ParagraphLayoutOptions options;
  options.lineMetrics.height = 40;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  ASSERT_FALSE(layout.runs.empty());
  const PositionedRun& run = layout.runs.front();
  ASSERT_TRUE(run.shaped && run.shaped->vertical);

  const SkFont font = makeFont(run.shaped->typeface, run.shaped->fontSize);
  SkFontMetrics metrics;
  font.getMetrics(&metrics);
  Decoration skipping;  // default underline, skipInk = true
  const detail::ResolvedDecorationBand band = detail::resolveDecorationBand(
      skipping, metrics, SK_ColorBLACK, /*alongColumn=*/true);
  const auto segments = detail::decorationSegments(run, skipping, band);
  // Intercepts are cut out of a horizontal window; a column has none to
  // read, so the band is whole and runs along the column, not across it.
  ASSERT_EQ(segments.size(), 1u);
  EXPECT_FLOAT_EQ(segments[0].first, run.origin.y());
  EXPECT_FLOAT_EQ(segments[0].second, run.origin.y() + run.shaped->advance);
}
