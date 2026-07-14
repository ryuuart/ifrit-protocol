#include <textflow/TextFlow.h>

#include <include/core/SkFontMetrics.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>
#include <include/ports/SkFontMgr_mac_ct.h>

#include <gtest/gtest.h>

#include <absl/container/flat_hash_set.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <memory>
#include <numbers>
#include <numeric>
#include <random>

using namespace textflow;

namespace {

FontContext &sharedContext() {
  // CoreText font-manager construction enumerates system fonts; do it once.
  static auto *fontContext = new FontContext(SkFontMgr_New_CoreText(nullptr));
  return *fontContext;
}

TextStyle basicStyle(float fontSize = 16.0f) {
  TextStyle style;
  style.shaping.fontSize = fontSize;
  return style;
}

Paragraph makeParagraph(std::string_view utf8, float fontSize = 16.0f) {
  Paragraph paragraph;
  paragraph.appendText(utf8, basicStyle(fontSize));
  return paragraph;
}

// Exact pen x where a run ends. Blob ink bounds are conservative
// (font-bounds based), so line-edge checks use shaped advances instead.
// Each run is one word *segment* (multi-segment words emit several runs,
// each offset by its own advanceOffset), so use the segment's shaped advance.
float runEnd(const Paragraph &paragraph, const PositionedRun &run) {
  if (run.shaped)
    return run.origin.x() + run.shaped->advance;
  return run.origin.x() + paragraph.words()[run.wordIndex].width;
}

} // namespace

// ── Shaping & caching ─────────────────────────────────────────────────────

TEST(Shaper, ShapesLatinWord) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph("Hello");
  paragraph.ensureShaped(fontContext);
  ASSERT_EQ(paragraph.words().size(), 1u);
  const Word &word = paragraph.words()[0];
  ASSERT_EQ(word.segments.size(), 1u);
  EXPECT_EQ(word.segments[0].shaped->glyphs.size(), 5u);
  EXPECT_GT(word.width, 0.0f);
  EXPECT_EQ(word.spaceWidth, 0.0f);
}

TEST(Shaper, CacheHitsOnIdenticalWords) {
  FontContext &fontContext = sharedContext();
  fontContext.purgeShapeCache();
  fontContext.resetStats();
  Paragraph paragraph = makeParagraph("tick tock tick tock tick");
  paragraph.ensureShaped(fontContext);
  // 3 distinct shape inputs: "tick", "tock", " " (glue). Repeats hit cache.
  EXPECT_EQ(fontContext.stats().shapeCalls, 3u);
  EXPECT_GT(fontContext.stats().shapeCacheHits, 0u);
}

TEST(Shaper, EditReshapesOnlyTheEditedWord) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      "the quick brown fox jumps over the lazy dog again and again");
  paragraph.ensureShaped(fontContext);

  fontContext.resetStats();
  // "quick" → "swift": positions 4..9.
  paragraph.replaceText(4, 9, "swift");
  paragraph.ensureShaped(fontContext);
  // Only the new word content misses the cache ("swift"); everything else
  // (words and glue) must hit.
  EXPECT_LE(fontContext.stats().shapeCalls, 1u);
  EXPECT_GT(fontContext.stats().shapeCacheHits, 10u);
}

TEST(Shaper, PaintOnlyRestyleNeverReshapes) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph("colorful words change their paint only");
  paragraph.ensureShaped(fontContext);

  fontContext.resetStats();
  paragraph.setPaint(9, 14, PaintStyle{SK_ColorRED});
  paragraph.ensureShaped(fontContext);
  EXPECT_EQ(fontContext.stats().shapeCalls, 0u);
}

TEST(Shaper, FontSizeRestyleReshapesOnlyCoveredWords) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph("alpha beta gamma delta epsilon");
  paragraph.ensureShaped(fontContext);

  fontContext.resetStats();
  TextStyle big = basicStyle(24.0f);
  paragraph.setStyle(6, 10, big); // "beta"
  paragraph.ensureShaped(fontContext);
  // "beta"@24 is new; its neighbors keep their cached 16px shapes. The glue
  // after "beta" also re-shapes at the new size (2 calls max).
  EXPECT_LE(fontContext.stats().shapeCalls, 2u);
}

TEST(Shaper, ClustersAreMonotone) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph("office"); // 'ffi' may ligate
  paragraph.ensureShaped(fontContext);
  const auto &clusters = paragraph.words()[0].segments[0].shaped->clusters;
  EXPECT_TRUE(std::is_sorted(clusters.begin(), clusters.end()));
}

TEST(Shaper, WordBlobIsSharedAcrossLayouts) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph("stable");
  paragraph.ensureShaped(fontContext);
  const ShapedWordRef &shaped = paragraph.words()[0].segments[0].shaped;
  const SkTextBlob *first = wordBlob(*shaped).get();
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(wordBlob(*shaped).get(), first);
}

// ── Itemization ───────────────────────────────────────────────────────────

TEST(Itemization, MixedLatinCjkSplitsIntoWords) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph("Skia は速い and 빠르다 也很快");
  paragraph.ensureShaped(fontContext);
  ASSERT_GT(paragraph.words().size(), 4u);

  bool sawIdeographic = false, sawLatin = false;
  for (const Word &word : paragraph.words()) {
    if (word.ideographic)
      sawIdeographic = true;
    else if (word.width > 0)
      sawLatin = true;
  }
  EXPECT_TRUE(sawIdeographic);
  EXPECT_TRUE(sawLatin);
}

TEST(Itemization, CjkGetsPerCharacterBreakOpportunities) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph("日本語のテキスト");
  paragraph.ensureShaped(fontContext);
  // ICU line breaking splits ideographic text nearly per character; the
  // exact count depends on kinsoku rules, but it must be far more than one.
  EXPECT_GE(paragraph.words().size(), 4u);
  for (const Word &word : paragraph.words())
    EXPECT_TRUE(word.ideographic);
}

TEST(Itemization, FallbackResolvesCjkGlyphs) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph("abc漢字xyz");
  paragraph.ensureShaped(fontContext);
  for (const Word &word : paragraph.words())
    for (const WordSegment &seg : word.segments) {
      ASSERT_TRUE(seg.shaped->typeface);
      for (uint16_t glyph : seg.shaped->glyphs)
        EXPECT_NE(glyph, 0) << "missing glyph (.notdef) leaked into layout";
    }
}

TEST(Itemization, HardBreakIsMandatory) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph("first line\nsecond");
  paragraph.ensureShaped(fontContext);
  bool sawMandatory = false;
  for (const Word &word : paragraph.words())
    sawMandatory |= word.mandatoryBreakAfter;
  EXPECT_TRUE(sawMandatory);
}

TEST(Itemization, RtlWordShapesRtl) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph("שלום");
  paragraph.ensureShaped(fontContext);
  ASSERT_EQ(paragraph.words().size(), 1u);
  const auto &clusters = paragraph.words()[0].segments[0].shaped->clusters;
  ASSERT_GE(clusters.size(), 2u);
  // RTL output is in visual order: cluster values run backwards.
  EXPECT_GT(clusters.front(), clusters.back());
}

TEST(Paragraph, ReplaceTextPreservesSurroundingStyles) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph;
  TextStyle red = basicStyle();
  red.paint.color = SK_ColorRED;
  TextStyle blue = basicStyle();
  blue.paint.color = SK_ColorBLUE;
  paragraph.appendText("red ", red);
  paragraph.appendText("blue", blue);

  paragraph.replaceText(4, 8, "teal"); // swap the blue word's text
  paragraph.ensureShaped(fontContext);
  ASSERT_GE(paragraph.spans().size(), 2u);
  EXPECT_EQ(paragraph.spans().front().style.paint.color, SK_ColorRED);
  // Inserted text inherits the style at the edit point (the blue span).
  EXPECT_EQ(paragraph.spans().back().style.paint.color, SK_ColorBLUE);
  EXPECT_EQ(paragraph.text(), u"red teal");
}

// ── Flow geometry ─────────────────────────────────────────────────────────

TEST(Flow, BlockFlowFillsRect) {
  BlockFlow flow(SkRect::MakeXYWH(10, 20, 300, 100));
  std::vector<LineInterval> out;
  ASSERT_TRUE(flow.lineIntervals(0, 20, 15, out));
  ASSERT_EQ(out.size(), 1u);
  EXPECT_FLOAT_EQ(out[0].origin.x(), 10);
  EXPECT_FLOAT_EQ(out[0].origin.y(), 35); // top + ascent
  EXPECT_FLOAT_EQ(out[0].length, 300);
  ASSERT_TRUE(flow.lineIntervals(4, 20, 15, out)); // last fitting line
  EXPECT_FALSE(flow.lineIntervals(5, 20, 15, out));
}

