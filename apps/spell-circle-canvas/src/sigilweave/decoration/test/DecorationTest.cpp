/** @file
 * Decoration bands resolved without drawing: what a face's metrics fill in
 * and what an explicit number overrides, where every kind and side anchors
 * its band, skip-ink cutting a band around descenders, a paint override
 * applied verbatim, a highlight's translucent default, and the walk both
 * draws run over turning a paragraph's decorations into rectangles.
 */

#include <gtest/gtest.h>
#include <include/core/SkBlendMode.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkPaint.h>
#include <include/core/SkShader.h>
#include <sigilweave/decoration/Decoration.h>
#include <sigilweave/decoration/DecorationRects.h>

#include <string>
#include <vector>

#include "support/Faces.h"
#include "support/Layouts.h"
#include "support/Paragraphs.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

namespace {

/// A face that reports no band metrics at all: a band resolved against it
/// has nothing but the floors and the style's own numbers to go on.
SkFontMetrics silentMetrics() {
  SkFontMetrics metrics = {};
  metrics.fFlags = 0;
  metrics.fAscent = -20.0f;
  metrics.fXHeight = 10.0f;
  return metrics;
}

}  // namespace

TEST(DecorationBand, AFaceThatReportsNoBandMetricsGetsAOnePixelFloor) {
  const detail::ResolvedDecorationBand underline =
      detail::resolveDecorationBand({}, silentMetrics(), SK_ColorRED);
  EXPECT_FLOAT_EQ(underline.thickness, 1.0f);
}

TEST(DecorationBand, AFacesOwnBandMetricsWinOverTheFloor) {
  SkFontMetrics metrics = silentMetrics();
  metrics.fFlags = SkFontMetrics::kUnderlineThicknessIsValid_Flag |
                   SkFontMetrics::kUnderlinePositionIsValid_Flag;
  metrics.fUnderlineThickness = 2.25f;
  metrics.fUnderlinePosition = 4.0f;
  const detail::ResolvedDecorationBand band =
      detail::resolveDecorationBand({}, metrics, SK_ColorRED);
  EXPECT_FLOAT_EQ(band.thickness, 2.25f);
  EXPECT_FLOAT_EQ(band.position, 4.0f);
}

TEST(DecorationBand, AnExplicitThicknessAndOffsetOverrideTheMetricsEntirely) {
  SkFontMetrics metrics = silentMetrics();
  metrics.fFlags = SkFontMetrics::kUnderlineThicknessIsValid_Flag |
                   SkFontMetrics::kUnderlinePositionIsValid_Flag;
  metrics.fUnderlineThickness = 2.25f;
  metrics.fUnderlinePosition = 4.0f;
  Decoration custom;
  custom.thickness = 3.5f;
  custom.offset = 7.0f;
  const detail::ResolvedDecorationBand band =
      detail::resolveDecorationBand(custom, metrics, SK_ColorRED);
  EXPECT_FLOAT_EQ(band.thickness, 3.5f);
  EXPECT_FLOAT_EQ(band.position, 7.0f);
}

TEST(DecorationBand, AnUnsetColourIsTheForegroundAndAnExplicitOneWins) {
  EXPECT_EQ(detail::resolveDecorationBand({}, silentMetrics(), SK_ColorRED)
                .color,
            SK_ColorRED);
  Decoration blue;
  blue.color = SK_ColorBLUE;
  EXPECT_EQ(detail::resolveDecorationBand(blue, silentMetrics(), SK_ColorRED)
                .color,
            SK_ColorBLUE);
}

