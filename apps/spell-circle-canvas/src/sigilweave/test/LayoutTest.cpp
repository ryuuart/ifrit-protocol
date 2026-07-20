/** @file
 * layoutParagraph() placement: alignment, justification,
 * mandatory breaks, exclusion/line-set/path flows, plus the
 * Blink-inspired typographic correctness invariants.
 */

#include "TestSupport.h"

#include <gtest/gtest.h>

#include <include/core/SkFontMetrics.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>

#include <absl/container/flat_hash_set.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <set>
#include <vector>
using namespace sigil::weave;
using namespace sigil::weave::test;

// ── Line-width invariant, both breakers ──────────────────────────────────

namespace {

/// The one width invariant every breaker must uphold: no placed run may
/// stick out past the measure — overfull lines are infeasible unless there
/// is truly no alternative. Parameterized over both strategies so greedy
/// and Knuth-Plass are held to the identical standard on the same
/// hyphen-laden text across a sweep of measures.
class LineWidthInvariant : public ::testing::TestWithParam<LineBreakStrategy> {
};

} // namespace

TEST_P(LineWidthInvariant, LinesNeverExceedTheMeasure) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph;
  paragraph.appendText(
      u8"The para­graph breaker con­sid­ers every way to break "
      "this text into lines and picks the one with the least bad­ness, "
      "ex­act­ly like TeX. Greedy breaking com­mits line by "
      "line and leaves rag­ged, in­con­sis­tent "
      "spac­ing be­hind; op­ti­mal breaking spreads the "
      "slack across the whole para­graph in­stead.",
      basicStyle(17.0f));

  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  options.lineBreakStrategy = GetParam();
  options.lineMetrics.height = 27;

  for (float measure = 150; measure <= 430; measure += 7.0f) {
    BlockFlow flow(SkRect::MakeWH(measure, 3000));
    ParagraphLayout layout =
        layoutParagraph(fontContext, paragraph, flow, options);
    EXPECT_FALSE(layout.overflowed());
    for (const PositionedRun &run : layout.runs) {
      if (!run.shaped)
        continue;
      const float end = run.origin.x() + run.shaped->advance;
      EXPECT_LE(end, measure + 0.75f)
          << "line " << run.lineIndex << " leaks past the " << measure
          << "px measure";
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    Breakers, LineWidthInvariant,
    ::testing::Values(LineBreakStrategy::kGreedy,
                      LineBreakStrategy::kKnuthPlass),
    [](const ::testing::TestParamInfo<LineBreakStrategy> &info) {
      return info.param == LineBreakStrategy::kGreedy ? "Greedy" : "KnuthPlass";
    });

TEST(ParagraphLayout, MandatoryBreakStartsNewLine) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"alpha\nbeta");
  BlockFlow flow(SkRect::MakeWH(500, 300));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
  ASSERT_EQ(layout.runs.size(), 2u);
  EXPECT_NE(layout.runs[0].lineIndex, layout.runs[1].lineIndex);
  EXPECT_LT(layout.runs[0].origin.y(), layout.runs[1].origin.y());
}

TEST(ParagraphLayout, CenterAndEndAlignment) {
  FontContext &fontContext = sharedContext();
  ParagraphLayoutOptions options;

  Paragraph paragraph = makeParagraph(u8"word");
  BlockFlow flow(SkRect::MakeWH(400, 100));

  options.alignment = TextAlignment::kStart;
  const float startX =
      layoutParagraph(fontContext, paragraph, flow, options).runs[0].origin.x();
  options.alignment = TextAlignment::kCenter;
  const float centerX =
      layoutParagraph(fontContext, paragraph, flow, options).runs[0].origin.x();
  options.alignment = TextAlignment::kEnd;
  const float endX =
      layoutParagraph(fontContext, paragraph, flow, options).runs[0].origin.x();

  EXPECT_FLOAT_EQ(startX, 0);
  EXPECT_GT(centerX, startX);
  EXPECT_GT(endX, centerX);
  EXPECT_NEAR(centerX * 2, endX, 1.0f);
}