TEST(Flow, ExclusionSplitsLineAroundCircle) {
  ExclusionFlow flow(SkRect::MakeWH(400, 200));
  flow.shapes().push_back(
      {ExclusionFlow::Shape::kCircle, SkRect::MakeXYWH(150, 50, 100, 100), 0});

  std::vector<LineInterval> out;
  // A band through the circle's center: two intervals around x∈[100, 300].
  ASSERT_TRUE(flow.lineIntervals(4, 20, 15, out)); // band y=[80,100]
  ASSERT_EQ(out.size(), 2u);
  EXPECT_FLOAT_EQ(out[0].origin.x(), 0);
  EXPECT_LE(out[0].length, 150.0f);
  EXPECT_GE(out[1].origin.x(), 250.0f - 1.0f);

  // A band fully above the circle: one full-width interval.
  ASSERT_TRUE(flow.lineIntervals(0, 20, 15, out));
  ASSERT_EQ(out.size(), 1u);
  EXPECT_FLOAT_EQ(out[0].length, 400);
}

TEST(Flow, ExclusionRectBlocksWholeBand) {
  ExclusionFlow flow(SkRect::MakeWH(300, 100));
  flow.shapes().push_back(
      {ExclusionFlow::Shape::kRect, SkRect::MakeXYWH(0, 30, 300, 20), 0});
  std::vector<LineInterval> out;
  ASSERT_TRUE(flow.lineIntervals(1, 25, 18, out)); // band [25,50] overlaps
  EXPECT_TRUE(out.empty());
}

// ── ParagraphLayout: greedy
// ────────────────────────────────────────────────────────

namespace {

SkPath pentagramPath(SkPoint center, float radius, SkPathFillType fillType) {
  SkPathBuilder builder;
  for (int pointIndex = 0; pointIndex < 5; ++pointIndex) {
    // Every second vertex of a pentagon: the classic self-intersecting star.
    const float angle = -std::numbers::pi_v<float> / 2.0f +
                        static_cast<float>(pointIndex) * 4.0f *
                            std::numbers::pi_v<float> / 5.0f;
    const SkPoint point = {center.x() + radius * std::cos(angle),
                           center.y() + radius * std::sin(angle)};
    if (pointIndex == 0)
      builder.moveTo(point);
    else
      builder.lineTo(point);
  }
  builder.close();
  builder.setFillType(fillType);
  return builder.detach();
}

} // namespace

TEST(Flow, PathExclusionRespectsFillRule) {
  // A pentagram's centre is winding-filled but even-odd-hollow; the band
  // through the centre must block or stay open accordingly.
  const SkPoint center = {200, 150};
  auto centerIntervals = [&](SkPathFillType fillType) {
    ExclusionFlow flow(SkRect::MakeWH(400, 300));
    flow.shapes().push_back(
        ExclusionFlow::Shape::fromPath(pentagramPath(center, 100, fillType)));
    std::vector<LineInterval> out;
    // Line band [145, 155] straddles the star's centre.
    EXPECT_TRUE(flow.lineIntervals(29, 5, 4, out));
    return out;
  };

  const std::vector<LineInterval> winding =
      centerIntervals(SkPathFillType::kWinding);
  // Solid star: just the two stretches left and right of it.
  ASSERT_EQ(winding.size(), 2u);
  EXPECT_LT(winding[0].origin.x() + winding[0].length, center.x() - 55);
  EXPECT_GT(winding[1].origin.x(), center.x() + 55);

  const std::vector<LineInterval> evenOdd =
      centerIntervals(SkPathFillType::kEvenOdd);
  // Hollow centre pentagon: a third interval opens up inside the star.
  ASSERT_EQ(evenOdd.size(), 3u);
  EXPECT_GT(evenOdd[1].origin.x(), center.x() - 40);
  EXPECT_LT(evenOdd[1].origin.x() + evenOdd[1].length, center.x() + 40);
}

TEST(Flow, CompoundPathKeepsHoleAvailable) {
  // Donut: two circles, even-odd — text may flow inside the hole.
  SkPathBuilder builder;
  builder.addCircle(200, 150, 100);
  builder.addCircle(200, 150, 50);
  builder.setFillType(SkPathFillType::kEvenOdd);

  ExclusionFlow flow(SkRect::MakeWH(400, 300));
  flow.shapes().push_back(ExclusionFlow::Shape::fromPath(builder.detach()));
  std::vector<LineInterval> out;
  ASSERT_TRUE(flow.lineIntervals(14, 10, 8, out)); // band [140, 150]
  ASSERT_EQ(out.size(), 3u);
  // Middle interval sits inside the hole (|x - 200| < 50 at this height).
  EXPECT_GT(out[1].origin.x(), 145.0f);
  EXPECT_LT(out[1].origin.x() + out[1].length, 255.0f);
  EXPECT_GT(out[1].length, 60.0f);
}

TEST(Flow, PathExclusionTipsBetweenScanlinesStillBlock) {
  // A left-pointing wedge whose tip falls strictly between the band's
  // sample scanlines: the conservative edge-extent union must still block
  // the tip's x-range.
  SkPathBuilder builder;
  builder.moveTo(100, 103); // tip at y=103, inside band [100, 110] but off
  builder.lineTo(300, 96);  // the top/mid/bottom sample lines
  builder.lineTo(300, 111);
  builder.close();

  ExclusionFlow flow(SkRect::MakeWH(400, 300));
  flow.shapes().push_back(ExclusionFlow::Shape::fromPath(builder.detach()));
  std::vector<LineInterval> out;
  ASSERT_TRUE(flow.lineIntervals(10, 10, 8, out)); // band [100, 110]
  ASSERT_FALSE(out.empty());
  // Nothing may be placed across the tip: the first interval ends at or
  // before x=100.
  EXPECT_LE(out[0].origin.x() + out[0].length, 100.0f + 0.5f);
}

TEST(Flow, PathExclusionOffsetMovesWithoutReflatten) {
  SkPath star = pentagramPath({200, 150}, 100, SkPathFillType::kWinding);
  ExclusionFlow flow(SkRect::MakeWH(800, 300));
  flow.shapes().push_back(ExclusionFlow::Shape::fromPath(star));

  std::vector<LineInterval> at0, at300;
  ASSERT_TRUE(flow.lineIntervals(29, 5, 4, at0));
  flow.shapes()[0].pathOffset = {300, 0};
  ASSERT_TRUE(flow.lineIntervals(29, 5, 4, at300));
  ASSERT_EQ(at0.size(), at300.size());
  for (size_t intervalIndex = 0; intervalIndex < at0.size(); ++intervalIndex) {
    // Interval edges bordering the star shift by exactly the offset; the
    // flow-rect edges stay put.
    const float end0 =
        at0[intervalIndex].origin.x() + at0[intervalIndex].length;
    const float end300 =
        at300[intervalIndex].origin.x() + at300[intervalIndex].length;
    EXPECT_TRUE(at300[intervalIndex].origin.x() ==
                    at0[intervalIndex].origin.x() ||
                std::abs(at300[intervalIndex].origin.x() -
                         (at0[intervalIndex].origin.x() + 300)) < 0.01f);
    EXPECT_TRUE(end300 == end0 || std::abs(end300 - (end0 + 300)) < 0.01f);
  }
}

TEST(Flow, RunsNeverEnterExclusionShapes) {
  // The gallery scene, distilled: mixed Latin/CJK justified text flowing
  // around a drifting donut and circle. Every placed run must stay inside
  // one of its line's intervals — text ending up *inside* a shape means
  // the breaker placed an overfull line into the gap beside it.
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      "Typography is the craft of arranging type, and glyphs flow around "
      "obstacles the way water flows around stones. 日本語のテキストも同じ"
      "流れに乗って進み、한국어 단어들도 자연스럽게 흐르고, 中文字符同样"
      "围绕形状排布。 Latin and CJK mix freely because every word is shaped "
      "once, cached, and repositioned with pure arithmetic frame after "
      "frame while the donut drifts back and forth across the column.");

  SkPathBuilder donutBuilder;
  donutBuilder.addCircle(430, 260, 150);
  donutBuilder.addCircle(430, 260, 75);
  donutBuilder.setFillType(SkPathFillType::kEvenOdd);
  const SkPath donutPath = donutBuilder.detach();

  const float lineHeight = 26, lineAscent = 20;
  for (int phase = 0; phase < 6; ++phase) {
    ExclusionFlow flow(SkRect::MakeWH(760, 900));
    ExclusionFlow::Shape donut = ExclusionFlow::Shape::fromPath(donutPath, 8);
    donut.pathOffset = {60.0f * std::sin(static_cast<float>(phase) * 1.1f),
                        70.0f * std::cos(static_cast<float>(phase) * 0.7f)};
    flow.shapes().push_back(donut);
    flow.shapes().push_back(ExclusionFlow::Shape::fromCircle(
        SkRect::MakeXYWH(80.0f + 20.0f * static_cast<float>(phase), 480, 180,
                         180),
        8));

    for (LineBreakStrategy breaker :
         {LineBreakStrategy::kGreedy, LineBreakStrategy::kKnuthPlass}) {
      ParagraphLayoutOptions options;
      options.lineBreakStrategy = breaker;
      options.alignment = TextAlignment::kJustify;
      options.lineMetrics.height = lineHeight;
      options.lineMetrics.ascent = lineAscent;
      ParagraphLayout layout =
          layoutParagraph(fontContext, paragraph, flow, options);
      EXPECT_FALSE(layout.overflowed());

      std::vector<LineInterval> intervals;
      for (const PositionedRun &run : layout.runs) {
        if (!run.shaped)
          continue;
        ASSERT_TRUE(flow.lineIntervals(run.lineIndex, lineHeight, lineAscent,
                                       intervals));
        const float runStartX = run.origin.x();
        const float runEndX = runStartX + run.shaped->advance;
        bool inside = false;
        for (const LineInterval &interval : intervals)
          inside = inside ||
                   (runStartX >= interval.origin.x() - 0.75f &&
                    runEndX <= interval.origin.x() + interval.length + 0.75f);
        EXPECT_TRUE(inside)
            << "run on line " << run.lineIndex << " spans [" << runStartX
            << ", " << runEndX << "] outside every interval (breaker "
            << (breaker == LineBreakStrategy::kGreedy ? "greedy"
                                                      : "knuth-plass")
            << ", phase " << phase << ")";
      }
    }
  }
}