TEST(DecorationBand, SkipInkBreaksAroundDescenders) {
  FontContext& fontContext = sigil::test::fonts();
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

TEST(DecorationBand, BandPaintOverrideAppliesVerbatim) {
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

TEST(DecorationBand, HighlightDefaultColorIsTranslucentForeground) {
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

TEST(DecorationBand, AColumnBandRunsUncutDownItsRun) {
  FontContext& fontContext = sigil::test::fonts();
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

// ── Where a band's kind and side anchor it ───────────────────────────────

namespace {

/// A face that reports its own underline: ascent 20 and descent 6 make the
/// em box 26 deep, so a column's half-em is 13, and the underline sits 4
/// below the baseline and is 2 thick.
SkFontMetrics instrumentMetrics() {
  SkFontMetrics metrics = {};
  metrics.fFlags = SkFontMetrics::kUnderlineThicknessIsValid_Flag |
                   SkFontMetrics::kUnderlinePositionIsValid_Flag;
  metrics.fAscent = -20.0f;
  metrics.fDescent = 6.0f;
  metrics.fXHeight = 10.0f;
  metrics.fUnderlineThickness = 2.0f;
  metrics.fUnderlinePosition = 4.0f;
  return metrics;
}

detail::ResolvedDecorationBand bandFor(Decoration::Kind kind,
                                       Decoration::Side side,
                                       bool alongColumn) {
  Decoration decoration;
  decoration.kind = kind;
  decoration.side = side;
  return detail::resolveDecorationBand(decoration, instrumentMetrics(),
                                       SK_ColorRED, alongColumn);
}

/// One row of the anchor table: the near edge and the depth a band of this
/// kind, on this side, in this writing mode resolves to.
struct BandAnchor {
  const char* name;
  Decoration::Kind kind;
  Decoration::Side side;
  bool alongColumn;
  float position;
  float thickness;
};

class DecorationAnchor : public ::testing::TestWithParam<BandAnchor> {};

}  // namespace

TEST_P(DecorationAnchor, ABandLandsOnTheAnchorItsKindAndSideName) {
  // An underline and an overline are one band on the two sides of the same
  // axis, so asking for the opposite side is reading the other's metric —
  // below the baseline becomes the ascent line along a line, and right of
  // the column becomes left of it down a column. The thickness is the
  // decoration's own either way: it borrows a position, not a whole band.
  const BandAnchor& row = GetParam();
  const detail::ResolvedDecorationBand band =
      bandFor(row.kind, row.side, row.alongColumn);
  EXPECT_FLOAT_EQ(band.position, row.position);
  EXPECT_FLOAT_EQ(band.thickness, row.thickness);
}

INSTANTIATE_TEST_SUITE_P(
    Anchors, DecorationAnchor,
    ::testing::Values(
        BandAnchor{"UnderlineOnALine", Decoration::Kind::kUnderline,
                   Decoration::Side::kDefault, false, 4.0f, 2.0f},
        BandAnchor{"UnderlineFlippedOnALine", Decoration::Kind::kUnderline,
                   Decoration::Side::kOpposite, false, -20.0f, 2.0f},
        BandAnchor{"OverlineOnALine", Decoration::Kind::kOverline,
                   Decoration::Side::kDefault, false, -20.0f, 2.0f},
        BandAnchor{"OverlineFlippedOnALine", Decoration::Kind::kOverline,
                   Decoration::Side::kOpposite, false, 4.0f, 2.0f},
        BandAnchor{"UnderlineDownAColumn", Decoration::Kind::kUnderline,
                   Decoration::Side::kDefault, true, 13.0f, 2.0f},
        BandAnchor{"UnderlineFlippedDownAColumn", Decoration::Kind::kUnderline,
                   Decoration::Side::kOpposite, true, -15.0f, 2.0f},
        BandAnchor{"OverlineDownAColumn", Decoration::Kind::kOverline,
                   Decoration::Side::kDefault, true, -15.0f, 2.0f},
        BandAnchor{"OverlineFlippedDownAColumn", Decoration::Kind::kOverline,
                   Decoration::Side::kOpposite, true, 13.0f, 2.0f},
        BandAnchor{"HighlightDownAColumn", Decoration::Kind::kHighlight,
                   Decoration::Side::kDefault, true, -13.0f, 26.0f}),
    [](const ::testing::TestParamInfo<BandAnchor>& info) {
      return std::string(info.param.name);
    });

TEST(DecorationBand, ABandThatCrossesTheTypeHasNoSecondSideToTakeUp) {
  // A strikethrough and a highlight cross the type rather than standing
  // beside it, in either writing mode.
  for (const bool alongColumn : {false, true})
    for (const Decoration::Kind kind :
         {Decoration::Kind::kStrikethrough, Decoration::Kind::kHighlight})
      EXPECT_FLOAT_EQ(
          bandFor(kind, Decoration::Side::kOpposite, alongColumn).position,
          bandFor(kind, Decoration::Side::kDefault, alongColumn).position);

  // Along a line a strikethrough rides above the baseline; down a column it
  // runs ON the axis it crosses out, half its own depth to either side.
  EXPECT_LT(bandFor(Decoration::Kind::kStrikethrough,
                    Decoration::Side::kDefault, false)
                .position,
            0.0f);
  const detail::ResolvedDecorationBand crossing = bandFor(
      Decoration::Kind::kStrikethrough, Decoration::Side::kDefault, true);
  EXPECT_FLOAT_EQ(crossing.position, -crossing.thickness * 0.5f);
}

TEST(DecorationBand, AnExplicitOffsetNamesTheNearEdgeAndNoSideMovesIt) {
  Decoration nudged;
  nudged.offset = -9.0f;
  nudged.side = Decoration::Side::kOpposite;
  for (const bool alongColumn : {false, true})
    EXPECT_FLOAT_EQ(detail::resolveDecorationBand(nudged, instrumentMetrics(),
                                                  SK_ColorRED, alongColumn)
                        .position,
                    -9.0f);
}

// ── The walk both draws run over ─────────────────────────────────────────

TEST(DecorationRects, ASpanningBandIsOneRectangleAcrossEveryRunItCovers) {
  // The walk merges contiguous runs of one style on one line into a single
  // band that also covers the glue between them, and emits it in the pass
  // its kind belongs to.
  auto [paragraph, layout] = twoWordsOnOneLine();
  const std::vector<const PositionedRun*> placed = wordRuns(layout);
  ASSERT_GE(placed.size(), 2u);

  PaintStyle underlined(SK_ColorBLACK);
  Decoration underline;
  underline.thickness = 3.0f;
  underline.offset = 6.0f;
  underline.skipInk = false;
  underlined.addDecoration(underline);
  paragraph.setPaint(0, static_cast<uint32_t>(paragraph.text().size()),
                     underlined);

  const auto rectsInPhase = [&](detail::DecorationPhase phase) {
    std::vector<SkRect> rects;
    detail::forEachDecorationRect(
        layout.runs, paragraph.spans(), nullptr, phase,
        [&](const SkRect& rect, const SkPaint&) { rects.push_back(rect); });
    return rects;
  };

  const std::vector<SkRect> above =
      rectsInPhase(detail::DecorationPhase::kAboveGlyphs);
  ASSERT_EQ(above.size(), 1u) << "two words under one style are one band";
  EXPECT_FLOAT_EQ(above.front().left(), placed.front()->origin.x());
  EXPECT_FLOAT_EQ(above.front().right(),
                  placed.back()->origin.x() + placed.back()->shaped->advance)
      << "the band must reach across the glue to the last run";
  EXPECT_FLOAT_EQ(above.front().height(), 3.0f);
  EXPECT_FLOAT_EQ(above.front().top(), placed.front()->origin.y() + 6.0f);

  EXPECT_TRUE(rectsInPhase(detail::DecorationPhase::kBelowGlyphs).empty())
      << "only a highlight draws beneath the glyphs";
}

TEST(DecorationRects, APerWordBandIsOneRectanglePerRunWithTheGapLeftOpen) {
  auto [paragraph, layout] = twoWordsOnOneLine();

  PaintStyle underlined(SK_ColorBLACK);
  Decoration perWord;
  perWord.span = Decoration::Span::kPerWord;
  perWord.thickness = 3.0f;
  perWord.offset = 6.0f;
  perWord.skipInk = false;
  underlined.addDecoration(perWord);
  paragraph.setPaint(0, static_cast<uint32_t>(paragraph.text().size()),
                     underlined);

  std::vector<SkRect> rects;
  detail::forEachDecorationRect(
      layout.runs, paragraph.spans(), nullptr,
      detail::DecorationPhase::kAboveGlyphs,
      [&](const SkRect& rect, const SkPaint&) { rects.push_back(rect); });
  ASSERT_EQ(rects.size(), 2u) << "one band per word run";
  EXPECT_LT(rects[0].right(), rects[1].left())
      << "the glue between the words stays open";
}