TEST(ParagraphLayout, JustifiedLinesFillTheMeasure) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      u8"justification stretches the spaces between words so every full line "
      "extends to the right edge of the measure exactly");
  BlockFlow flow(SkRect::MakeWH(260, 600));
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  ASSERT_GT(layout.lineCount, 2);

  // Every line except the last must reach (near) the right edge.
  std::vector<float> lineEnds(static_cast<size_t>(layout.lineCount), 0.0f);
  for (const PositionedRun &run : layout.runs)
    lineEnds[static_cast<size_t>(run.lineIndex)] = std::max(
        lineEnds[static_cast<size_t>(run.lineIndex)], runEnd(paragraph, run));
  for (int line = 0; line + 1 < layout.lineCount; ++line)
    EXPECT_NEAR(lineEnds[static_cast<size_t>(line)], 260.0f, 3.0f)
        << "line " << line << " not justified";
  EXPECT_LT(lineEnds.back(), 260.0f); // ragged last line
}

TEST(ParagraphLayout, ExclusionShapeSplitsText) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      u8"text flows around the shape and continues on the far side of it, "
      "filling both fragments of every interrupted line with words");
  ExclusionFlow flow(SkRect::MakeWH(400, 300));
  flow.shapes().push_back(
      {ExclusionFlow::Shape::kCircle, SkRect::MakeXYWH(140, 40, 120, 120), 6});
  flow.setMinIntervalWidth(40);
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  // Some line must have runs both left and right of the circle.
  bool split = false;
  for (int line = 0; line < layout.lineCount && !split; ++line) {
    bool left = false, right = false;
    for (const PositionedRun &run : layout.runs) {
      if (run.lineIndex != line)
        continue;
      if (run.origin.x() < 140)
        left = true;
      if (run.origin.x() > 260)
        right = true;
    }
    split = left && right;
  }
  EXPECT_TRUE(split);
}

TEST(ParagraphLayout, LineSetFlowPlacesTextOnArbitrarySegments) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"words on custom lines flow freely");

  LineSetFlow flow;
  flow.lines().push_back({LineInterval{{50, 40}, {1, 0}, 150}});
  flow.lines().push_back({LineInterval{{200, 90}, {1, 0}, 150}});
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  ASSERT_FALSE(layout.runs.empty());
  for (const PositionedRun &run : layout.runs) {
    if (run.lineIndex == 0) {
      EXPECT_FLOAT_EQ(run.origin.y(), 40);
      EXPECT_GE(run.origin.x(), 50);
    } else {
      EXPECT_FLOAT_EQ(run.origin.y(), 90);
      EXPECT_GE(run.origin.x(), 200);
    }
  }
}

TEST(ParagraphLayout, RotatedLineBakesTransformedBlob) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"diagonal");
  const float inv = 1.0f / std::sqrt(2.0f);
  LineSetFlow flow;
  flow.lines().push_back({LineInterval{{0, 0}, {inv, inv}, 400}});
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
  ASSERT_EQ(layout.runs.size(), 1u);
  // Transformed runs bake positions into the blob (origin stays at 0,0)
  // and the glyphs march diagonally.
  EXPECT_EQ(layout.runs[0].origin, (SkPoint{0, 0}));
  const SkRect bounds = layout.runs[0].blob->bounds();
  EXPECT_GT(bounds.right(), 40.0f);
  EXPECT_GT(bounds.bottom(), 40.0f);
}

TEST(ParagraphLayout, PathFlowLaysGlyphsAlongCircle) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"around and around and around it goes");
  SkPath circle = SkPath::Circle(200, 200, 120);
  PathFlow flow(circle);
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  ASSERT_FALSE(layout.runs.empty());
  for (const PositionedRun &run : layout.runs) {
    const SkRect bounds = run.blob->bounds();
    const float horizontalOffset = bounds.centerX() - 200.0f;
    const float verticalOffset = bounds.centerY() - 200.0f;
    const float distanceFromCenter = std::sqrt(
        horizontalOffset * horizontalOffset + verticalOffset * verticalOffset);
    EXPECT_NEAR(distanceFromCenter, 120.0f, 40.0f)
        << "glyphs strayed off the circle";
  }
}

