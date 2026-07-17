/** @file
 * FlowGeometry implementations: BlockFlow bands, ExclusionFlow
 * interval splitting (circles, rects, arbitrary SkPaths with fill
 * rules and holes), and the runs-never-enter-shapes invariant.
 */

#include "TestSupport.h"

#include <gtest/gtest.h>

#include <include/core/SkPathBuilder.h>

#include <cmath>
#include <numbers>
#include <vector>
using namespace textflow;
using namespace textflow::test;

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
      u8"Typography is the craft of arranging type, and glyphs flow around "
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
