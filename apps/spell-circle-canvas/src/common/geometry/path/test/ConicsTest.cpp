/** @file
 * The conic about its focus: the four curves one value covers, where the
 * focus stands against the figure it draws, and what a span of one comes
 * back as.
 */

#include <gtest/gtest.h>
#include <include/core/SkPath.h>

#include <cmath>
#include <glm/geometric.hpp>

#include "sigilgeometry/path/Conic.h"

using namespace sigil::geometry::path;

namespace {

/** How far the drawn curve reaches from a point, and how near it comes —
 *  the two numbers that tell an ellipse from a circle without knowing how
 *  it was sampled. */
void extremes(const SkPath& path, glm::vec2 about, float* nearest,
              float* furthest) {
  *nearest = 1e30f;
  *furthest = 0;
  for (int i = 0; i < path.countPoints(); ++i) {
    const SkPoint p = path.getPoint(i);
    const float d = glm::length(glm::vec2{p.fX, p.fY} - about);
    *nearest = std::min(*nearest, d);
    *furthest = std::max(*furthest, d);
  }
}

}  // namespace

TEST(Conics, AtZeroEccentricityItIsACircleAboutTheFocusItself) {
  const Conic circle{.focus = {100, 50}, .semiLatus = 40};
  for (float v = -180; v <= 180; v += 15) {
    EXPECT_NEAR(circle.radiusAt(v), 40.0f, 1e-4f);
    // The direction of travel is square to the radius, which is true of
    // the circle and of no other conic.
    EXPECT_NEAR(glm::dot(circle.alongAt(v), circle.outwardAt(v)), 0.0f, 1e-5f);
  }
  float nearest = 0, furthest = 0;
  extremes(conicPath(circle, {.fromDeg = 0, .toDeg = 360}), {100, 50}, &nearest,
           &furthest);
  EXPECT_NEAR(nearest, 40.0f, 1e-3f);
  EXPECT_NEAR(furthest, 40.0f, 1e-3f);
}

TEST(Conics, AnEllipseStandsOffCentreBecauseTheFocusIsWhereItIsMeasuredFrom) {
  const Conic ellipse{.focus = {0, 0}, .semiLatus = 100, .eccentricity = 0.5f};
  // Near point at the periapsis, far point half a turn away: p/(1+e) and
  // p/(1-e). The centre of the figure is neither, which is the whole
  // point of measuring from a focus.
  EXPECT_NEAR(ellipse.radiusAt(0), 100.0f / 1.5f, 1e-3f);
  EXPECT_NEAR(ellipse.radiusAt(180), 100.0f / 0.5f, 1e-3f);
  EXPECT_TRUE(ellipse.closes());

  float nearest = 0, furthest = 0;
  extremes(conicPath(ellipse, {.fromDeg = 0, .toDeg = 360}), {0, 0}, &nearest,
           &furthest);
  EXPECT_NEAR(nearest, 100.0f / 1.5f, 1e-2f);
  EXPECT_NEAR(furthest, 100.0f / 0.5f, 1e-2f);

  // The periapsis is where it was asked to be: a quarter turn round puts
  // the near point on +y rather than on +x.
  const Conic turned{
      .focus = {0, 0}, .semiLatus = 100, .eccentricity = 0.5f, .periapsisDeg = 90};
  const glm::vec2 near = turned.at(0);
  EXPECT_NEAR(near.x, 0.0f, 1e-3f);
  EXPECT_NEAR(near.y, 100.0f / 1.5f, 1e-3f);
}

TEST(Conics, AtOneItIsAParabolaAndPastOneTheBranchHasAnAsymptote) {
  const Conic parabola{.semiLatus = 100, .eccentricity = 1.0f};
  EXPECT_FALSE(parabola.closes());
  // The one curve that has no far point at all: half a turn from the
  // periapsis the radius has run away, and the asymptote is the direction
  // it went — straight back the way it came.
  EXPECT_GT(parabola.radiusAt(179.9f), 100.0f * 100.0f);
  EXPECT_NEAR(parabola.asymptoteDeg(), 180.0f, 1e-3f);

  const Conic hyperbola{.semiLatus = 100, .eccentricity = 2.0f};
  // acos(-1/2): the branch turns 120 degrees from its near point and then
  // runs straight, so 240 degrees of the sweep is not on the curve.
  EXPECT_NEAR(hyperbola.asymptoteDeg(), 120.0f, 1e-3f);
  EXPECT_GT(hyperbola.radiusAt(119.99f), hyperbola.radiusAt(90.0f));

  // Past the asymptote a caller is asking about a place the branch never
  // reaches; the answer is a distant point rather than an infinity, so a
  // path built over the whole sweep is still a path.
  const SkPath swept = conicPath(hyperbola, {.fromDeg = -180, .toDeg = 180});
  EXPECT_TRUE(std::isfinite(swept.getBounds().width()));
  EXPECT_FALSE(swept.isLastContourClosed());
}

TEST(Conics, ASpanIsWhatComesBackAndReachBreaksTheContourRatherThanCloseIt) {
  const Conic hyperbola{.semiLatus = 100, .eccentricity = 2.0f};
  // Held to what is near enough to draw, the arms are cut off and what is
  // left is one run through the near point.
  const SkPath held =
      conicPath(hyperbola, {.fromDeg = -180, .toDeg = 180, .reach = 400});
  float nearest = 0, furthest = 0;
  extremes(held, {0, 0}, &nearest, &furthest);
  EXPECT_LE(furthest, 400.0f);
  EXPECT_NEAR(nearest, 100.0f / 3.0f, 1e-2f);

  // A closed conic swept the whole way round closes; a part of one does
  // not, so a reveal or a label along it has two ends to work between.
  const Conic ellipse{.semiLatus = 100, .eccentricity = 0.4f};
  EXPECT_TRUE(
      conicPath(ellipse, {.fromDeg = 0, .toDeg = 360}).isLastContourClosed());
  EXPECT_FALSE(
      conicPath(ellipse, {.fromDeg = -90, .toDeg = 90}).isLastContourClosed());

  // The path starts where the span does, so a trim from the start of the
  // contour is a trim from there.
  const SkPath part = conicPath(ellipse, {.fromDeg = -90, .toDeg = 90});
  const glm::vec2 begins = ellipse.at(-90);
  EXPECT_NEAR(part.getPoint(0).fX, begins.x, 1e-3f);
  EXPECT_NEAR(part.getPoint(0).fY, begins.y, 1e-3f);
}