TEST(ParagraphLayout, AdvanceScaleTightensContourSpacing) {
  FontContext &fontContext = sharedContext();
  SkContourMeasureIter iter(SkPath::Circle(0, 0, 200), /*forceClosed=*/false);
  const sk_sp<SkContourMeasure> ring = iter.next();
  ASSERT_TRUE(ring);

  // Same text on the same ring, once at natural arc consumption and once at
  // half — the half-scale layout's final word must sit at roughly half the
  // angle around the ring (pen starts at (200, 0) and marches clockwise).
  auto lastRunAngle = [&](float scale) {
    Paragraph paragraph = makeParagraph(u8"curvature compensation", 40.0f);
    LineInterval interval;
    interval.contour = ring;
    interval.length = ring->length() / scale;
    interval.advanceScale = scale;
    LineSetFlow flow;
    flow.lines().push_back({interval});
    ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
    EXPECT_FALSE(layout.runs.empty());
    const SkRect bounds = layout.runs.back().blob->bounds();
    float angle = std::atan2(bounds.centerY(), bounds.centerX());
    if (angle < 0)
      angle += 2.0f * std::numbers::pi_v<float>;
    return angle;
  };

  const float full = lastRunAngle(1.0f);
  const float half = lastRunAngle(0.5f);
  EXPECT_GT(full, half * 1.5f)
      << "advanceScale should compress the arc the text subtends";
  EXPECT_NEAR(full, half * 2.0f, full * 0.25f);
}
// ── Typographic correctness (Blink/HarfBuzzShaperTest-inspired) ──────────
// The invariants Chrome's text stack enforces in its shaper/layout unit
// tests, adapted to SigilWeave's word model.

TEST(Correctness, VariableAxesReachHarfBuzz) {
  // A multi-axis variable instance must shape with the same complete design
  // position Skia rasterizes.
  FontContext &fontContext = sharedContext();
  sk_sp<SkTypeface> base = fontContext.fontManager()->matchFamilyStyle(
      "Noto Sans", SkFontStyle::Normal());
  const int axisCount = base ? base->getVariationDesignPosition({}) : 0;
  if (axisCount < 2)
    GTEST_SKIP() << "no multi-axis variable Noto Sans installed";

  std::vector<SkFontArguments::VariationPosition::Coordinate> coordinates(
      static_cast<size_t>(axisCount));
  if (base->getVariationDesignPosition(
          {coordinates.data(), coordinates.size()}) != axisCount)
    GTEST_SKIP() << "Noto Sans variation position unavailable";
  bool changedWeight = false;
  bool changedWidth = false;
  for (auto &coordinate : coordinates) {
    if (coordinate.axis == SkSetFourByteTag('w', 'g', 'h', 't')) {
      coordinate.value = 900.0f;
      changedWeight = true;
    } else if (coordinate.axis == SkSetFourByteTag('w', 'd', 't', 'h')) {
      coordinate.value = 62.5f;
      changedWidth = true;
    }
  }
  if (!changedWeight || !changedWidth)
    GTEST_SKIP() << "Noto Sans wght/wdth axes unavailable";

  SkFontArguments args;
  args.setVariationDesignPosition(
      {coordinates.data(), static_cast<int>(coordinates.size())});
  sk_sp<SkTypeface> varied = base->makeClone(args);
  ASSERT_TRUE(varied);

  constexpr std::u8string_view kText = u8"hamburgefonstiv";
  auto shapedAdvance = [&](const sk_sp<SkTypeface> &typeface) {
    Paragraph paragraph;
    TextStyle style = basicStyle(32.0f);
    style.shaping.typeface = typeface;
    paragraph.appendText(kText, style);
    paragraph.ensureShaped(fontContext);
    float total = 0;
    for (const Word &word : paragraph.words())
      total += word.width;
    return total;
  };

  const float regular = shapedAdvance(base);
  const float variedAdvance = shapedAdvance(varied);
  EXPECT_GT(std::abs(variedAdvance - regular), regular * 0.01f)
      << "changing wght and wdth should affect shaping";

  // HarfBuzz's advances agree with what Skia measures for that instance.
  const SkScalar measured =
      makeFont(varied, 32.0f)
          .measureText(kText.data(), kText.size(), SkTextEncoding::kUTF8);
  EXPECT_NEAR(variedAdvance, measured, variedAdvance * 0.015f);
}

