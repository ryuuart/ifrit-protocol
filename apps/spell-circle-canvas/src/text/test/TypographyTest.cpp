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

// ── Text transform (ShapingStyle::textTransform) ─────────────────────────

namespace {

Paragraph transformedParagraph(std::u8string_view text,
                               TextTransform transform,
                               std::string languageTag = {}) {
  TextStyle style = basicStyle();
  style.shaping.textTransform = transform;
  style.shaping.languageTag = std::move(languageTag);
  Paragraph paragraph;
  paragraph.appendText(text, style);
  return paragraph;
}

float paragraphWidth(Paragraph &paragraph) {
  return paragraph.naturalWidth(sharedContext());
}

} // namespace

TEST(TextTransformTest, UppercaseShapesUppercaseGlyphs) {
  Paragraph transformed =
      transformedParagraph(u8"hello", TextTransform::kUppercase);
  Paragraph reference = makeParagraph(u8"HELLO");
  // Identical shaped output — and, per the documented contract, the same
  // shape-cache entry, since the transformed text is itself the key text.
  transformed.ensureShaped(sharedContext());
  reference.ensureShaped(sharedContext());
  EXPECT_EQ(transformed.words()[0].segments[0].shaped.get(),
            reference.words()[0].segments[0].shaped.get());
  // The stored document text stays untransformed.
  EXPECT_EQ(transformed.text(), u"hello");
}

TEST(TextTransformTest, GermanSharpSExpandsUnderUppercase) {
  Paragraph transformed =
      transformedParagraph(u8"straße", TextTransform::kUppercase);
  Paragraph reference = makeParagraph(u8"STRASSE");
  EXPECT_NEAR(paragraphWidth(transformed), paragraphWidth(reference), 0.5f)
      << "ß must full-map to SS, not simple-map";
}

TEST(TextTransformTest, TurkishDotlessIRespectsLocale) {
  Paragraph turkish =
      transformedParagraph(u8"istanbul", TextTransform::kUppercase, "tr");
  Paragraph plain = transformedParagraph(u8"istanbul",
                                         TextTransform::kUppercase);
  turkish.ensureShaped(sharedContext());
  plain.ensureShaped(sharedContext());
  // tr maps i → İ (dotted capital); the root locale maps i → I. Different
  // glyph streams must come back.
  EXPECT_NE(turkish.words()[0].segments[0].shaped->glyphs,
            plain.words()[0].segments[0].shaped->glyphs);
}

TEST(TextTransformTest, CapitalizeTitlecasesFirstLetterOnly) {
  Paragraph transformed =
      transformedParagraph(u8"mixedCase words here", TextTransform::kCapitalize);
  Paragraph reference = makeParagraph(u8"MixedCase Words Here");
  transformed.ensureShaped(sharedContext());
  reference.ensureShaped(sharedContext());
  ASSERT_EQ(transformed.words().size(), reference.words().size());
  for (size_t wordIndex = 0; wordIndex < reference.words().size(); ++wordIndex)
    EXPECT_EQ(
        transformed.words()[wordIndex].segments[0].shaped.get(),
        reference.words()[wordIndex].segments[0].shaped.get())
        << "word " << wordIndex
        << ": capitalize must uppercase the first letter and leave the rest";
}

TEST(TextTransformTest, ToggleReshapesButQueriesStayUntransformed) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"query my text");
  paragraph.ensureShaped(fontContext);

  fontContext.resetStats();
  TextStyle upper = basicStyle();
  upper.shaping.textTransform = TextTransform::kUppercase;
  paragraph.setStyle(0, 5, upper); // "query"
  paragraph.ensureShaped(fontContext);
  EXPECT_GT(fontContext.stats().shapeCalls, 0u)
      << "changing the transform must re-shape the covered words";

  // Query addresses the stored (untransformed) text.
  const std::vector<CharRange> hits = findAllOccurrences(paragraph, u8"query");
  ASSERT_EQ(hits.size(), 1u);
  EXPECT_EQ(hits[0], (CharRange{0, 5}));
}

// ── Word spacing (ShapingStyle::wordSpacing) ─────────────────────────────

