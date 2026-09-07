/** @file
 * Text set on a geometry of its own rather than on a block: an arbitrary
 * set of line segments, a turned line, a closed contour, and the interval
 * arithmetic underneath all of them — a phase walks a run round a closed
 * contour without a relayout, a geometrically closed contour wraps only
 * when flagged, an advance scale compresses the arc the text subtends, and
 * a negative one walks the contour backwards.
 */

#include <gtest/gtest.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>

#include <cmath>
#include <numbers>
#include <vector>

#include "support/Layouts.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

TEST(PathText, APhaseWalksTheRunRoundAClosedContourWithoutRelayout) {
  // The marquee, at the level the geometry provides it: one layout, and a
  // phase that re-places every glyph. Crossing the seam is the case that
  // must not fall off — the run walks through fraction 1 and out the other
  // side.
  auto [contour, length] = circleContour(150.0f);
  ASSERT_TRUE(contour.valid());
  LineInterval interval;
  interval.contour = contour;
  interval.length = length;

  SkPoint atZero, nearSeam, pastSeam;
  SkVector tangent;
  EXPECT_TRUE(interval.placeAt(0.0f, 0.0f, 0, &atZero, &tangent));
  EXPECT_TRUE(interval.placeAt(0.0f, length * 0.98f, 0, &nearSeam, &tangent));
  EXPECT_TRUE(interval.placeAt(0.0f, length * 1.02f, 0, &pastSeam, &tangent));
  // Every one of them is ON the circle…
  for (const SkPoint& p : {atZero, nearSeam, pastSeam})
    EXPECT_NEAR(std::hypot(p.x(), p.y()), 150.0f, 0.5f);
  // …and a phase past the seam is a short step from the phase before it,
  // not a jump back to the start.
  EXPECT_LT(SkPoint::Distance(nearSeam, pastSeam), length * 0.1f);
  EXPECT_GT(SkPoint::Distance(atZero, nearSeam), 1.0f);
}

TEST(PathText, AGeometricallyClosedContourWrapsWhenItSaysSo) {
  // A 359.9-degree arc is a common spelling of a ring and is NOT flagged
  // closed. Without the opt-in it clamps; with it, it wraps.
  SkPathBuilder arcBuilder;
  arcBuilder.addArc(SkRect::MakeLTRB(-100, -100, 100, 100), 0, 359.9f);
  const std::vector<sigil::geometry::path::Contour> contours =
      sigil::geometry::path::Contour::of(arcBuilder.detach());
  ASSERT_EQ(contours.size(), 1u);
  const sigil::geometry::path::Contour& contour = contours.front();
  ASSERT_FALSE(contour.closed());

  LineInterval clamping;
  clamping.contour = contour;
  clamping.length = contour.length();
  LineInterval wrapping = clamping;
  wrapping.wrapContour = true;

  SkPoint clamped, wrapped;
  SkVector tangent;
  EXPECT_FALSE(clamping.placeAt(-40.0f, 0.0f, 0, &clamped, &tangent))
      << "a pen before the start must report that it was clamped";
  EXPECT_TRUE(wrapping.placeAt(-40.0f, 0.0f, 0, &wrapped, &tangent));
  EXPECT_GT(SkPoint::Distance(clamped, wrapped), 10.0f);
  EXPECT_NEAR(std::hypot(wrapped.x(), wrapped.y()), 100.0f, 1.0f);
}

TEST(PathText, ANegativeAdvanceScaleWalksTheContourBackwards) {
  // How a run reads right way up along the lower half of a ring: the whole
  // run turns round once, rather than each letter turning over.
  auto [contour, length] = circleContour(120.0f);
  ASSERT_TRUE(contour.valid());
  LineInterval forward;
  forward.contour = contour;
  forward.length = length;
  LineInterval backward = forward;
  backward.advanceScale = -1.0f;
  backward.contourStart = 100.0f;

  SkPoint forwardPoint, backwardPoint;
  SkVector forwardTangent, backwardTangent;
  forward.placeAt(100.0f, 0.0f, 0, &forwardPoint, &forwardTangent);
  backward.placeAt(0.0f, 0.0f, 0, &backwardPoint, &backwardTangent);
  // Same point on the contour…
  EXPECT_NEAR(forwardPoint.x(), backwardPoint.x(), 0.01f);
  EXPECT_NEAR(forwardPoint.y(), backwardPoint.y(), 0.01f);
  // …faced the other way.
  EXPECT_NEAR(backwardTangent.x(), -forwardTangent.x(), 1e-4f);
  EXPECT_NEAR(backwardTangent.y(), -forwardTangent.y(), 1e-4f);
  // And the pen still travels forward through the text: a later pen sits
  // further BACK along the contour.
  SkPoint later;
  SkVector ignored;
  backward.placeAt(30.0f, 0.0f, 0, &later, &ignored);
  SkPoint earlierForward;
  forward.placeAt(70.0f, 0.0f, 0, &earlierForward, &ignored);
  EXPECT_NEAR(later.x(), earlierForward.x(), 0.01f);
  EXPECT_NEAR(later.y(), earlierForward.y(), 0.01f);
}

