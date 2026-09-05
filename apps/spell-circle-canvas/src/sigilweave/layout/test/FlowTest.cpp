/** @file
 * FlowGeometry implementations: BlockFlow bands, ExclusionFlow
 * interval splitting (circles, rects, arbitrary SkPaths with fill
 * rules and holes), and the runs-never-enter-shapes invariant.
 */

#include <gtest/gtest.h>
#include <include/core/SkPathBuilder.h>

#include <cmath>
#include <numbers>
#include <vector>

#include "support/LayoutSupport.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

// ── Flow geometry ─────────────────────────────────────────────────────────

TEST(Flow, ABlockHandsOutOneFullWidthBandPerLineUntilItRunsOut) {
  BlockFlow flow(SkRect::MakeXYWH(10, 20, 300, 100));
  std::vector<LineInterval> out;
  ASSERT_TRUE(flow.lineIntervals(0, 20, 15, out));
  ASSERT_EQ(out.size(), 1u);
  EXPECT_FLOAT_EQ(out[0].origin.x(), 10);
  EXPECT_FLOAT_EQ(out[0].origin.y(), 35);  // top + ascent
  EXPECT_FLOAT_EQ(out[0].length, 300);
  ASSERT_TRUE(flow.lineIntervals(4, 20, 15, out));  // last fitting line
  EXPECT_FALSE(flow.lineIntervals(5, 20, 15, out));
}

TEST(Flow, ExclusionSplitsLineAroundCircle) {
  ExclusionFlow flow(SkRect::MakeWH(400, 200));
  flow.shapes().push_back(
      {ExclusionFlow::Shape::kCircle, SkRect::MakeXYWH(150, 50, 100, 100), 0});

  std::vector<LineInterval> out;
  // A band through the circle's center: two intervals around x∈[100, 300].
  ASSERT_TRUE(flow.lineIntervals(4, 20, 15, out));  // band y=[80,100]
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
  ASSERT_TRUE(flow.lineIntervals(1, 25, 18, out));  // band [25,50] overlaps
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

}  // namespace

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
  ASSERT_TRUE(flow.lineIntervals(14, 10, 8, out));  // band [140, 150]
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
  builder.moveTo(100, 103);  // tip at y=103, inside band [100, 110] but off
  builder.lineTo(300, 96);   // the top/mid/bottom sample lines
  builder.lineTo(300, 111);
  builder.close();

  ExclusionFlow flow(SkRect::MakeWH(400, 300));
  flow.shapes().push_back(ExclusionFlow::Shape::fromPath(builder.detach()));
  std::vector<LineInterval> out;
  ASSERT_TRUE(flow.lineIntervals(10, 10, 8, out));  // band [100, 110]
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

namespace {

/// A run landing inside a shape means the breaker put an overfull line
/// into the gap beside it, so the claim is one about breaking and both
/// breakers answer for it.
class ExcludedFlow : public BrokenBothWays {};

}  // namespace

TEST_P(ExcludedFlow, NoRunEverSitsInsideAnExclusionShape) {
  // Mixed Latin/CJK justified text flowing around a drifting donut and
  // circle. Every placed run must stay inside one of its line's intervals —
  // text ending up *inside* a shape means the breaker placed an overfull
  // line into the gap beside it.
  FontContext& fontContext = sigil::test::fonts();
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
  // Two placements of the same obstacles: one where the donut's hole opens
  // a second interval on a band, and one where the donut and the circle
  // overlap, which splits more bands than either shape does alone.
  for (int phase : {3, 4}) {
    ExclusionFlow flow(SkRect::MakeWH(760, 900));
    ExclusionFlow::Shape donut = ExclusionFlow::Shape::fromPath(donutPath, 8);
    donut.pathOffset = {60.0f * std::sin(static_cast<float>(phase) * 1.1f),
                        70.0f * std::cos(static_cast<float>(phase) * 0.7f)};
    flow.shapes().push_back(donut);
    flow.shapes().push_back(ExclusionFlow::Shape::fromCircle(
        SkRect::MakeXYWH(80.0f + 20.0f * static_cast<float>(phase), 480, 180,
                         180),
        8));

    ParagraphLayoutOptions options;
    options.lineBreakStrategy = breaker();
    options.alignment = TextAlignment::kJustify;
    options.lineMetrics.height = lineHeight;
    options.lineMetrics.ascent = lineAscent;
    ParagraphLayout layout =
        layoutParagraph(fontContext, paragraph, flow, options);
    EXPECT_FALSE(layout.overflowed());

    const IntervalContainment held = runsStayInsideIntervals(
        flow, layout, lineHeight, lineAscent, PenAxis::kAlongLines);
    EXPECT_GT(held.runs, 0);
    EXPECT_EQ(held.exhausted, 0)
        << "the flow refused a band it had already placed a run on";
    EXPECT_GT(held.splitBands, 0)
        << "the shapes split no band at all (phase " << phase << ")";
    EXPECT_EQ(held.outside, 0)
        << "a run on line " << held.outsideBand << " spans ["
        << held.outsideStart << ", " << held.outsideEnd
        << "] outside every interval (phase " << phase << ")";
  }
}

INSTANTIATE_TEST_SUITE_P(Breakers, ExcludedFlow, bothBreakers(), breakerName);

// ── The same exclusions met by a column ──────────────────────────────────