TEST(WordSpacingTest, WidensGlueWithoutReshaping) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"alpha beta gamma");
  paragraph.ensureShaped(fontContext);
  const float baseGlue = paragraph.words()[0].spaceWidth;
  ASSERT_GT(baseGlue, 0.0f);

  fontContext.resetStats();
  TextStyle spaced = basicStyle();
  spaced.shaping.wordSpacing = 12.0f;
  paragraph.setStyle(0, static_cast<uint32_t>(paragraph.text().size()), spaced);
  paragraph.ensureShaped(fontContext);
  EXPECT_FLOAT_EQ(paragraph.words()[0].spaceWidth, baseGlue + 12.0f);
  EXPECT_EQ(fontContext.stats().shapeCalls, 0u)
      << "word spacing must not enter the shape-cache key";

  // Negative spacing shrinks but never below zero.
  TextStyle crushed = basicStyle();
  crushed.shaping.wordSpacing = -1000.0f;
  paragraph.setStyle(0, static_cast<uint32_t>(paragraph.text().size()),
                     crushed);
  paragraph.ensureShaped(fontContext);
  EXPECT_FLOAT_EQ(paragraph.words()[0].spaceWidth, 0.0f);
}

TEST(WordSpacingTest, BreakerAndNaturalWidthConsumeIt) {
  FontContext &fontContext = sharedContext();
  TextStyle spaced = basicStyle();
  spaced.shaping.wordSpacing = 40.0f;
  Paragraph wide;
  wide.appendText(u8"one two three four five", spaced);
  Paragraph normal = makeParagraph(u8"one two three four five");
  // Natural width grows by exactly 4 gaps × 40px.
  EXPECT_NEAR(wide.naturalWidth(fontContext),
              normal.naturalWidth(fontContext) + 4 * 40.0f, 0.01f);
  // A measure that fits the normal text on one line wraps the spaced one.
  const float measure = normal.naturalWidth(fontContext) + 20.0f;
  BlockFlow flowNormal(SkRect::MakeWH(measure, 400));
  BlockFlow flowWide(SkRect::MakeWH(measure, 400));
  EXPECT_EQ(layoutParagraph(fontContext, normal, flowNormal).lineCount, 1);
  EXPECT_GT(layoutParagraph(fontContext, wide, flowWide).lineCount, 1);
}

// ── Text decorations (PaintStyle::decorations) ───────────────────────────

TEST(DecorationTest, BandResolvesFromMetricsWithFloors) {
  SkFontMetrics metrics = {};
  metrics.fFlags = 0; // face reports no underline/strikeout metrics
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
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"gjpqy", 48.0f);
  ParagraphLayout layout =
      layoutSingleLine(fontContext, paragraph, {10, 100});
  ASSERT_FALSE(layout.runs.empty());
  const PositionedRun &run = layout.runs.front();

  const SkFont font = makeFont(run.shaped->typeface, run.shaped->fontSize);
  SkFontMetrics metrics;
  font.getMetrics(&metrics);

  Decoration skipping; // default underline, skipInk = true
  const detail::ResolvedDecorationBand band =
      detail::resolveDecorationBand(skipping, metrics, SK_ColorBLACK);
  const auto skippedSegments =
      detail::decorationSegments(run, skipping, band);
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
  for (const auto &[start, end] : skippedSegments)
    skippedLength += end - start;
  EXPECT_LT(skippedLength, solidSegments[0].second - solidSegments[0].first);
}

TEST(DecorationTest, RestyleDrawsWithoutReshaping) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"decorate me");
  BlockFlow flow(SkRect::MakeWH(400, 60));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  fontContext.resetStats();
  PaintStyle decorated(SK_ColorBLACK);
  decorated.addDecoration({})
      .addDecoration({.kind = Decoration::Kind::kStrikethrough,
                      .color = SK_ColorRED});
  paragraph.setPaint(0, 8, decorated);

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 60));
  surface->getCanvas()->clear(SK_ColorWHITE);
  layout.draw(surface->getCanvas(), paragraph);
  layout.drawBatched(surface->getCanvas(), paragraph);
  EXPECT_EQ(fontContext.stats().shapeCalls, 0u)
      << "decorations are paint-side: no reshape, no relayout";

  // The red strikethrough must have put red ink on the surface. The band
  // may be 1px tall on a fractional baseline offset, so anti-aliasing can
  // blend every pixel — accept dominantly-red rather than exact SK_ColorRED.
  SkPixmap pixmap;
  ASSERT_TRUE(surface->peekPixels(&pixmap));
  bool sawRed = false;
  for (int y = 0; y < pixmap.height() && !sawRed; ++y)
    for (int x = 0; x < pixmap.width() && !sawRed; ++x) {
      const SkColor color = pixmap.getColor(x, y);
      sawRed = SkColorGetR(color) > 200 && SkColorGetG(color) < 128 &&
               SkColorGetB(color) < 128;
    }
  EXPECT_TRUE(sawRed);
}

// ── Features:: presets (Features.h) ──────────────────────────────────────