TEST(ParagraphLayout, GreedyLinesRespectWidth) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      "one two three four five six seven eight nine ten eleven twelve "
      "thirteen fourteen fifteen sixteen seventeen eighteen nineteen twenty");
  BlockFlow flow(SkRect::MakeWH(200, 600));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  EXPECT_FALSE(layout.overflowed());
  EXPECT_GT(layout.lineCount, 1);
  for (const PositionedRun &run : layout.runs)
    EXPECT_LE(runEnd(paragraph, run), 200.0f + 2.0f)
        << "run leaks past the right edge";
}

TEST(ParagraphLayout, MandatoryBreakStartsNewLine) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph("alpha\nbeta");
  BlockFlow flow(SkRect::MakeWH(500, 300));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
  ASSERT_EQ(layout.runs.size(), 2u);
  EXPECT_NE(layout.runs[0].lineIndex, layout.runs[1].lineIndex);
  EXPECT_LT(layout.runs[0].origin.y(), layout.runs[1].origin.y());
}

TEST(ParagraphLayout, CenterAndEndAlignment) {
  FontContext &fontContext = sharedContext();
  ParagraphLayoutOptions options;

  Paragraph paragraph = makeParagraph("word");
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
      "justification stretches the spaces between words so every full line "
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
      "text flows around the shape and continues on the far side of it, "
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
  Paragraph paragraph = makeParagraph("words on custom lines flow freely");

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
  Paragraph paragraph = makeParagraph("diagonal");
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
  Paragraph paragraph = makeParagraph("around and around and around it goes");
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
    Paragraph paragraph = makeParagraph("curvature compensation", 40.0f);
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

TEST(ParagraphLayout, OverflowReportsFirstUnplacedWord) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      "far more text than could ever fit inside such a tiny little box");
  BlockFlow flow(SkRect::MakeWH(120, 40));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
  EXPECT_TRUE(layout.overflowed());
  EXPECT_GT(layout.firstUnplacedWord, 0u);
}

// ── ParagraphLayout: Knuth-Plass
// ───────────────────────────────────────────────────

namespace {

// Sum of squared leftover space across all full lines — the raggedness
// measure Knuth-Plass style breaking should not lose to greedy on.
float raggedness(const Paragraph &paragraph, const ParagraphLayout &layout,
                 float measure) {
  if (layout.lineCount <= 1)
    return 0;
  std::vector<float> lineEnds(static_cast<size_t>(layout.lineCount), 0.0f);
  for (const PositionedRun &run : layout.runs)
    lineEnds[static_cast<size_t>(run.lineIndex)] = std::max(
        lineEnds[static_cast<size_t>(run.lineIndex)], runEnd(paragraph, run));
  float total = 0;
  for (int line = 0; line + 1 < layout.lineCount; ++line) {
    const float slack = measure - lineEnds[static_cast<size_t>(line)];
    total += slack * slack;
  }
  return total;
}

} // namespace

TEST(KnuthPlass, ProducesValidLines) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      "In olden times when wishing still helped one, there lived a king "
      "whose daughters were all beautiful; and the youngest was so beautiful "
      "that the sun itself, which has seen so much, was astonished whenever "
      "it shone in her face.");
  BlockFlow flow(SkRect::MakeWH(300, 900));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  options.alignment = TextAlignment::kJustify;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);

  EXPECT_FALSE(layout.overflowed());
  EXPECT_GT(layout.lineCount, 3);
  for (const PositionedRun &run : layout.runs)
    EXPECT_LE(runEnd(paragraph, run), 300.0f + 3.0f);
  // Words appear in order (logical == visual for pure-LTR text).
  std::vector<uint32_t> seen;
  for (const PositionedRun &run : layout.runs)
    seen.push_back(run.wordIndex);
  EXPECT_TRUE(std::is_sorted(seen.begin(), seen.end()));
}

TEST(KnuthPlass, NoWorseRaggednessThanGreedy) {
  FontContext &fontContext = sharedContext();
  const char *tale =
      "It was the best of times, it was the worst of times, it was the age "
      "of wisdom, it was the age of foolishness, it was the epoch of belief, "
      "it was the epoch of incredulity, it was the season of Light, it was "
      "the season of Darkness, it was the spring of hope, it was the winter "
      "of despair.";
  const float measure = 320;

  Paragraph paragraph = makeParagraph(tale);
  BlockFlow greedyFlow(SkRect::MakeWH(measure, 2000));
  ParagraphLayout greedyLayout =
      layoutParagraph(fontContext, paragraph, greedyFlow); // ragged-right

  BlockFlow knuthPlassFlow(SkRect::MakeWH(measure, 2000));
  ParagraphLayoutOptions knuthPlassOptions;
  knuthPlassOptions.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  ParagraphLayout knuthPlassLayout = layoutParagraph(
      fontContext, paragraph, knuthPlassFlow, knuthPlassOptions);

  EXPECT_FALSE(knuthPlassLayout.overflowed());
  EXPECT_LE(raggedness(paragraph, knuthPlassLayout, measure),
            raggedness(paragraph, greedyLayout, measure) * 1.05f);
}

TEST(KnuthPlass, JustifiedCjkParagraph) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      "吾輩は猫である。名前はまだ無い。どこで生れたかとんと見当がつかぬ。"
      "何でも薄暗いじめじめした所でニャーニャー泣いていた事だけは記憶している"
      "。");
  BlockFlow flow(SkRect::MakeWH(280, 600));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  options.alignment = TextAlignment::kJustify;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);

  EXPECT_FALSE(layout.overflowed());
  EXPECT_GT(layout.lineCount, 2);
  for (const PositionedRun &run : layout.runs)
    EXPECT_LE(runEnd(paragraph, run), 280.0f + 3.0f);
}

// ── Incremental behavior end-to-end ───────────────────────────────────────

TEST(Incremental, OneWordEditKeepsOtherWordBlobs) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      "steady text with one word that will change between frames while all "
      "other words keep their shaped blobs perfectly intact");
  BlockFlow flow(SkRect::MakeWH(300, 600));
  ParagraphLayout before = layoutParagraph(fontContext, paragraph, flow);

  paragraph.replaceText(17, 20, "two"); // "one" → "two"
  ParagraphLayout after = layoutParagraph(fontContext, paragraph, flow);

  // Blobs are shared via the shape cache: unchanged words reuse the very
  // same SkTextBlob instances across the edit.
  size_t sharedBlobs = 0;
  for (const PositionedRun &beforeRun : before.runs)
    for (const PositionedRun &afterRun : after.runs)
      if (beforeRun.blob.get() == afterRun.blob.get())
        sharedBlobs++;
  EXPECT_GT(sharedBlobs, before.runs.size() / 2);
}

TEST(Incremental, MovingExclusionOnlyRepositions) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      "the shape moves through the paragraph and every frame the words "
      "reflow around it without any reshaping at all, just new positions");
  ExclusionFlow flow(SkRect::MakeWH(360, 400));
  flow.shapes().push_back(
      {ExclusionFlow::Shape::kCircle, SkRect::MakeXYWH(50, 30, 90, 90), 4});

  ParagraphLayout first = layoutParagraph(fontContext, paragraph, flow);
  fontContext.resetStats();
  flow.shapes()[0].bounds.offset(60, 25);
  ParagraphLayout second = layoutParagraph(fontContext, paragraph, flow);

  EXPECT_EQ(fontContext.stats().shapeCalls, 0u)
      << "moving a shape must not reshape";
  EXPECT_FALSE(second.runs.empty());
}

// ── Typographic options (last-line alignment, hyphenation, effects) ───────