namespace {

/// The pen spans of one band, in the pen's own direction — the one answer
/// a flow gives, whichever way its lines run.
std::vector<std::pair<float, float>> penSpans(FlowGeometry& flow, int index,
                                              float pitch, float ascent,
                                              bool columns) {
  std::vector<LineInterval> intervals;
  std::vector<std::pair<float, float>> spans;
  if (!flow.lineIntervals(index, pitch, ascent, intervals)) return spans;
  for (const LineInterval& interval : intervals) {
    const float start = columns ? interval.origin.y() : interval.origin.x();
    spans.emplace_back(start, start + interval.length);
  }
  return spans;
}

/// The same rectangle a quarter turn later. A line flow's bands count DOWN
/// from the top and a column flow's count LEFT from the right, so a shape
/// sitting `a` from the top of one sits `a` from the right of the other,
/// and its along-coordinate simply changes axis.
SkRect turned(const SkRect& rect, float acrossExtent) {
  return SkRect::MakeXYWH(acrossExtent - rect.bottom(), rect.left(),
                          rect.height(), rect.width());
}

}  // namespace

TEST(Flow, AnExclusionCutsAColumnExactlyAsItCutsALine) {
  // A COLUMN IS A LINE TURNED A QUARTER TURN. Turn the geometry and the
  // shape together and every band must answer with the very same pen
  // spans — which is what makes this one implementation and not two.
  constexpr float kAlong = 400, kAcross = 200, kPitch = 20, kAscent = 15;
  const SkRect shape = SkRect::MakeXYWH(150, 50, 100, 60);

  ExclusionFlow lines(SkRect::MakeWH(kAlong, kAcross));
  lines.shapes().push_back(ExclusionFlow::Shape::fromRectangle(shape, 4));
  ExclusionFlow columns(SkRect::MakeWH(kAcross, kAlong), FlowAxis::kColumns);
  columns.shapes().push_back(
      ExclusionFlow::Shape::fromRectangle(turned(shape, kAcross), 4));

  bool sawASplit = false;
  for (int index = 0; index < 12; ++index) {
    const std::vector<std::pair<float, float>> lineSpans =
        penSpans(lines, index, kPitch, kAscent, false);
    const std::vector<std::pair<float, float>> columnSpans =
        penSpans(columns, index, kPitch, kAscent, true);
    ASSERT_EQ(lineSpans.size(), columnSpans.size()) << "band " << index;
    if (lineSpans.size() == 2) sawASplit = true;
    for (size_t span = 0; span < lineSpans.size(); ++span) {
      EXPECT_FLOAT_EQ(columnSpans[span].first, lineSpans[span].first);
      EXPECT_FLOAT_EQ(columnSpans[span].second, lineSpans[span].second);
    }
  }
  EXPECT_TRUE(sawASplit) << "the shape must have split some band in two";
}

TEST(Flow, AnExclusionShortensTheColumnItCrosses) {
  // The column's own reading of it: a shape over the head of a column
  // moves the pen down past it, and the column beside it runs clean.
  ExclusionFlow flow(SkRect::MakeWH(200, 300), FlowAxis::kColumns);
  flow.shapes().push_back(
      ExclusionFlow::Shape::fromRectangle(SkRect::MakeXYWH(160, 0, 40, 120)));

  std::vector<LineInterval> out;
  ASSERT_TRUE(flow.lineIntervals(0, 20, 0, out));  // column x ∈ [180, 200]
  ASSERT_EQ(out.size(), 1u);
  EXPECT_FLOAT_EQ(out[0].origin.x(), 190) << "the column's central axis";
  EXPECT_FLOAT_EQ(out[0].direction.x(), 0);
  EXPECT_FLOAT_EQ(out[0].direction.y(), 1);
  EXPECT_FLOAT_EQ(out[0].origin.y(), 120) << "the pen starts below the shape";
  EXPECT_FLOAT_EQ(out[0].length, 180);

  ASSERT_TRUE(flow.lineIntervals(2, 20, 0, out));  // clear of the shape
  ASSERT_EQ(out.size(), 1u);
  EXPECT_FLOAT_EQ(out[0].origin.y(), 0);
  EXPECT_FLOAT_EQ(out[0].length, 300);

  // Columns run out at the left edge, exactly as lines run out at the
  // bottom.
  EXPECT_FALSE(flow.lineIntervals(10, 20, 0, out));
}

TEST(Flow, ASilhouetteSplitsAColumn) {
  // The per-column twin of the silhouette scan: the same flattening and
  // the same fill rule, read DOWN the column. A donut standing in a
  // column's way leaves its head, its hole and its foot open.
  SkPathBuilder builder;
  builder.addCircle(150, 200, 100);
  builder.addCircle(150, 200, 50);
  builder.setFillType(SkPathFillType::kEvenOdd);

  ExclusionFlow flow(SkRect::MakeWH(300, 400), FlowAxis::kColumns);
  flow.shapes().push_back(ExclusionFlow::Shape::fromPath(builder.detach()));

  std::vector<LineInterval> out;
  ASSERT_TRUE(flow.lineIntervals(7, 20, 0, out));  // column x ∈ [140, 160]
  ASSERT_EQ(out.size(), 3u);
  for (const LineInterval& interval : out) {
    EXPECT_FLOAT_EQ(interval.origin.x(), 150) << "all three on one axis";
    EXPECT_FLOAT_EQ(interval.direction.y(), 1);
  }
  EXPECT_FLOAT_EQ(out[0].origin.y(), 0);
  EXPECT_LT(out[0].length, 105.0f) << "the head stops above the ring";
  EXPECT_GT(out[1].origin.y(), 145.0f) << "the middle one is the hole";
  EXPECT_LT(out[1].origin.y() + out[1].length, 255.0f);
  EXPECT_GT(out[2].origin.y(), 295.0f) << "the foot resumes below the ring";
  EXPECT_FLOAT_EQ(out[2].origin.y() + out[2].length, 400.0f);

  // A column clear of the donut runs the whole height.
  ASSERT_TRUE(flow.lineIntervals(0, 20, 0, out));
  ASSERT_EQ(out.size(), 1u);
  EXPECT_FLOAT_EQ(out[0].length, 400);
}