TEST(Correctness, ClusterCoverageIsComplete) {
  FontContext &fontContext = sharedContext();
  // Ligating Latin, joining Arabic, conjunct Devanagari, ZWJ emoji.
  const char8_t *samples[] = {u8"office", u8"العربية", u8"नमस्ते",
                              u8"👨‍👩‍👧"};
  for (const char8_t *sample : samples) {
    Paragraph paragraph = makeParagraph(sample);
    paragraph.ensureShaped(fontContext);
    for (const Word &word : paragraph.words())
      for (const WordSegment &seg : word.segments) {
        const auto &clusters = seg.shaped->clusters;
        ASSERT_FALSE(clusters.empty());
        const size_t segLen = seg.shaped->glyphs.size();
        // Every cluster index points inside the shaped text, and the run
        // starts at offset 0 from one end (LTR: front, RTL: back).
        const uint32_t first = std::min(clusters.front(), clusters.back());
        EXPECT_EQ(first, 0u);
        for (uint32_t cluster : clusters)
          EXPECT_LT(cluster, word.textEnd - word.textBegin);
        EXPECT_GT(segLen, 0u);
      }
  }
}

TEST(Correctness, ZwnjBlocksArabicJoining) {
  FontContext &fontContext = sharedContext();
  auto glyphsOf = [&](const char8_t *text) {
    Paragraph paragraph = makeParagraph(text);
    paragraph.ensureShaped(fontContext);
    std::multiset<uint16_t> ids;
    for (const Word &word : paragraph.words())
      for (const WordSegment &seg : word.segments)
        for (uint16_t glyph : seg.shaped->glyphs)
          if (glyph != 0)
            ids.insert(glyph);
    return ids;
  };
  // "بب" joins (initial+final forms); a ZWNJ between forces isolated forms.
  EXPECT_NE(glyphsOf(u8"بب"), glyphsOf(u8"ب‌ب"));
}

TEST(Correctness, CombiningMarkAttachesToBase) {
  FontContext &fontContext = sharedContext();
  Paragraph nfc = makeParagraph(u8"café"); // é precomposed
  Paragraph nfd = makeParagraph(u8"café"); // e + combining acute
  nfc.ensureShaped(fontContext);
  nfd.ensureShaped(fontContext);
  ASSERT_EQ(nfc.words().size(), 1u);
  ASSERT_EQ(nfd.words().size(), 1u);
  // The decomposed form must not gain width: the mark attaches to the base.
  EXPECT_NEAR(nfc.words()[0].width, nfd.words()[0].width, 0.75f);
  // And the mark forms one grapheme cluster with its base: the NFD segment
  // reports at most as many clusters as it has base characters (4).
  absl::flat_hash_set<uint32_t> unique(
      nfd.words()[0].segments[0].shaped->clusters.begin(),
      nfd.words()[0].segments[0].shaped->clusters.end());
  EXPECT_LE(unique.size(), 4u);
}

TEST(Correctness, ExtremeCombiningStacksKeepBaseAdvance) {
  FontContext &fontContext = sharedContext();
  Paragraph plain = makeParagraph(u8"ZALGO TEXT", 32.0f);
  Paragraph stacked = makeParagraph(u8"Z̴̢̨̛̲̦̹̰̓̈́͊͘A̵̛̪̯̜̩͆̈́͝L̷̨̡̲̤̬̝̑̓͑̕G̵̢̺̙͎̺̤̓͛̾Ơ̶̢͙̟̲̦̿̽͋̚ "
                                    "T̷̨̗̰͉̼̯͛̋E̴̡̨̩̱͕̪͗̎X̷̢̳̮̱̪̿̈́͘T̴̛̬̠̦̞͙̋̄͝",
                                    32.0f);
  plain.ensureShaped(fontContext);
  stacked.ensureShaped(fontContext);
  if (!allGlyphsResolved(stacked))
    GTEST_SKIP() << "combining-mark fallback coverage unavailable";

  auto glyphCount = [](const Paragraph &paragraph) {
    size_t count = 0;
    for (const Word &word : paragraph.words())
      for (const WordSegment &segment : word.segments)
        count += segment.shaped->glyphs.size();
    return count;
  };
  EXPECT_GT(glyphCount(stacked), glyphCount(plain) + 50u);
  const float plainWidth = plain.naturalWidth(fontContext);
  EXPECT_NEAR(stacked.naturalWidth(fontContext), plainWidth, plainWidth * 0.03f)
      << "attached mark stacks must add ink, not horizontal advance";
}

