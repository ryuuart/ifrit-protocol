/** @file
 * Placement on a contour interval, read by the layout and by a caller that
 * re-places its glyphs: a phase walks a run round a closed contour without
 * a relayout, a geometrically closed contour wraps only when flagged, and a
 * negative advance scale walks the contour backwards.
 */

#include <gtest/gtest.h>
#include <include/core/SkPathBuilder.h>

#include <cmath>
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
  const std::vector<sigil::geometry::Contour> contours =
      sigil::geometry::Contour::of(arcBuilder.detach());
  ASSERT_EQ(contours.size(), 1u);
  const sigil::geometry::Contour& contour = contours.front();
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