TEST(PathText, ALineSetPlacesEachLineOnTheSegmentItNamed) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"words on custom lines flow freely");

  LineSetFlow flow;
  flow.lines().push_back({LineInterval{{50, 40}, {1, 0}, 150}});
  flow.lines().push_back({LineInterval{{200, 90}, {1, 0}, 150}});
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  ASSERT_FALSE(layout.runs.empty());
  for (const PositionedRun& run : layout.runs) {
    if (run.lineIndex == 0) {
      EXPECT_FLOAT_EQ(run.origin.y(), 40);
      EXPECT_GE(run.origin.x(), 50);
    } else {
      EXPECT_FLOAT_EQ(run.origin.y(), 90);
      EXPECT_GE(run.origin.x(), 200);
    }
  }
}

TEST(PathText, ARunOnATurnedLineBakesItsPositionsIntoTheBlob) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"diagonal");
  const float inv = 1.0f / std::sqrt(2.0f);
  LineSetFlow flow;
  flow.lines().push_back({LineInterval{{0, 0}, {inv, inv}, 400}});
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
  ASSERT_EQ(layout.runs.size(), 1u);
  ASSERT_TRUE(layout.runs[0].transformed);
  // A transformed run carries no origin of its own: the glyphs are already
  // where they belong, marching down and to the right along the line the
  // interval named. On a line at forty-five degrees the run reaches about
  // its own advance over the root of two on each axis, so half its advance
  // is a floor no horizontal setting could clear on y.
  EXPECT_EQ(layout.runs[0].origin, (SkPoint{0, 0}));
  const float half = layout.runs[0].shaped->advance * 0.5f;
  const SkRect bounds = layout.runs[0].blob->bounds();
  EXPECT_GT(bounds.right(), half);
  EXPECT_GT(bounds.bottom(), half);
}

TEST(PathText, APathFlowLaysEveryRunAlongTheContour) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"around and around and around it goes");
  SkPath circle = SkPath::Circle(200, 200, 120);
  PathFlow flow(circle);
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  ASSERT_FALSE(layout.runs.empty());
  for (const PositionedRun& run : layout.runs) {
    const SkRect bounds = run.blob->bounds();
    const float horizontalOffset = bounds.centerX() - 200.0f;
    const float verticalOffset = bounds.centerY() - 200.0f;
    const float distanceFromCenter = std::sqrt(
        horizontalOffset * horizontalOffset + verticalOffset * verticalOffset);
    EXPECT_NEAR(distanceFromCenter, 120.0f, 40.0f)
        << "glyphs strayed off the circle";
  }
}

TEST(PathText, AnAdvanceScaleCompressesTheArcTheTextSubtends) {
  FontContext& fontContext = sigil::test::fonts();
  const std::vector<sigil::geometry::path::Contour> rings =
      sigil::geometry::path::Contour::of(SkPath::Circle(0, 0, 200));
  ASSERT_EQ(rings.size(), 1u);
  const sigil::geometry::path::Contour& ring = rings.front();

  // Same text on the same ring, once at natural arc consumption and once at
  // half — the half-scale layout's final word must sit at roughly half the
  // angle around the ring (pen starts at (200, 0) and marches clockwise).
  auto lastRunAngle = [&](float scale) {
    Paragraph paragraph = makeParagraph(u8"curvature compensation", 40.0f);
    LineInterval interval;
    interval.contour = ring;
    interval.length = ring.length() / scale;
    interval.advanceScale = scale;
    LineSetFlow flow;
    flow.lines().push_back({interval});
    ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
    EXPECT_FALSE(layout.runs.empty());
    const SkRect bounds = layout.runs.back().blob->bounds();
    float angle = std::atan2(bounds.centerY(), bounds.centerX());
    if (angle < 0) angle += 2.0f * std::numbers::pi_v<float>;
    return angle;
  };

  const float full = lastRunAngle(1.0f);
  const float half = lastRunAngle(0.5f);
  EXPECT_GT(full, half * 1.5f)
      << "advanceScale should compress the arc the text subtends";
  EXPECT_NEAR(full, half * 2.0f, full * 0.25f);
}