TEST(Typography, LastLineAlignmentEnd) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      "a justified paragraph whose final line is pushed to the right edge "
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

TEST(Typography, SoftHyphenRendersHyphenOnBreak) {
  FontContext &fontContext = sharedContext();
  // "extra­ordinarily" fits neither whole nor as "extra" without the
  // discretionary break being taken on a narrow measure.
  Paragraph paragraph = makeParagraph("an extra­ordinarily narrow measure");
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

TEST(Typography, SoftHyphenInvisibleWhenNotBroken) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph("extra­ordinarily");
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

TEST(Typography, KnuthPlassUsesSoftHyphens) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph =
      makeParagraph("the as­ton­ish­ing­ly in­com­pre­hen"
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
      "a long sentence that will certainly wrap across several lines gets "
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
        spans[run.styleIndex].style.paint.color == SK_ColorRED) {
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
  Paragraph paragraph = makeParagraph("effects are paint-only");
  BlockFlow flow(SkRect::MakeWH(400, 100));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  fontContext.resetStats();
  PaintStyle fancy;
  fancy.color = SK_ColorWHITE;
  fancy.shadows.push_back({0x80000000, {3, 3}, 2.5f});
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

// ── Complex scripts & grapheme clusters ───────────────────────────────────

namespace {

// True when every glyph in the paragraph resolved to a real glyph (no
// .notdef): the script actually shaped with a covering font.
bool allGlyphsResolved(const Paragraph &paragraph) {
  for (const Word &word : paragraph.words())
    for (const WordSegment &seg : word.segments)
      for (uint16_t glyph : seg.shaped->glyphs)
        if (glyph == 0)
          return false;
  return true;
}

size_t uniqueClusterCount(const ShapedWord &shapedWord) {
  absl::flat_hash_set<uint32_t> unique(shapedWord.clusters.begin(),
                                       shapedWord.clusters.end());
  return unique.size();
}

} // namespace

TEST(KnuthPlass, LinesNeverExceedTheMeasure) {
  FontContext &fontContext = sharedContext();
  // Hyphen-laden text (the gallery scene) across a breathing measure: no
  // placed line may stick out past the measure — overfull breaks are
  // infeasible unless there is truly no alternative.
  Paragraph paragraph;
  paragraph.appendText(
      "The para­graph breaker con­sid­ers every way to break "
      "this text into lines and picks the one with the least bad­ness, "
      "ex­act­ly like TeX. Greedy breaking com­mits line by "
      "line and leaves rag­ged, in­con­sis­tent "
      "spac­ing be­hind; op­ti­mal breaking spreads the "
      "slack across the whole para­graph in­stead.",
      basicStyle(17.0f));

  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
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

// ── Inline placeholders (pills / images woven into the flow) ─────────────

TEST(Placeholders, ReservesWidthInTheLine) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph;
  paragraph.appendText("before ", basicStyle());
  paragraph.appendPlaceholder({90, 20, 0}, basicStyle());
  paragraph.appendText(" after", basicStyle());

  BlockFlow flow(SkRect::MakeWH(600, 60)); // everything on one line
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  const auto rects = layout.placeholderRects(paragraph);
  ASSERT_EQ(rects.size(), 1u);
  EXPECT_FLOAT_EQ(rects[0].rect.width(), 90);
  EXPECT_FLOAT_EQ(rects[0].rect.height(), 20);

  // "after" starts past the slot's right edge.
  float afterX = -1;
  for (const PositionedRun &run : layout.runs) {
    if (run.placeholderIndex >= 0)
      continue;
    const Word &word = paragraph.words()[run.wordIndex];
    const std::u16string_view text(paragraph.text());
    if (text.substr(word.textBegin, 5) == u"after")
      afterX = run.origin.x();
  }
  ASSERT_GE(afterX, 0);
  EXPECT_GE(afterX, rects[0].rect.right() - 0.25f);
}

TEST(Placeholders, SitOnTheBaselineWithDrop) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph;
  paragraph.appendText("x ", basicStyle());
  paragraph.appendPlaceholder({40, 30, 8},
                              basicStyle()); // bottom 8px below base
  BlockFlow flow(SkRect::MakeWH(300, 60));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  float baselineY = -1;
  for (const PositionedRun &run : layout.runs)
    if (run.placeholderIndex < 0)
      baselineY = run.origin.y();
  const auto rects = layout.placeholderRects(paragraph);
  ASSERT_EQ(rects.size(), 1u);
  EXPECT_FLOAT_EQ(rects[0].rect.bottom(), baselineY + 8);
  EXPECT_FLOAT_EQ(rects[0].rect.top(), baselineY + 8 - 30);
}

TEST(Placeholders, WrapAndJustifyLikeWords) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph;
  for (int placeholderIndex = 0; placeholderIndex < 6; ++placeholderIndex) {
    paragraph.appendText("word word word ", basicStyle());
    paragraph.appendPlaceholder({60, 14, 0}, basicStyle());
    paragraph.appendText(" ", basicStyle());
  }
  BlockFlow flow(SkRect::MakeWH(220, 400));
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);

  const auto rects = layout.placeholderRects(paragraph);
  ASSERT_EQ(rects.size(), 6u);
  absl::flat_hash_set<int> lines;
  for (const auto &placed : rects) {
    lines.insert(placed.lineIndex);
    // Slots never overflow the measure.
    EXPECT_GE(placed.rect.left(), -0.25f);
    EXPECT_LE(placed.rect.right(), 220.5f);
  }
  EXPECT_GT(lines.size(), 1u) << "slots must wrap onto later lines";
}

TEST(Placeholders, ResizeRelayoutsLive) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph;
  paragraph.appendText("pill: ", basicStyle());
  paragraph.appendPlaceholder({50, 16, 0}, basicStyle());
  BlockFlow flow(SkRect::MakeWH(400, 60));
  ParagraphLayout before = layoutParagraph(fontContext, paragraph, flow);

  fontContext.resetStats();
  paragraph.setPlaceholder(0, {120, 16, 0});
  ParagraphLayout after = layoutParagraph(fontContext, paragraph, flow);
  EXPECT_EQ(fontContext.stats().shapeCalls, 0u)
      << "resizing a slot reshapes nothing";
  EXPECT_FLOAT_EQ(after.placeholderRects(paragraph)[0].rect.width(), 120);
}

// ── Typographic correctness (Blink/HarfBuzzShaperTest-inspired) ──────────
// The invariants Chrome's text stack enforces in its shaper/layout unit
// tests, adapted to TextFlow's word model.

TEST(Correctness, VariableAxesReachHarfBuzz) {
  // A variable instance must shape with the same design position Skia
  // rasterizes: identical advances across weights would mean HarfBuzz
  // silently shaped the default instance.
  FontContext &fontContext = sharedContext();
  sk_sp<SkTypeface> base = fontContext.fontManager()->matchFamilyStyle(
      "Noto Sans", SkFontStyle::Normal());
  if (!base || base->getVariationDesignPosition({}) <= 0)
    GTEST_SKIP() << "no variable Noto Sans installed";

  const SkFontArguments::VariationPosition::Coordinate heavy{
      SkSetFourByteTag('w', 'g', 'h', 't'), 900.0f};
  SkFontArguments args;
  args.setVariationDesignPosition({&heavy, 1});
  sk_sp<SkTypeface> black = base->makeClone(args);
  ASSERT_TRUE(black);

  constexpr std::string_view kText = "hamburgefonstiv";
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
  const float bold = shapedAdvance(black);
  EXPECT_GT(bold, regular * 1.01f) << "wght 900 should shape wider than 400";

  // HarfBuzz's advances agree with what Skia measures for that instance.
  const SkScalar measured =
      makeFont(black, 32.0f)
          .measureText(kText.data(), kText.size(), SkTextEncoding::kUTF8);
  EXPECT_NEAR(bold, measured, bold * 0.015f);
}

TEST(Correctness, ClusterCoverageIsComplete) {
  FontContext &fontContext = sharedContext();
  // Ligating Latin, joining Arabic, conjunct Devanagari, ZWJ emoji.
  const char *samples[] = {"office", "العربية", "नमस्ते", "👨‍👩‍👧"};
  for (const char *sample : samples) {
    Paragraph paragraph = makeParagraph(sample);
    paragraph.ensureShaped(fontContext);
    for (const Word &word : paragraph.words())
      for (const WordSegment &seg : word.segments) {
        const auto &clusters = seg.shaped->clusters;
        ASSERT_FALSE(clusters.empty()) << sample;
        const size_t segLen = seg.shaped->glyphs.size();
        // Every cluster index points inside the shaped text, and the run
        // starts at offset 0 from one end (LTR: front, RTL: back).
        const uint32_t first = std::min(clusters.front(), clusters.back());
        EXPECT_EQ(first, 0u) << sample;
        for (uint32_t cluster : clusters)
          EXPECT_LT(cluster, word.textEnd - word.textBegin) << sample;
        EXPECT_GT(segLen, 0u);
      }
  }
}