TEST(FeaturePresets, TagsAreConstexprAndWellFormed) {
  static_assert(Features::tabularNumbers == FontFeature{"tnum", 1});
  static_assert(Features::standardLigaturesOff == FontFeature{"liga", 0});
  static_assert(Features::smallCaps == FontFeature{"smcp", 1});
  static_assert(Features::stylisticSet(1) == FontFeature{"ss01", 1});
  static_assert(Features::stylisticSet(20) == FontFeature{"ss20", 1});
  static_assert(Features::stylisticSet(7) == FontFeature{"ss07", 1});
  // Out-of-range indices clamp instead of producing bogus tags.
  static_assert(Features::stylisticSet(0) == FontFeature{"ss01", 1});
  static_assert(Features::stylisticSet(99) == FontFeature{"ss20", 1});
  SUCCEED();
}

TEST(FeaturePresets, TabularNumbersEqualizeDigitAdvances) {
  FontContext &fontContext = sharedContext();
  // SF Pro (macOS system font) ships proportional figures by default and a
  // tnum feature; fall back to skipping when neither is measurable.
  auto digitWidths = [&](std::vector<FontFeature> features) {
    TextStyle style = basicStyle(32.0f);
    style.shaping.fontFeatures = std::move(features);
    std::vector<float> widths;
    for (const char8_t *digit : {u8"1", u8"0", u8"7", u8"9"}) {
      Paragraph paragraph;
      paragraph.appendText(digit, style);
      paragraph.ensureShaped(fontContext);
      widths.push_back(paragraph.words()[0].width);
    }
    return widths;
  };

  const std::vector<float> proportional = digitWidths({});
  const std::vector<float> tabular = digitWidths({Features::tabularNumbers});

  const auto spread = [](const std::vector<float> &widths) {
    const auto [minimum, maximum] =
        std::minmax_element(widths.begin(), widths.end());
    return *maximum - *minimum;
  };
  if (spread(proportional) < 0.01f)
    GTEST_SKIP() << "default face already has uniform digits; tnum unprovable";
  EXPECT_LT(spread(tabular), 0.01f)
      << "tabular figures must share one advance";
}

TEST(DecorationTest, UnderlineSpansAcrossWordGaps) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"mono nano", 32.0f);
  BlockFlow flow(SkRect::MakeWH(400, 80));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  // Two words → (at least) two runs on one line with a glue gap between.
  std::vector<const PositionedRun *> wordRuns;
  for (const PositionedRun &run : layout.runs)
    if (run.shaped)
      wordRuns.push_back(&run);
  ASSERT_GE(wordRuns.size(), 2u);
  const float gapStart =
      wordRuns[0]->origin.x() + wordRuns[0]->shaped->advance;
  const float gapEnd = wordRuns[1]->origin.x();
  ASSERT_GT(gapEnd, gapStart) << "expected inter-word glue";

  PaintStyle underlined(SK_ColorBLACK);
  Decoration underline;
  underline.thickness = 3.0f;
  underline.offset = 6.0f; // clear of any glyph ink
  underline.skipInk = false;
  underlined.addDecoration(underline);
  paragraph.setPaint(0, static_cast<uint32_t>(paragraph.text().size()),
                     underlined);

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 80));
  surface->getCanvas()->clear(SK_ColorWHITE);
  layout.draw(surface->getCanvas(), paragraph);

  // The decorated range is one continuous band: the middle of the glue gap
  // must be inked, not white (pre-spanning behavior drew per-word bands
  // that skipped the space).
  SkPixmap pixmap;
  ASSERT_TRUE(surface->peekPixels(&pixmap));
  const int probeX = static_cast<int>((gapStart + gapEnd) * 0.5f);
  const int probeY =
      static_cast<int>(wordRuns[0]->origin.y() + 6.0f + 1.5f);
  ASSERT_LT(probeX, pixmap.width());
  ASSERT_LT(probeY, pixmap.height());
  const SkColor gapColor = pixmap.getColor(probeX, probeY);
  EXPECT_LT(SkColorGetR(gapColor), 100u)
      << "underline must cover the word gap (got "
      << std::hex << gapColor << ")";

  // Same probe with skip-ink on: gaps still covered (no ink there).
  PaintStyle skipInked(SK_ColorBLACK);
  Decoration inkAware;
  inkAware.thickness = 3.0f;
  inkAware.offset = 6.0f;
  skipInked.addDecoration(inkAware);
  paragraph.setPaint(0, static_cast<uint32_t>(paragraph.text().size()),
                     skipInked);
  surface->getCanvas()->clear(SK_ColorWHITE);
  layout.draw(surface->getCanvas(), paragraph);
  ASSERT_TRUE(surface->peekPixels(&pixmap));
  EXPECT_LT(SkColorGetR(pixmap.getColor(probeX, probeY)), 100u)
      << "skip-ink must only break at glyph ink, never at word gaps";
}

