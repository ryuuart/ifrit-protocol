/** @file
 * The two coordinate systems a figure is measured in — the polar Frame
 * and the unit-map Grid — and the centred rect both are read through.
 */

#include <gtest/gtest.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSize.h>

#include <cmath>
#include <vector>

#include "sigilgeometry/path/Frame.h"

using namespace sigil::geometry::path;

namespace {

::testing::AssertionResult near(SkPoint a, SkPoint b, float tol) {
  const float d = std::hypot(a.fX - b.fX, a.fY - b.fY);
  if (d <= tol) return ::testing::AssertionSuccess();
  return ::testing::AssertionFailure()
         << "(" << a.fX << ", " << a.fY << ") vs (" << b.fX << ", " << b.fY
         << ") — " << d << " px apart, tolerance " << tol;
}

// ---------------------------------------------------------------------------
// Frame — the figure-local polar coordinate system.

TEST(Frame, TheDefaultConventionIsTwelveOClockAndClockwise) {
  // What Frame promises without being told: 0° at 12 o'clock, increasing
  // clockwise, radius normalized. P() below is that spelled by hand, which
  // is what a figure would otherwise write inline at every call site.
  const Frame f{.centre = {100, 100}, .radius = 50};
  const auto P = [](float thDeg, float rNorm) {
    const float a = thDeg * 0.01745329252f;
    return SkPoint{100 + rNorm * 50 * std::sin(a),
                   100 - rNorm * 50 * std::cos(a)};
  };
  for (float th : {0.0f, 37.5f, 90.0f, 180.0f, 271.25f, 359.0f})
    for (float r : {0.25f, 1.0f})
      EXPECT_TRUE(near(f.at(th, r), P(th, r), 1e-3f)) << "th=" << th;

  // The four cardinals, spelled out, because a sign flip here is the
  // defect this value exists to stop.
  EXPECT_TRUE(near(f.at(0, 1), {100, 50}, 1e-4f));    // 12 o'clock
  EXPECT_TRUE(near(f.at(90, 1), {150, 100}, 1e-4f));  // 3 o'clock
  EXPECT_TRUE(near(f.at(180, 1), {100, 150}, 1e-4f));
  EXPECT_TRUE(near(f.at(270, 1), {50, 100}, 1e-4f));
}

TEST(Frame, EastAndCounterClockwiseAreTheOtherConventions) {
  const Frame east{.centre = {0, 0}, .radius = 1, .zero = Zero::East};
  EXPECT_TRUE(near(east.at(0, 1), {1, 0}, 1e-5f));
  EXPECT_TRUE(near(east.at(90, 1), {0, 1}, 1e-5f));  // screen-clockwise

  const Frame ccw{
      .centre = {0, 0}, .radius = 1, .zero = Zero::East, .sense = Sense::CCW};
  EXPECT_TRUE(near(ccw.at(90, 1), {0, -1}, 1e-5f));
}

TEST(Frame, TheConventionIsCarriedByTheValueAndNotByTheCallSite) {
  const Frame skiaLike{.centre = {100, 100},
                       .radius = 50,
                       .zero = Zero::East,
                       .sense = Sense::CW};
  const Frame plate{.centre = {100, 100},
                    .radius = 50,
                    .zero = Zero::North,
                    .sense = Sense::CW};
  // 0 degrees is due east in one and twelve o'clock in the other, and
  // that is the whole reason this is a value.
  EXPECT_FLOAT_EQ(skiaLike.skiaDeg(0), 0.0f);
  EXPECT_FLOAT_EQ(plate.skiaDeg(0), -90.0f);
  // A counter-clockwise plate turns the other way from the same zero.
  const Frame widdershins{.centre = {100, 100},
                          .radius = 50,
                          .zero = Zero::North,
                          .sense = Sense::CCW};
  EXPECT_FLOAT_EQ(widdershins.skiaDeg(90), -180.0f);
  EXPECT_FLOAT_EQ(plate.skiaDeg(90), 0.0f);
}

TEST(Frame, DegOfInvertsFractionThroughAnyOrigin) {
  // fraction() and its inverse are the one place the labelled contour's
  // start leaks, and they compose back to the angle they were given — mod
  // a turn, since a fraction has no memory of which lap it was on — for a
  // scan rotated off the frame's own zero as well as one on it.
  for (float originDeg : {0.0f, -3.2f, 41.0f}) {
    const Frame f{.centre = {0, 0}, .radius = 1, .originDeg = originDeg};
    for (float th : {5.0f, 37.5f, 120.0f, 180.0f, 359.0f}) {
      const float back = f.degOf(f.fraction(th));
      EXPECT_NEAR(std::fmod(back - th + 720.0f, 360.0f), 0.0f, 1e-2f)
          << "originDeg=" << originDeg << " th=" << th;
    }
  }
}

TEST(Frame, DerivedFramesKeepEveryConventionTheyCameFrom) {
  const Frame f{.centre = {10, 20},
                .radius = 80,
                .zero = Zero::North,
                .sense = Sense::CCW,
                .originDeg = 4.5f};
  const Frame inner = f.scaled(0.5f);
  EXPECT_FLOAT_EQ(inner.radius, 40.0f);
  EXPECT_EQ(inner.zero, f.zero);
  EXPECT_EQ(inner.sense, f.sense);
  EXPECT_FLOAT_EQ(inner.originDeg, f.originDeg);
  // Scaling the frame and scaling the radius name the same point.
  EXPECT_TRUE(near(inner.at(31.0f, 1.0f), f.at(31.0f, 0.5f), 1e-4f));
  EXPECT_EQ(f.about({0, 0}).sense, f.sense);

  // Turning composes and inverts, which is what makes a nudge safe, and a
  // half-division offset moves the point by half a division.
  EXPECT_TRUE(f.turned(9.0f).turned(-9.0f) == f);
  EXPECT_TRUE(near(f.turned(9.0f).at(0, 1), f.at(9.0f, 1), 1e-4f));
}

TEST(Frame, BoxIsTheSquareASilhouetteInscribesItselfIn) {
  const Frame f{.centre = {10, 20}, .radius = 80};
  EXPECT_EQ(f.box(0.5f), SkRect::MakeXYWH(10 - 40, 20 - 40, 80, 80));
  const Frame off{.centre = {50, 60}, .radius = 20};
  const SkRect b = off.box(0.5f);
  EXPECT_FLOAT_EQ(b.width(), 20);
  EXPECT_FLOAT_EQ(b.height(), 20);
  EXPECT_FLOAT_EQ(b.centerX(), 50);
  EXPECT_FLOAT_EQ(b.centerY(), 60);
}

TEST(Frame, PolarPointsLandWhereTheirDegreesSay) {
  const Frame f{
      .centre = {0, 0}, .radius = 100, .zero = Zero::North, .sense = Sense::CW};
  const SkPoint north = f.at(0, 1.0f);
  EXPECT_NEAR(north.fX, 0.0f, 1e-3f);
  EXPECT_NEAR(north.fY, -100.0f, 1e-3f);
  const SkPoint east = f.at(90, 1.0f);
  EXPECT_NEAR(east.fX, 100.0f, 1e-3f);
  EXPECT_NEAR(east.fY, 0.0f, 1e-3f);
}

// ---------------------------------------------------------------------------
// Grid — the unit map.

TEST(Grid, ALengthTakesNoOriginAndAPositionDoes) {
  const Grid g{.scale = 4.0f, .origin = {100, 50}};
  EXPECT_FLOAT_EQ(g.s(10), 40);   // a WIDTH
  EXPECT_FLOAT_EQ(g.x(10), 140);  // a POSITION
  EXPECT_FLOAT_EQ(g.y(10), 90);
  const SkRect r = g.rect(10, 10, 5, 5);
  EXPECT_FLOAT_EQ(r.fLeft, 140);
  EXPECT_FLOAT_EQ(r.width(), 20);
}

TEST(Grid, SnapRoundsTheResultAndTwoGridsCoexist) {
  // Grid is a value rather than a free snapping function precisely so one
  // figure can carry two of them — say a 4 px geometry grid and a 2.5 px
  // text grid — without either being global state.
  const Grid geo{.scale = 4.0f, .snap = 4.0f};
  const Grid type{.scale = 2.5f, .snap = 2.5f};
  EXPECT_FLOAT_EQ(geo.x(1.3f), 4.0f);   // 5.2 → 4
  EXPECT_FLOAT_EQ(type.x(1.3f), 2.5f);  // 3.25 → 2.5
  EXPECT_NE(geo.s(3), type.s(3));
  const Grid none{.scale = 4.0f};
  EXPECT_FLOAT_EQ(none.x(1.3f), 5.2f);
  // Snap rounds half away from zero: 0.7 units is 2.8 px, which is more
  // than half a 5 px step, and 0.6 is not.
  const Grid snapped{.scale = 4.0f, .origin = {0, 0}, .snap = 5.0f};
  EXPECT_FLOAT_EQ(snapped.x(0.7f), 5.0f);
  EXPECT_FLOAT_EQ(snapped.x(0.6f), 0.0f);
  // Scaling the map scales the unit and leaves the step it snaps to.
  EXPECT_FLOAT_EQ(snapped.scaled(0.5f).scale, 2.0f);
  EXPECT_FLOAT_EQ(snapped.scaled(0.5f).snap, 5.0f);
}

TEST(Grid, ARectIsSnappedAtBothEdges) {
  const Grid g{.scale = 1.0f, .snap = 4.0f};
  const SkRect r = g.rect(SkRect::MakeLTRB(1, 1, 11, 11));
  EXPECT_FLOAT_EQ(r.fLeft, 0);
  EXPECT_FLOAT_EQ(r.fRight, 12);
}

TEST(Grid, APolylineAndAMatrixCarryTheSameMap) {
  const Grid g{.scale = 2.0f, .origin = {5, 5}};
  const std::vector<SkPoint> units{{0, 0}, {1, 2}};
  const std::vector<SkPoint> px = g.map(units);
  ASSERT_EQ(px.size(), 2u);
  EXPECT_TRUE(near(px[1], {7, 9}, 1e-5f));
  SkPoint m = {1, 2};
  g.matrix().mapPoints({&m, 1});
  EXPECT_TRUE(near(m, {7, 9}, 1e-5f));
}

// ---------------------------------------------------------------------------
// The centred rect — the peer of both, and the one place `x - w * 0.5f` is
// written.

TEST(Arrange, CentredBuildsTheRectAroundAPoint) {
  const SkRect r = centred({100, 50}, 40, 20);
  EXPECT_EQ(r, SkRect::MakeXYWH(80, 40, 40, 20));
  EXPECT_EQ(centred({100, 50}, SkSize{40, 20}), r);
}

}  // namespace