TEST(Correctness, ZwnjBlocksArabicJoining) {
  FontContext &fontContext = sharedContext();
  auto glyphsOf = [&](const char *text) {
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
  EXPECT_NE(glyphsOf("بب"), glyphsOf("ب‌ب"));
}

TEST(Correctness, CombiningMarkAttachesToBase) {
  FontContext &fontContext = sharedContext();
  Paragraph nfc = makeParagraph("café"); // é precomposed
  Paragraph nfd = makeParagraph("café"); // e + combining acute
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

TEST(Correctness, KinsokuProhibitsLineInitialPunctuation) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph =
      makeParagraph("これは、禁則処理のテストです。行頭に句読点は来ない。");
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
  Paragraph spaced = makeParagraph("100 km");
  Paragraph glued = makeParagraph("100 km");
  spaced.ensureShaped(fontContext);
  glued.ensureShaped(fontContext);
  EXPECT_EQ(spaced.words().size(), 2u);
  EXPECT_EQ(glued.words().size(), 1u) << "NBSP must not be a break point";
}

TEST(Correctness, TabsMeasureAsSpaces) {
  FontContext &fontContext = sharedContext();
  Paragraph tab = makeParagraph("a\tb");
  Paragraph space = makeParagraph("a b");
  tab.ensureShaped(fontContext);
  space.ensureShaped(fontContext);
  ASSERT_EQ(tab.words().size(), 2u);
  EXPECT_FLOAT_EQ(tab.words()[0].spaceWidth, space.words()[0].spaceWidth);
}

TEST(Correctness, JustifiedShrinkNeverCollapsesSpaces) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      "several reasonably long words keep justification honest here", 18.0f);
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
  Paragraph paragraph = makeParagraph("aaa בבב גגג zzz", 16.0f);
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
  Paragraph paragraph = makeParagraph("metrics", 32.0f);
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
  Paragraph paragraph = makeParagraph("ab 𝕏𝕐 cd"); // 𝕏/𝕐 are surrogate pairs
  paragraph.ensureShaped(fontContext);
  // Cut straight through the middle of the first surrogate pair.
  const size_t textOffset = paragraph.text().find(u"ab");
  ASSERT_NE(textOffset, std::u16string::npos);
  paragraph.replaceText(4, 5, "Z");    // [4,5) is inside a pair for this string
  paragraph.ensureShaped(fontContext); // must not crash or emit garbage words
  for (const Word &word : paragraph.words())
    EXPECT_LE(word.textEnd, paragraph.text().size());
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph,
                      *std::make_unique<BlockFlow>(SkRect::MakeWH(400, 100)));
  EXPECT_FALSE(layout.runs.empty());
}

// ── Vertical writing mode ────────────────────────────────────────────────

TEST(Vertical, UprightCjkStacksDownColumns) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph;
  paragraph.appendText("縦書きのテキストは上から下へ流れる", basicStyle(20.0f));
  paragraph.setWritingMode(WritingMode::kVerticalRL);

  VerticalBlockFlow flow(SkRect::MakeWH(200, 220));
  ParagraphLayoutOptions options;
  options.lineMetrics.height = 30; // column pitch
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);

  ASSERT_FALSE(layout.runs.empty());
  EXPECT_GT(layout.lineCount, 1) << "16 chars at 20px must overflow one column";
  float prevYInLine0 = -1e9f;
  float line0x = 0, line1x = 0;
  for (const PositionedRun &run : layout.runs) {
    EXPECT_FALSE(run.transformed) << "upright CJK is a positioned blob";
    EXPECT_TRUE(run.shaped->vertical);
    if (run.lineIndex == 0) {
      line0x = run.origin.x();
      EXPECT_GT(run.origin.y(), prevYInLine0) << "pen must travel downward";
      prevYInLine0 = run.origin.y();
    } else if (run.lineIndex == 1) {
      line1x = run.origin.x();
    }
  }
  EXPECT_LT(line1x, line0x) << "columns must advance right to left";
}

TEST(Vertical, VertFeatureSubstitutesForms) {
  FontContext &fontContext = sharedContext();
  auto glyphsOf = [&](WritingMode mode) {
    Paragraph paragraph;
    paragraph.appendText("「縦組み」", basicStyle(20.0f));
    paragraph.setWritingMode(mode);
    paragraph.ensureShaped(fontContext);
    std::multiset<uint16_t> ids;
    for (const Word &word : paragraph.words())
      for (const WordSegment &segment : word.segments)
        for (uint16_t glyph : segment.shaped->glyphs)
          ids.insert(glyph);
    return ids;
  };
  // Vertical shaping must swap in 'vert' forms (rotated brackets at least).
  EXPECT_NE(glyphsOf(WritingMode::kHorizontal),
            glyphsOf(WritingMode::kVerticalRL));
}

TEST(Vertical, AutoRotatesLatinMixedIntoCjk) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph;
  paragraph.appendText("縦書きにHTTPが混ざる", basicStyle(20.0f));
  paragraph.setWritingMode(WritingMode::kVerticalRL);
  VerticalBlockFlow flow(SkRect::MakeWH(200, 400));
  ParagraphLayoutOptions options;
  options.lineMetrics.height = 30;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);

  bool sawUpright = false, sawRotated = false;
  for (const PositionedRun &run : layout.runs) {
    if (run.shaped->vertical)
      sawUpright = true;
    else if (run.transformed)
      sawRotated = true; // Latin baked as a rotated RSXform blob
  }
  EXPECT_TRUE(sawUpright);
  EXPECT_TRUE(sawRotated);
}

TEST(Vertical, TateChuYokoSetsRunUprightAcrossColumn) {
  FontContext &fontContext = sharedContext();
  TextStyle japaneseStyle = basicStyle(20.0f);
  TextStyle tcy = basicStyle(20.0f);
  tcy.shaping.verticalForm = VerticalForm::kTateChuYoko;

  Paragraph paragraph;
  paragraph.appendText("平成", japaneseStyle);
  paragraph.appendText("31", tcy);
  paragraph.appendText("年の縦組み", japaneseStyle);
  paragraph.setWritingMode(WritingMode::kVerticalRL);

  VerticalBlockFlow flow(SkRect::MakeWH(200, 400));
  ParagraphLayoutOptions options;
  options.lineMetrics.height = 30;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);

  const float axis = 200 - 30 * 0.5f; // first column's central axis
  const PositionedRun *tcyRun = nullptr;
  for (const PositionedRun &run : layout.runs)
    if (!run.shaped->vertical && !run.transformed)
      tcyRun = &run;
  ASSERT_NE(tcyRun, nullptr) << "the digit run must be placed upright";
  // Centred across the column: origin shifted left by half its advance.
  EXPECT_NEAR(tcyRun->origin.x(), axis - tcyRun->shaped->advance * 0.5f, 0.5f);
  // And it must not consume more column length than its font height (~23px
  // at 20px), far less than the two digits' horizontal advance would be if
  // they were stacked.
  const Word &word = paragraph.words()[std::min<size_t>(
      tcyRun->wordIndex, paragraph.words().size() - 1)];
  EXPECT_LT(word.width, 30.0f);
}

// ── Query layer (Query.h — optional, built on the Paragraph edit log) ────

TEST(Query, FindAllAndRegex) {
  Paragraph paragraph = makeParagraph("the cat sat on the mat, the end");
  const std::vector<CharRange> occurrences =
      findAllOccurrences(paragraph, "the");
  ASSERT_EQ(occurrences.size(), 3u);
  EXPECT_EQ(occurrences[0], (CharRange{0, 3}));
  EXPECT_EQ(occurrences[2], (CharRange{24, 27}));

  const auto matches = findRegexMatches(paragraph, "[cms]at");
  ASSERT_TRUE(matches.has_value());
  ASSERT_EQ(matches->size(), 3u);
  EXPECT_EQ((*matches)[0], (CharRange{4, 7}));   // cat
  EXPECT_EQ((*matches)[2], (CharRange{19, 22})); // mat

  EXPECT_FALSE(findRegexMatches(paragraph, "[unclosed").has_value());
}

TEST(Query, WordRangesMatchSegmentation) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph("one two three");
  const std::vector<CharRange> words = wordRanges(paragraph, fontContext);
  ASSERT_EQ(words.size(), 3u);
  EXPECT_EQ(words[0], (CharRange{0, 3}));
  EXPECT_EQ(words[1], (CharRange{4, 7}));
  EXPECT_EQ(words[2], (CharRange{8, 13}));
}