TEST(DecorationTest, PerWordSpanBreaksAtGaps) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"mono nano", 32.0f);
  BlockFlow flow(SkRect::MakeWH(400, 80));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  std::vector<const PositionedRun *> wordRuns;
  for (const PositionedRun &run : layout.runs)
    if (run.shaped)
      wordRuns.push_back(&run);
  ASSERT_GE(wordRuns.size(), 2u);
  const float gapStart =
      wordRuns[0]->origin.x() + wordRuns[0]->shaped->advance;
  const float gapEnd = wordRuns[1]->origin.x();
  ASSERT_GT(gapEnd, gapStart);

  PaintStyle underlined(SK_ColorBLACK);
  Decoration perWord;
  perWord.span = Decoration::Span::kPerWord;
  perWord.thickness = 3.0f;
  perWord.offset = 6.0f;
  perWord.skipInk = false;
  underlined.addDecoration(perWord);
  paragraph.setPaint(0, static_cast<uint32_t>(paragraph.text().size()),
                     underlined);

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 80));
  surface->getCanvas()->clear(SK_ColorWHITE);
  layout.draw(surface->getCanvas(), paragraph);

  SkPixmap pixmap;
  ASSERT_TRUE(surface->peekPixels(&pixmap));
  const int gapX = static_cast<int>((gapStart + gapEnd) * 0.5f);
  const int bandY = static_cast<int>(wordRuns[0]->origin.y() + 6.0f + 1.5f);
  // The gap stays bare…
  EXPECT_GT(SkColorGetR(pixmap.getColor(gapX, bandY)), 200u)
      << "kPerWord must not underline the word gap";
  // …while both words still carry their own bands.
  const int firstWordX =
      static_cast<int>(wordRuns[0]->origin.x() +
                       wordRuns[0]->shaped->advance * 0.5f);
  EXPECT_LT(SkColorGetR(pixmap.getColor(firstWordX, bandY)), 100u);
}

TEST(DecorationTest, HighlightSpansGapsBeneathGlyphs) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"mono nano", 32.0f);
  BlockFlow flow(SkRect::MakeWH(400, 80));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  std::vector<const PositionedRun *> wordRuns;
  for (const PositionedRun &run : layout.runs)
    if (run.shaped)
      wordRuns.push_back(&run);
  ASSERT_GE(wordRuns.size(), 2u);
  const float gapStart =
      wordRuns[0]->origin.x() + wordRuns[0]->shaped->advance;
  const float gapEnd = wordRuns[1]->origin.x();
  ASSERT_GT(gapEnd, gapStart);

  PaintStyle marked(SK_ColorBLACK);
  Decoration highlight;
  highlight.kind = Decoration::Kind::kHighlight;
  highlight.color = 0x80FFE066; // translucent marker yellow
  marked.addDecoration(highlight);
  paragraph.setPaint(0, static_cast<uint32_t>(paragraph.text().size()),
                     marked);

  for (const bool batched : {false, true}) {
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 80));
    surface->getCanvas()->clear(SK_ColorWHITE);
    if (batched)
      layout.drawBatched(surface->getCanvas(), paragraph);
    else
      layout.draw(surface->getCanvas(), paragraph);

    SkPixmap pixmap;
    ASSERT_TRUE(surface->peekPixels(&pixmap));
    // Mid-gap, mid-x-height: the marker stroke must cover the space
    // between words (tinted, not white).
    const int gapX = static_cast<int>((gapStart + gapEnd) * 0.5f);
    const int xHeightY = static_cast<int>(wordRuns[0]->origin.y() - 8.0f);
    const SkColor gapColor = pixmap.getColor(gapX, xHeightY);
    EXPECT_NE(gapColor, SK_ColorWHITE)
        << (batched ? "batched" : "immediate")
        << ": highlight must cover the word gap";
    EXPECT_GT(SkColorGetB(gapColor), 100u)
        << "gap should be a tint, not glyph ink";

    // The glyphs draw over the highlight: dark ink must survive somewhere
    // inside the first word's extent at x-height.
    bool sawInk = false;
    const int wordStartX = static_cast<int>(wordRuns[0]->origin.x());
    const int wordEndX = static_cast<int>(gapStart);
    for (int x = wordStartX; x < wordEndX && !sawInk; ++x)
      for (int y = xHeightY - 6; y <= xHeightY + 6 && !sawInk; ++y)
        sawInk = SkColorGetR(pixmap.getColor(x, y)) < 80;
    EXPECT_TRUE(sawInk) << "glyphs must draw above the highlight";
  }
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