TEST(Correctness, KinsokuProhibitsLineInitialPunctuation) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph =
      makeParagraph(u8"これは、禁則処理のテストです。行頭に句読点は来ない。");
  paragraph.ensureShaped(fontContext);
  const std::u16string &text = paragraph.text();
  for (const Word &word : paragraph.words()) {
    // A break opportunity never lands *before* a closing punctuation mark:
    // no word (== potential line start) begins with 。、」.
    const char16_t first = text[word.textBegin];
    EXPECT_NE(first, u'。');
    EXPECT_NE(first, u'、');
    EXPECT_NE(first, u'」');
    // …and never *after* an opening bracket: no word's content ends with 「.
    if (word.textEnd > word.textBegin) {
      EXPECT_NE(text[word.textEnd - 1], u'「');
    }
  }
}

TEST(Correctness, NbspNeverBreaks) {
  FontContext &fontContext = sharedContext();
  Paragraph spaced = makeParagraph(u8"100 km");
  Paragraph glued = makeParagraph(u8"100 km");
  spaced.ensureShaped(fontContext);
  glued.ensureShaped(fontContext);
  EXPECT_EQ(spaced.words().size(), 2u);
  EXPECT_EQ(glued.words().size(), 1u) << "NBSP must not be a break point";
}

TEST(Correctness, TabsMeasureAsSpaces) {
  FontContext &fontContext = sharedContext();
  Paragraph tab = makeParagraph(u8"a\tb");
  Paragraph space = makeParagraph(u8"a b");
  tab.ensureShaped(fontContext);
  space.ensureShaped(fontContext);
  ASSERT_EQ(tab.words().size(), 2u);
  EXPECT_FLOAT_EQ(tab.words()[0].spaceWidth, space.words()[0].spaceWidth);
}

TEST(Correctness, JustifiedShrinkNeverCollapsesSpaces) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      u8"several reasonably long words keep justification honest here", 18.0f);
  paragraph.ensureShaped(fontContext);
  // A measure a hair narrower than a natural line forces shrink.
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  BlockFlow flow(SkRect::MakeWH(200, 400));
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);

  ASSERT_GT(layout.lineCount, 1);
  for (size_t runIndex = 0; runIndex + 1 < layout.runs.size(); ++runIndex) {
    const PositionedRun &firstRun = layout.runs[runIndex];
    const PositionedRun &secondRun = layout.runs[runIndex + 1];
    if (firstRun.lineIndex != secondRun.lineIndex)
      continue;
    const float gapWidth = secondRun.origin.x() - runEnd(paragraph, firstRun);
    const float naturalSpaceWidth =
        paragraph.words()[firstRun.wordIndex].spaceWidth;
    if (naturalSpaceWidth <= 0)
      continue;
    // Shrink is clamped at the glue's shrink limit (glueShrink = 1/3).
    EXPECT_GT(gapWidth, naturalSpaceWidth * (1.0f - 0.34f) - 0.25f)
        << "space collapsed past the shrink limit on line "
        << firstRun.lineIndex;
  }
}

TEST(Correctness, BidiVisualOrderForMixedDirections) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"aaa בבב גגג zzz", 16.0f);
  BlockFlow flow(SkRect::MakeWH(600, 60)); // one wide line
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  // Logical order: aaa(0) בבב(1) גגג(2) zzz(3). UAX#9: the two RTL words
  // swap visually — גגג renders left of בבב, both between aaa and zzz.
  float runOrigins[4] = {0, 0, 0, 0};
  for (const PositionedRun &run : layout.runs)
    if (run.wordIndex < 4)
      runOrigins[run.wordIndex] = run.origin.x();
  EXPECT_LT(runOrigins[0], runOrigins[2]);
  EXPECT_LT(runOrigins[2], runOrigins[1])
      << "RTL pair must render in reversed visual order";
  EXPECT_LT(runOrigins[1], runOrigins[3]);
}

TEST(Correctness, StrutMatchesFontMetrics) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"metrics", 32.0f);
  const Paragraph::Strut strut = paragraph.strut(fontContext);
  const SkFont font = makeFont(fontContext.defaultTypeface(), 32.0f);
  SkFontMetrics metrics;
  font.getMetrics(&metrics);
  EXPECT_FLOAT_EQ(strut.ascent, -metrics.fAscent);
  EXPECT_FLOAT_EQ(strut.height,
                  -metrics.fAscent + metrics.fDescent + metrics.fLeading);
}