TEST(Query, MarkerSetFollowsEdits) {
  Paragraph paragraph = makeParagraph("alpha beta gamma delta");
  MarkerSet marks(paragraph);
  marks.setRanges("greek", findAllOccurrences(paragraph, "gamma")); // [11, 16)

  // Insert before the marker: it shifts.
  paragraph.replaceText(0, 0, ">>> ");
  ASSERT_TRUE(marks.synchronize(paragraph));
  ASSERT_EQ(marks.rangesFor("greek")->size(), 1u);
  EXPECT_EQ(marks.rangesFor("greek")->front(), (CharRange{15, 20}));

  // Replace text overlapping the marker: it absorbs the replacement.
  paragraph.replaceText(17, 22,
                        "MMA plus"); // ">>> alpha beta gaMMA plus delta"
  ASSERT_TRUE(marks.synchronize(paragraph));
  EXPECT_EQ(marks.rangesFor("greek")->front(), (CharRange{15, 25}));

  // Delete the whole marked range: the marker collapses and is dropped.
  paragraph.replaceText(15, 25, "");
  ASSERT_TRUE(marks.synchronize(paragraph));
  EXPECT_TRUE(marks.rangesFor("greek")->empty());
}

TEST(Query, MarkerSetStylesAcrossEdits) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph("keep the flame lit forever");
  MarkerSet marks(paragraph);
  marks.setRanges("flame", findAllOccurrences(paragraph, "flame"));

  paragraph.replaceText(0, 4, "guard"); // shifts the marker by +1
  PaintStyle red;
  red.color = 0xFFFF0000;
  marks.applyPaint(paragraph, "flame", red);
  paragraph.ensureShaped(fontContext);

  // The span carrying red must cover exactly "flame" in the new text.
  const std::u16string &text = paragraph.text();
  const size_t flameOffset = text.find(u"flame");
  ASSERT_NE(flameOffset, std::u16string::npos);
  bool found = false;
  for (const StyleSpan &span : paragraph.spans())
    if (span.style.paint.color == 0xFFFF0000) {
      EXPECT_EQ(span.start, static_cast<uint32_t>(flameOffset));
      EXPECT_EQ(span.end, static_cast<uint32_t>(flameOffset + 5));
      found = true;
    }
  EXPECT_TRUE(found);
}

TEST(Query, MarkerSetReportsHistoryLoss) {
  Paragraph paragraph = makeParagraph("word");
  MarkerSet marks(paragraph);
  marks.setRanges("w", findAllOccurrences(paragraph, "word"));
  // Blow past the bounded history (256 ops).
  for (int editIndex = 0; editIndex < 400; ++editIndex)
    paragraph.replaceText(0, 0, "x");
  EXPECT_FALSE(marks.synchronize(paragraph));
  EXPECT_TRUE(marks.rangesFor("w")->empty()); // cleared, caller must re-query
}

TEST(Query, ScopedSearchesStayInsideTheWindow) {
  //                            0123456789012345678901234567890
  Paragraph paragraph = makeParagraph("the cat sat on the mat, the end");

  // Substring search: offsets are absolute, matches before the window
  // drop out ("the" at 0), matches ending exactly at the edge stay.
  const std::vector<CharRange> occurrences =
      findAllOccurrences(paragraph, "the", {4, 27});
  ASSERT_EQ(occurrences.size(), 2u);
  EXPECT_EQ(occurrences[0], (CharRange{15, 18}));
  EXPECT_EQ(occurrences[1], (CharRange{24, 27}));
  // A match straddling the edge is not a match: "the" at 24 ends at 27,
  // one unit past end=26.
  const std::vector<CharRange> clipped =
      findAllOccurrences(paragraph, "the", {4, 26});
  ASSERT_EQ(clipped.size(), 1u);
  EXPECT_EQ(clipped[0], (CharRange{15, 18}));

  // Regex: same substring semantics, offsets absolute.
  const auto matches = findRegexMatches(paragraph, "[cms]at", {8, 22});
  ASSERT_TRUE(matches.has_value());
  ASSERT_EQ(matches->size(), 2u); // sat, mat — cat starts at 4, outside
  EXPECT_EQ((*matches)[0], (CharRange{8, 11}));
  EXPECT_EQ((*matches)[1], (CharRange{19, 22}));

  // The window's edges are text boundaries: ^ and $ anchor to them.
  const auto anchored = findRegexMatches(paragraph, "^sat", {8, 22});
  ASSERT_TRUE(anchored.has_value());
  ASSERT_EQ(anchored->size(), 1u);
  EXPECT_EQ((*anchored)[0], (CharRange{8, 11}));

  // Degenerate scopes clamp instead of tripping.
  EXPECT_TRUE(findAllOccurrences(paragraph, "the", {40, 90}).empty());
  EXPECT_TRUE(findRegexMatches(paragraph, "the", {27, 9000})->empty());
  const auto full = findRegexMatches(paragraph, "the", {0, 9000});
  ASSERT_TRUE(full.has_value());
  EXPECT_EQ(full->size(), 3u);
}

TEST(Query, BatchPaintMatchesSequentialPaint) {
  const char *text = "one two three four five six seven eight nine ten";
  Paragraph sequential = makeParagraph(text);
  Paragraph batched = makeParagraph(text);

  const std::vector<CharRange> words =
      findAllOccurrences(sequential, "e"); // scattered
  ASSERT_GT(words.size(), 3u);
  PaintStyle green;
  green.color = 0xFF00AA00;
  for (const CharRange &wordRange : words)
    sequential.setPaint(wordRange.start, wordRange.end, green);
  batched.setPaint(words, green);

  ASSERT_EQ(sequential.spans().size(), batched.spans().size());
  for (size_t spanIndex = 0; spanIndex < batched.spans().size(); ++spanIndex) {
    EXPECT_EQ(sequential.spans()[spanIndex].start,
              batched.spans()[spanIndex].start);
    EXPECT_EQ(sequential.spans()[spanIndex].end,
              batched.spans()[spanIndex].end);
    EXPECT_EQ(sequential.spans()[spanIndex].style.paint.color,
              batched.spans()[spanIndex].style.paint.color);
  }

  // Unsorted and overlapping input is sanitized, not trusted.
  Paragraph messy = makeParagraph(text);
  const std::array<CharRange, 4> ranges = {CharRange{8, 13}, CharRange{0, 3},
                                           CharRange{10, 17}, CharRange{3, 3}};
  messy.setPaint(ranges, green);
  uint32_t painted = 0;
  for (const StyleSpan &span : messy.spans())
    if (span.style.paint.color == green.color)
      painted += span.end - span.start;
  EXPECT_EQ(painted, 3u + 9u); // [0,3) + merged [8,17)
}

TEST(Query, PaintOnlyRestyleSkipsReanalysis) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      "the quick brown fox jumps over the lazy dog again and again");
  BlockFlow flow(SkRect::MakeWH(300, 200));
  ParagraphLayout before = layoutParagraph(fontContext, paragraph, flow);
  const uint32_t shapedBefore = paragraph.shapedWordCount();

  // Repaint word-aligned ranges: span boundaries land between words, so
  // every segment re-derives from the shape cache — zero new shape calls.
  const std::vector<CharRange> marks = findAllOccurrences(paragraph, "again");
  ASSERT_EQ(marks.size(), 2u);
  PaintStyle red;
  red.color = 0xFFCC0000;
  const uint64_t callsBefore = fontContext.stats().shapeCalls;
  paragraph.setPaint(marks, red);
  ParagraphLayout after = layoutParagraph(fontContext, paragraph, flow);
  EXPECT_EQ(fontContext.stats().shapeCalls, callsBefore);

  // Same geometry in, same geometry out — only paint moved.
  EXPECT_EQ(paragraph.shapedWordCount(), shapedBefore);
  ASSERT_EQ(after.runs.size(), before.runs.size());
  for (size_t runIndex = 0; runIndex < after.runs.size(); ++runIndex) {
    EXPECT_EQ(after.runs[runIndex].origin.x(),
              before.runs[runIndex].origin.x());
    EXPECT_EQ(after.runs[runIndex].origin.y(),
              before.runs[runIndex].origin.y());
  }
  // And the marked words resolve to the new paint through their runs.
  int redRuns = 0;
  for (const PositionedRun &run : after.runs)
    if (paragraph.spans()[run.styleIndex].style.paint.color == red.color)
      redRuns++;
  EXPECT_EQ(redRuns, 2);

  // Steady state (hue cycling the same ranges): the span boundaries come
  // out identical, so nothing is even paint-dirty — the existing layout's
  // styleIndices already resolve to the new color, no relayout required.
  PaintStyle blue;
  blue.color = 0xFF0000CC;
  paragraph.setPaint(marks, blue);
  EXPECT_FALSE(paragraph.needsShaping());
  int blueRuns = 0;
  for (const PositionedRun &run : after.runs)
    if (paragraph.spans()[run.styleIndex].style.paint.color == blue.color)
      blueRuns++;
  EXPECT_EQ(blueRuns, 2);
}

TEST(Query, PaintBoundaryMidWordSplitsSegments) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph("highlight");
  paragraph.ensureShaped(fontContext);
  ASSERT_EQ(paragraph.words().size(), 1u);
  ASSERT_EQ(paragraph.words()[0].segments.size(), 1u);
  const float whole = paragraph.words()[0].width;

  PaintStyle blue;
  blue.color = 0xFF0000CC;
  paragraph.setPaint(0, 4, blue); // "high" | "light"
  paragraph.ensureShaped(fontContext);

  const Word &word = paragraph.words()[0];
  ASSERT_EQ(word.segments.size(), 2u);
  EXPECT_EQ(paragraph.spans()[word.segments[0].styleIndex].style.paint.color,
            blue.color);
  EXPECT_NE(paragraph.spans()[word.segments[1].styleIndex].style.paint.color,
            blue.color);
  // Width comes back as the sum of the halves (kerning across the split
  // may drift a hair, nothing more).
  EXPECT_NEAR(word.width, whole, whole * 0.05f);
}

TEST(Scripts, ArabicLamAlefLigates) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph("لا"); // lam + alef: mandatory ligature
  paragraph.ensureShaped(fontContext);
  ASSERT_EQ(paragraph.words().size(), 1u);
  const ShapedWord &shapedWord = *paragraph.words()[0].segments[0].shaped;
  if (!allGlyphsResolved(paragraph))
    GTEST_SKIP() << "no Arabic font on this system";
  EXPECT_EQ(shapedWord.glyphs.size(), 1u)
      << "lam-alef must fuse into one glyph";
}

TEST(Scripts, ArabicJoinsRtl) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph("العربية تكتب من اليمين إلى اليسار");
  paragraph.ensureShaped(fontContext);
  if (!allGlyphsResolved(paragraph))
    GTEST_SKIP() << "no Arabic font on this system";
  ASSERT_GE(paragraph.words().size(), 5u);
  for (const Word &word : paragraph.words()) {
    EXPECT_EQ(word.bidiLevel & 1, 1) << "Arabic words must be RTL";
    const auto &clusters = word.segments[0].shaped->clusters;
    if (clusters.size() >= 2) // RTL visual order: clusters run backwards
      EXPECT_GT(clusters.front(), clusters.back());
  }
}

TEST(Scripts, DevanagariFormsConjunctClusters) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph("नमस्ते दुनिया");
  paragraph.ensureShaped(fontContext);
  if (!allGlyphsResolved(paragraph))
    GTEST_SKIP() << "no Devanagari font on this system";
  // "नमस्ते" is 6 UTF-16 units but the virama fuses स्+ते into one grapheme
  // cluster: distinct clusters must be fewer than code units.
  const Word &namaste = paragraph.words()[0];
  ASSERT_EQ(namaste.segments.size(), 1u);
  const ShapedWord &shapedWord = *namaste.segments[0].shaped;
  EXPECT_LT(uniqueClusterCount(shapedWord), 6u);
  EXPECT_GE(shapedWord.glyphs.size(), 3u);
}

TEST(Scripts, CuneiformSupplementaryPlane) {
  FontContext &fontContext = sharedContext();
  // Three codepoints beyond the BMP (U+12000, U+12038, U+1204D): each is a
  // surrogate pair, so correct cluster values step by 2 UTF-16 units.
  Paragraph paragraph = makeParagraph("𒀀𒀸𒁍");
  paragraph.ensureShaped(fontContext);
  if (!allGlyphsResolved(paragraph))
    GTEST_SKIP() << "no Cuneiform font on this system";
  std::vector<uint32_t> clusters;
  for (const Word &word : paragraph.words())
    for (const WordSegment &segment : word.segments)
      for (uint32_t cluster : segment.shaped->clusters)
        clusters.push_back(cluster + word.textBegin);
  ASSERT_FALSE(clusters.empty());
  for (uint32_t cluster : clusters)
    EXPECT_EQ(cluster % 2, 0u) << "clusters must land on surrogate-pair starts";
}

TEST(Scripts, EmojiZwjFamilyIsOneCluster) {
  FontContext &fontContext = sharedContext();
  // Family emoji: 4 people joined by ZWJ = 11 UTF-16 units, ONE grapheme.
  Paragraph paragraph = makeParagraph("👨‍👩‍👧‍👦");
  paragraph.ensureShaped(fontContext);
  ASSERT_EQ(paragraph.words().size(), 1u);
  ASSERT_EQ(paragraph.words()[0].segments.size(), 1u);
  const ShapedWord &shapedWord = *paragraph.words()[0].segments[0].shaped;
  ASSERT_FALSE(shapedWord.glyphs.empty());
  EXPECT_EQ(uniqueClusterCount(shapedWord), 1u)
      << "a ZWJ family sequence is a single grapheme cluster";
  EXPECT_TRUE(allGlyphsResolved(paragraph));
}

TEST(Scripts, EmojiModifierAndFlagClusters) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph("👍🏽 🇺🇸"); // skin tone; regional pair
  paragraph.ensureShaped(fontContext);
  ASSERT_EQ(paragraph.words().size(), 2u);
  for (const Word &word : paragraph.words()) {
    const ShapedWord &shapedWord = *word.segments[0].shaped;
    EXPECT_EQ(uniqueClusterCount(shapedWord), 1u)
        << "modifier/flag sequences are single grapheme clusters";
  }
  EXPECT_TRUE(allGlyphsResolved(paragraph));
}

TEST(Scripts, EmojiInsideLatinFallsBackPerSegment) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph("great👍work");
  paragraph.ensureShaped(fontContext);
  absl::flat_hash_set<const SkTypeface *> faces;
  for (const Word &word : paragraph.words())
    for (const WordSegment &segment : word.segments)
      faces.insert(segment.shaped->typeface.get());
  EXPECT_GE(faces.size(), 2u) << "emoji must resolve to its own typeface";
  EXPECT_TRUE(allGlyphsResolved(paragraph));
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
  ligaturesEnabledParagraph.appendText("official", ligaturesEnabledStyle);
  ligaturesDisabledParagraph.appendText("official", ligaturesDisabledStyle);
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

// ── Overflowing paragraphs must cost what fits, not what exists ───────────

TEST(Stress, HugeOverflowRelayoutIsBoundedByGeometry) {
  FontContext &fontContext = sharedContext();
  const char *wordPool[] = {"letters", "flow",     "around",  "boxes",
                            "while",   "the",      "breaker", "stops",
                            "at",      "geometry", "instead", "of",
                            "walking", "every",    "word"};
  std::mt19937 randomEngine(11);
  std::string text;
  for (int wordIndex = 0; wordIndex < 30000; ++wordIndex) {
    text += wordPool[randomEngine() % 15];
    text += ' ';
  }
  Paragraph paragraph;
  paragraph.appendText(text, basicStyle());
  BlockFlow flow(SkRect::MakeWH(420, 320)); // room for ~1% of the text

  for (LineBreakStrategy breakStrategy :
       {LineBreakStrategy::kGreedy, LineBreakStrategy::kKnuthPlass}) {
    ParagraphLayoutOptions options;
    options.lineBreakStrategy = breakStrategy;
    options.alignment = TextAlignment::kJustify;
    ParagraphLayout layout =
        layoutParagraph(fontContext, paragraph, flow, options); // warm shapes
    EXPECT_TRUE(layout.overflowed());
    EXPECT_GT(layout.runs.size(), 50u);
    EXPECT_LT(layout.firstUnplacedWord, 600u);

    // Warm relayout must not scale with the ~29,700 words that never fit
    // (verified: a 3k-word paragraph relayouts in the same time as this
    // 30k one). The bounds sit far above the geometry-bounded cost but
    // far below an O(total words) regression, which is ~50× slower here.
    const auto startTime = std::chrono::steady_clock::now();
    constexpr int kIterationCount = 20;
    for (int iteration = 0; iteration < kIterationCount; ++iteration)
      layout = layoutParagraph(fontContext, paragraph, flow, options);
    const double averageMicroseconds =
        std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - startTime)
            .count() /
        kIterationCount;
#ifdef NDEBUG
    const double maximumMicroseconds = 2000.0;
#else
    const double maximumMicroseconds =
        20000.0; // Debug: same work, ~20× the overhead
#endif
    EXPECT_LT(averageMicroseconds, maximumMicroseconds)
        << (breakStrategy == LineBreakStrategy::kGreedy ? "greedy"
                                                        : "knuth-plass")
        << " relayout scales with unplaced text";
  }
}

TEST(ParagraphLayout, EllipsisMarksOverflow) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      "far more text than a two line box can ever hope to hold so the "
      "marker has to step in and admit that the rest is missing");
  BlockFlow flow(SkRect::MakeWH(260, 44)); // ~2 lines
  ParagraphLayoutOptions options;
  options.overflow.ellipsis = u"…";

  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  ASSERT_TRUE(layout.overflowed());
  ASSERT_TRUE(layout.ellipsized);
  ASSERT_FALSE(layout.runs.empty());
  const PositionedRun &marker = layout.runs.back();
  ASSERT_TRUE(marker.shaped);
  // The marker sits at the end of the final line, inside the measure.
  EXPECT_EQ(marker.lineIndex, layout.runs[layout.runs.size() - 2].lineIndex);
  EXPECT_LE(marker.origin.x() + marker.shaped->advance, 260.0f + 0.75f);
  EXPECT_GT(marker.origin.x(), 0.0f);
  // Truncated words count as unplaced.
  for (const PositionedRun &run : layout.runs)
    if (&run != &marker)
      EXPECT_LT(run.wordIndex, layout.firstUnplacedWord);
}