TEST(Correctness, EditAtSurrogateBoundaryIsSafe) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"ab 𝕏𝕐 cd"); // 𝕏/𝕐 are surrogate pairs
  paragraph.ensureShaped(fontContext);
  // Cut straight through the middle of the first surrogate pair.
  const size_t textOffset = paragraph.text().find(u"ab");
  ASSERT_NE(textOffset, std::u16string::npos);
  paragraph.replaceText(4, 5, u8"Z");  // [4,5) is inside a pair for this string
  paragraph.ensureShaped(fontContext); // must not crash or emit garbage words
  for (const Word &word : paragraph.words())
    EXPECT_LE(word.textEnd, paragraph.text().size());
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph,
                      *std::make_unique<BlockFlow>(SkRect::MakeWH(400, 100)));
  EXPECT_FALSE(layout.runs.empty());
}

// ── Line metrics (ParagraphLayout::lineMetrics) ──────────────────────────

TEST(LineMetricsQuery, DescribesEveryPlacedLine) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      u8"enough words to wrap this paragraph across a handful of lines in "
      "a narrow measure so every line has real geometry to report");
  BlockFlow flow(SkRect::MakeXYWH(10, 20, 220, 600));
  ParagraphLayoutOptions options;
  options.lineMetrics.height = 24;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  ASSERT_GT(layout.lineCount, 2);

  const std::vector<LineMetrics> lines = layout.lineMetrics(paragraph);
  ASSERT_EQ(lines.size(), static_cast<size_t>(layout.lineCount));

  for (size_t lineNumber = 0; lineNumber < lines.size(); ++lineNumber) {
    const LineMetrics &line = lines[lineNumber];
    EXPECT_EQ(line.lineIndex, static_cast<int>(lineNumber));
    EXPECT_GT(line.ascent, 0.0f);
    EXPECT_GT(line.descent, 0.0f);
    EXPECT_GT(line.right, line.left);
    EXPECT_GE(line.left, 10.0f); // inside the flow bounds
    if (lineNumber > 0) {
      // Baselines descend by the configured line pitch.
      EXPECT_NEAR(line.baseline - lines[lineNumber - 1].baseline, 24.0f, 0.5f);
      // Character ranges advance monotonically and stay contiguous-ish
      // (each line starts where the previous one's glue ended).
      EXPECT_EQ(line.textBegin, lines[lineNumber - 1].textEnd);
    }
    // rect() is the ascent/descent band around the baseline.
    const SkRect band = line.rect();
    EXPECT_FLOAT_EQ(band.top(), line.baseline - line.ascent);
    EXPECT_FLOAT_EQ(band.bottom(), line.baseline + line.descent);
  }
  EXPECT_EQ(lines.front().textBegin, 0u);
  EXPECT_EQ(lines.back().textEnd,
            static_cast<uint32_t>(paragraph.text().size()));

  // Every run's geometry sits inside its line's band.
  for (const PositionedRun &run : layout.runs) {
    if (!run.shaped)
      continue;
    const LineMetrics &line = lines[static_cast<size_t>(run.lineIndex)];
    EXPECT_GE(run.origin.x(), line.left);
    EXPECT_LE(run.origin.x() + run.shaped->advance, line.right + 0.01f);
    EXPECT_FLOAT_EQ(run.origin.y(), line.baseline);
  }
}

TEST(LineMetricsQuery, MixedFontsGrowTheLineBand) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph;
  paragraph.appendText(u8"small ", basicStyle(14.0f));
  paragraph.appendText(u8"HUGE", basicStyle(40.0f));
  BlockFlow flow(SkRect::MakeWH(600, 100)); // one line
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  const std::vector<LineMetrics> lines = layout.lineMetrics(paragraph);
  ASSERT_EQ(lines.size(), 1u);

  Paragraph smallOnly = makeParagraph(u8"small", 14.0f);
  BlockFlow smallFlow(SkRect::MakeWH(600, 100));
  const std::vector<LineMetrics> smallLines =
      layoutParagraph(fontContext, smallOnly, smallFlow)
          .lineMetrics(smallOnly);
  ASSERT_EQ(smallLines.size(), 1u);
  EXPECT_GT(lines[0].ascent, smallLines[0].ascent)
      << "the 40px span must raise the mixed line's ascent";
}

TEST(LineMetricsQuery, PlaceholdersAndSelectionBands) {
  FontContext &fontContext = sharedContext();
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
  SkCanvas *canvas = surface->getCanvas();
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