TEST(ParagraphLayout, NoEllipsisWhenTextFits) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph("short and sweet");
  BlockFlow flow(SkRect::MakeWH(400, 200));
  ParagraphLayoutOptions options;
  options.overflow.ellipsis = u"…";

  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  EXPECT_FALSE(layout.overflowed());
  EXPECT_FALSE(layout.ellipsized);
}

TEST(Stress, OverflowShapesOnlyWhatFits) {
  // Lazy shaping: layout pulls HarfBuzz along its frontier, so the ~29k
  // words that never fit the box are itemized but never shaped. Every word
  // is unique so the content-addressed cache can't hide eager shaping.
  FontContext &fontContext = sharedContext();
  std::string text;
  for (int wordIndex = 0; wordIndex < 30000; ++wordIndex) {
    text += "word";
    text += std::to_string(wordIndex);
    text += ' ';
  }
  Paragraph paragraph;
  paragraph.appendText(text, basicStyle());
  BlockFlow flow(SkRect::MakeWH(420, 320));

  const uint64_t callsBefore = fontContext.stats().shapeCalls;
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  options.alignment = TextAlignment::kJustify;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  const uint64_t newShapeCallCount =
      fontContext.stats().shapeCalls - callsBefore;

  EXPECT_TRUE(layout.overflowed());
  EXPECT_GT(layout.runs.size(), 50u);
  EXPECT_GT(paragraph.shapedWordCount(), layout.runs.size() / 2);
  // ~600 placed words (+ glue + slack); eager shaping would be ~30,000.
  EXPECT_LT(newShapeCallCount, 3000u) << "overflow text was shaped eagerly";
  EXPECT_LT(paragraph.shapedWordCount(), 3000u);

  // Full shaping still available on demand (measurement, queries, …).
  paragraph.ensureShaped(fontContext);
  EXPECT_EQ(paragraph.shapedWordCount(), paragraph.words().size());
  EXPECT_GT(paragraph.naturalWidth(fontContext),
            400.0f * 300.0f); // ~30k words wide
}

TEST(Stress, KnuthPlassFullyPlacedIsLinear) {
  // A huge paragraph that fits *entirely* (10k words on screen). On a
  // uniform-width flow the breaker merges same-breakpoint paths (TeX's
  // one-measure model), so the active list stays bounded by the line width
  // and warm relayout stays linear — without the merge this case was ~20×
  // slower and grew super-linearly.
  FontContext &fontContext = sharedContext();
  const char *wordPool[15] = {"letters", "falling", "gently",  "against",
                              "words",   "beacon",  "steady",  "rhythm",
                              "turing",  "flow",    "lattice", "shapes",
                              "glyphs",  "marker",  "cache"};
  std::mt19937 randomEngine(11);
  std::string text;
  for (int wordIndex = 0; wordIndex < 10000; ++wordIndex) {
    text += wordPool[randomEngine() % 15];
    text += ' ';
  }
  Paragraph paragraph;
  paragraph.appendText(text, basicStyle());
  BlockFlow flow(SkRect::MakeWH(420, 40000)); // tall: everything fits
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  options.alignment = TextAlignment::kJustify;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options); // warm shapes
  ASSERT_FALSE(layout.overflowed());

  const auto startTime = std::chrono::steady_clock::now();
  constexpr int kIterationCount = 5;
  for (int iteration = 0; iteration < kIterationCount; ++iteration)
    layout = layoutParagraph(fontContext, paragraph, flow, options);
  const double averageMicroseconds =
      std::chrono::duration<double, std::micro>(
          std::chrono::steady_clock::now() - startTime)
          .count() /
      kIterationCount;
#ifdef NDEBUG
  const double maximumMicroseconds = 8000.0; // measured ~1.9ms
#else
  const double maximumMicroseconds =
      80000.0; // Debug: same work, ~20× the overhead
#endif
  EXPECT_LT(averageMicroseconds, maximumMicroseconds)
      << "KP active list grows with the paragraph";
}

TEST(Stress, PaintOnlyRestyleIsGeometryBounded) {
  // The marker workflow: repaint ranges every frame (hue cycling), relayout.
  // Paint edits must not re-run ICU analysis over the whole text or rebuild
  // spans once per range — cost stays bounded by the geometry, like the
  // relayout itself (see Stress.HugeOverflowRelayoutIsGeometryBounded).
  FontContext &fontContext = sharedContext();
  const char *wordPool[15] = {"letters", "falling", "gently",  "against",
                              "words",   "Beacon",  "steady",  "rhythm",
                              "Turing",  "flow",    "Lattice", "shapes",
                              "glyphs",  "Марка",   "cache"};
  std::mt19937 randomEngine(7);
  std::string text;
  for (int wordIndex = 0; wordIndex < 30000; ++wordIndex) {
    text += wordPool[randomEngine() % 15];
    text += ' ';
  }
  Paragraph paragraph;
  paragraph.appendText(text, basicStyle());
  BlockFlow flow(SkRect::MakeWH(420, 320)); // room for ~1% of the text
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow); // warm analysis + shapes
  ASSERT_TRUE(layout.overflowed());

  // Scoped query over the placed window only.
  const uint32_t placedEnd =
      paragraph.words()[layout.firstUnplacedWord].textBegin;
  const std::vector<CharRange> marks =
      findRegexMatches(paragraph, "\\b\\p{Lu}\\p{Ll}+", {0, placedEnd})
          .value_or(std::vector<CharRange>{});
  ASSERT_GT(marks.size(), 10u);

  const auto startTime = std::chrono::steady_clock::now();
  constexpr int kIterationCount = 20;
  for (int iteration = 0; iteration < kIterationCount; ++iteration) {
    PaintStyle hue;
    hue.color = 0xFF000000 | static_cast<uint32_t>(iteration * 1234567);
    paragraph.setPaint(marks, hue);
    layout = layoutParagraph(fontContext, paragraph, flow);
  }
  const double averageMicroseconds =
      std::chrono::duration<double, std::micro>(
          std::chrono::steady_clock::now() - startTime)
          .count() /
      kIterationCount;
#ifdef NDEBUG
  const double maximumMicroseconds = 3000.0;
#else
  const double maximumMicroseconds =
      30000.0; // Debug: same work, ~20× the overhead
#endif
  EXPECT_LT(averageMicroseconds, maximumMicroseconds)
      << "paint restyle scales with unplaced text";
}

// ── 2000-token multi-script confetti stress ───────────────────────────────

TEST(Stress, BabelConfetti2000) {
  FontContext &fontContext = sharedContext();
  const char *tokens[] = {"حرف",   "كلمة",   "अक्षर", "शब्द",   "אות",
                          "מילה",  "ตัวอักษร", "字",   "글",    "λόγος",
                          "буква", "🎉",     "👍🏽", "文字",  "ঢাকা",
                          "கடல்",   "ᚱᚢᚾ",    "ainm", "słowo", "λέξη"};
  std::mt19937 randomEngine(77);
  Paragraph paragraph;
  TextStyle style = basicStyle(18.0f);
  std::string text;
  for (int tokenIndex = 0; tokenIndex < 2000; ++tokenIndex) {
    text += tokens[randomEngine() % 20];
    text += ' ';
  }
  paragraph.appendText(text, style);

  LineSetFlow flow;
  for (int intervalIndex = 0; intervalIndex < 2000; ++intervalIndex) {
    const float angle = static_cast<float>(randomEngine() % 628) * 0.01f;
    flow.lines().push_back(
        {LineInterval{{20.0f + static_cast<float>(randomEngine() % 1360),
                       20.0f + static_cast<float>(randomEngine() % 860)},
                      {std::cos(angle), std::sin(angle)},
                      60}});
  }
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
  EXPECT_GT(layout.runs.size(), 1500u);
  EXPECT_GT(paragraph.words().size(), 1900u);

  // Nothing may leak a .notdef for scripts macOS covers (all of these).
  size_t unresolvedGlyphCount = 0;
  size_t totalGlyphCount = 0;
  for (const PositionedRun &run : layout.runs)
    for (uint16_t glyph : run.shaped->glyphs) {
      totalGlyphCount++;
      unresolvedGlyphCount += glyph == 0;
    }
  EXPECT_EQ(unresolvedGlyphCount, 0u)
      << unresolvedGlyphCount << " of " << totalGlyphCount
      << " glyphs unresolved";
}
