/** @file
 * The node arithmetic: nodes put where a curve turns, nodes taken away
 * where they say nothing, a run of points fitted as few cubics, and the
 * exact in-between of two outlines that pair.
 */

#include <gtest/gtest.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRRect.h>
#include <include/core/SkRect.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <vector>

#include "sigilgeometry/path/Extremes.h"
#include "sigilgeometry/path/Fit.h"
#include "sigilgeometry/path/Interpolate.h"
#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Segments.h"
#include "sigilgeometry/path/Tidy.h"
#include "support/Paths.h"

using namespace sigil::geometry::path;
using sigil::geometry::test::rect;

namespace {

size_t nodeCount(const SkPath& path) {
  size_t total = 0;
  for (const SegmentContour& contour : segments(path))
    total += contour.segments.size();
  return total;
}

/** One cubic bowing to the right: its furthest reach in x is inside the
 *  curve rather than at either end, so it is exactly one axis extreme. */
SkPath bowedCubic() {
  SkPathBuilder b;
  b.moveTo(0, 0);
  b.cubicTo(80, 0, 80, 100, 0, 100);
  return b.detach();
}

/** An S: two bows the other way about, so the curve has an inflection
 *  in the middle and an axis extreme in each half. */
SkPath sCubic() {
  SkPathBuilder b;
  b.moveTo(0, 0);
  b.cubicTo(60, 0, -60, 100, 0, 100);
  return b.detach();
}

// ---------------------------------------------------------------------------
// Extremes.

TEST(PathExtremes, ACubicBowingOneWayGainsExactlyOneNode) {
  const SkPath source = bowedCubic();
  const SkPath split = extremes(source);
  EXPECT_EQ(nodeCount(source), 1u);
  EXPECT_EQ(nodeCount(split), 2u);
  const std::vector<glm::vec2> nodes = extremeNodes(source);
  ASSERT_EQ(nodes.size(), 1u);
  EXPECT_NEAR(nodes[0].x, source.computeTightBounds().right(), 1e-2f);
}

TEST(PathExtremes, TheSplitDrawsTheSameCurve) {
  for (const SkPath& source : {bowedCubic(), sCubic()}) {
    const SkRect before = source.computeTightBounds();
    const SkRect after = extremes(source).computeTightBounds();
    EXPECT_NEAR(before.left(), after.left(), 1e-2f);
    EXPECT_NEAR(before.right(), after.right(), 1e-2f);
    EXPECT_NEAR(before.top(), after.top(), 1e-2f);
    EXPECT_NEAR(before.bottom(), after.bottom(), 1e-2f);
  }
}

TEST(PathExtremes, AnSCubicGainsExactlyOneInflection) {
  EXPECT_EQ(extremeNodes(sCubic(), {.where = Where::Inflection}).size(), 1u);
}

// A shallow bulge is a rounding artefact rather than a feature of the
// drawing, and the depth dial is what tells the two apart.
TEST(PathExtremes, TheDepthDialSkipsAShallowTurnAndForcingItTakesIt) {
  SkPathBuilder b;
  b.moveTo(0, 0);
  b.cubicTo(0.7f, 0, 0.7f, 100, 0, 100);  // about half a pixel of bulge
  const SkPath shallow = b.detach();
  EXPECT_EQ(extremeNodes(shallow, {.minDepthPx = 1.0f}).size(), 0u);
  EXPECT_EQ(extremeNodes(shallow, {.minDepthPx = 0.0f}).size(), 1u);
}

// A quadratic's curvature peaks where it is tightest, which for a
// symmetric arch is its middle: the node lands at the apex and nowhere
// else, and the split leaves two pieces.
TEST(PathExtremes, MaxCurvatureNodesTheApexOfASymmetricQuad) {
  SkPathBuilder b;
  b.moveTo(0, 0);
  b.quadTo(50, 100, 100, 0);
  const SkPath arch = b.detach();
  const std::vector<glm::vec2> nodes =
      extremeNodes(arch, {.where = Where::MaxCurvature});
  ASSERT_EQ(nodes.size(), 1u);
  EXPECT_NEAR(nodes[0].x, 50.0f, 0.5f);
  EXPECT_NEAR(nodes[0].y, 50.0f, 0.5f);  // the quad's own midpoint
  EXPECT_EQ(nodeCount(extremes(arch, {.where = Where::MaxCurvature})), 2u);
}

// A cubic with one tight bend and one slack one peaks once, inside the
// bend rather than at either end.
TEST(PathExtremes, MaxCurvatureFindsTheTightBendOfACubic) {
  SkPathBuilder b;
  b.moveTo(0, 0);
  b.cubicTo(0, 100, 100, 100, 100, 0);
  const std::vector<glm::vec2> nodes =
      extremeNodes(b.detach(), {.where = Where::MaxCurvature});
  ASSERT_FALSE(nodes.empty());
  for (const glm::vec2 node : nodes) {
    EXPECT_GT(node.x, 0.0f);
    EXPECT_LT(node.x, 100.0f);
  }
}

TEST(PathExtremes, APathOfStraightLinesGainsNothing) {
  const SkPath square = rect(0, 0, 10, 10);
  EXPECT_TRUE(extremes(square) == square);
}

// ---------------------------------------------------------------------------
// Tidy.

TEST(PathTidy, AStraightRunCutIntoTenNodesComesBackAsOne) {
  SkPathBuilder b;
  b.moveTo(0, 0);
  for (int i = 1; i <= 10; ++i) b.lineTo((float)i * 10.0f, 0);
  b.lineTo(100, 50);
  const SkPath source = b.detach();
  EXPECT_EQ(nodeCount(source), 11u);
  EXPECT_EQ(nodeCount(tidy(source)), 2u);
}

TEST(PathTidy, ANodeOffAStraightRunGoesOnlyOnceTheToleranceCoversIt) {
  SkPathBuilder b;
  b.moveTo(0, 0);
  b.lineTo(50, 0.05f);
  b.lineTo(100, 0);
  const SkPath source = b.detach();
  EXPECT_EQ(nodeCount(tidy(source, 0.1f)), 1u);
  EXPECT_EQ(nodeCount(tidy(source, 0.01f)), 2u);
}

TEST(PathTidy, ACubicWhoseHandlesLieOnItsChordBecomesALine) {
  SkPathBuilder b;
  b.moveTo(0, 0);
  b.cubicTo(30, 0, 70, 0, 100, 0);
  const SkPath source = b.detach();
  const std::vector<SegmentContour> read = segments(tidy(source));
  ASSERT_EQ(read.size(), 1u);
  ASSERT_EQ(read[0].segments.size(), 1u);
  EXPECT_EQ(read[0].segments[0].kind, SegmentKind::Line);
}

TEST(PathTidy, AClosedContourStaysClosedAndKeepsItsShape) {
  const SkPath square = rect(0, 0, 100, 60);
  const SkPath tidied = tidy(square);
  ASSERT_EQ(segments(tidied).size(), 1u);
  EXPECT_TRUE(segments(tidied)[0].closed);
  EXPECT_EQ(tidied.computeTightBounds(), square.computeTightBounds());
}

// A zero-length piece says nothing wherever it sits, and where it sits
// must not decide whether it goes: guarding on how many pieces have been
// KEPT so far leaves a trailing one and takes a leading one.
TEST(PathTidy, ADuplicateNodeGoesFromTheFrontTheMiddleAndTheEnd) {
  const auto withDuplicateAt = [](int position) {
    SkPathBuilder b;
    b.moveTo(0, 0);
    if (position == 0) b.lineTo(0, 0);
    b.lineTo(100, 0);
    if (position == 1) b.lineTo(100, 0);
    b.lineTo(100, 60);
    if (position == 2) b.lineTo(100, 60);
    b.lineTo(0, 60);
    return b.detach();
  };
  for (int position = 0; position < 3; ++position) {
    const SkPath source = withDuplicateAt(position);
    EXPECT_EQ(nodeCount(source), 4u) << "position " << position;
    const SkPath tidied = tidy(source, 0.1f, {.collinear = false});
    EXPECT_EQ(nodeCount(tidied), 3u) << "position " << position;
    EXPECT_EQ(tidied.computeTightBounds(), source.computeTightBounds())
        << "position " << position;
  }
}

// A contour that is nothing but duplicates keeps the two pieces every
// contour keeps, rather than vanishing.
TEST(PathTidy, AContourOfNothingButDuplicatesDoesNotVanish) {
  SkPathBuilder b;
  b.moveTo(10, 10);
  b.lineTo(10, 10);
  b.lineTo(10, 10);
  b.lineTo(10, 10);
  const SkPath tidied = tidy(b.detach());
  ASSERT_EQ(segments(tidied).size(), 1u);
  EXPECT_EQ(nodeCount(tidied), 2u);
}

TEST(PathTidy, ACurvedOutlineIsLeftAlone) {
  const SkPath circle = SkPath::Circle(0, 0, 50);
  EXPECT_EQ(nodeCount(tidy(circle)), nodeCount(circle));
}

// ---------------------------------------------------------------------------
// Fit.

/** How far `point` stands from the segment `from`-`to`. */
float offSegment(glm::vec2 point, glm::vec2 from, glm::vec2 to) {
  const glm::vec2 run = to - from;
  const float length = glm::length(run);
  if (!(length > 1e-9f)) return glm::length(point - from);
  const float along =
      std::clamp(glm::dot(point - from, run) / (length * length), 0.0f, 1.0f);
  return glm::length(point - (from + run * along));
}

/** How far the worst of `points` stands from `path`, measured to the
 *  curve rather than to a sampled vertex of it: a flattened chord over a
 *  straight run is long, and a nearest-vertex measure would read a point
 *  sitting exactly on the curve as far from it. */
float worstError(const SkPath& path, const std::vector<glm::vec2>& points) {
  const std::vector<Polyline> flat = flatten(path, 0.01f);
  if (flat.empty()) return 1e9f;
  float worst = 0;
  for (const glm::vec2 point : points) {
    float nearest = 1e9f;
    for (const Polyline& line : flat)
      for (size_t i = 0; i + 1 < line.points.size(); ++i)
        nearest = std::min(
            nearest, offSegment(point, line.points[i], line.points[i + 1]));
    worst = std::max(worst, nearest);
  }
  return worst;
}

TEST(PathFit, HoldsEveryPointOfAnArcWithinTheTolerance) {
  std::vector<glm::vec2> arc;
  for (int i = 0; i <= 60; ++i) {
    const float a = (float)i / 60.0f * kPi * 0.5f;
    arc.push_back({100.0f * std::cos(a), 100.0f * std::sin(a)});
  }
  const SkPath fitted = fitCurve(arc, 0.5f);
  EXPECT_LE(worstError(fitted, arc), 0.5f);
  // A quarter circle is one cubic; a fit that answered a node per point
  // would be a resampling rather than a fit.
  EXPECT_LE(nodeCount(fitted), 2u);
}

TEST(PathFit, ATighterToleranceNeverNeedsFewerNodes) {
  std::vector<glm::vec2> wave;
  for (int i = 0; i <= 200; ++i) {
    const float x = (float)i;
    wave.push_back({x, 30.0f * std::sin(x * 0.08f)});
  }
  const size_t loose = nodeCount(fitCurve(wave, 8.0f));
  const size_t tight = nodeCount(fitCurve(wave, 0.2f));
  EXPECT_GE(tight, loose);
  EXPECT_LE(worstError(fitCurve(wave, 0.2f), wave), 0.2f);
}

TEST(PathFit, TwoPointsAreTheLineBetweenThemAndFewerAreNothing) {
  const std::vector<glm::vec2> two{{0, 0}, {10, 10}};
  const std::vector<SegmentContour> read = segments(fitCurve(two, 1.0f));
  ASSERT_EQ(read.size(), 1u);
  ASSERT_EQ(read[0].segments.size(), 1u);
  EXPECT_EQ(read[0].segments[0].kind, SegmentKind::Line);
  EXPECT_TRUE(fitCurve(std::vector<glm::vec2>{{0, 0}}, 1.0f).isEmpty());
}

// ---------------------------------------------------------------------------
// Interpolate.

TEST(PathInterpolate, TwoRoundedRectsMeetInTheMiddleVerbForVerb) {
  const SkPath a =
      SkPath::RRect(SkRRect::MakeRectXY(SkRect::MakeWH(80, 40), 4, 4));
  const SkPath b =
      SkPath::RRect(SkRRect::MakeRectXY(SkRect::MakeWH(80, 40), 16, 16));
  const std::optional<SkPath> half = interpolate(a, b, 0.5f);
  ASSERT_TRUE(half.has_value());
  EXPECT_EQ(compatible(a, *half), Compatible::Yes);
  const std::optional<SkPath> at0 = interpolate(a, b, 0.0f);
  ASSERT_TRUE(at0.has_value());
  EXPECT_TRUE(*at0 == a);
  const std::optional<SkPath> at1 = interpolate(a, b, 1.0f);
  ASSERT_TRUE(at1.has_value());
  EXPECT_TRUE(*at1 == b);
}

TEST(PathInterpolate, APairThatDoesNotPairAnswersNothing) {
  SkPathBuilder triangle;
  triangle.moveTo(0, 0);
  triangle.lineTo(10, 0);
  triangle.lineTo(5, 10);
  triangle.close();
  EXPECT_FALSE(
      interpolate(rect(0, 0, 10, 10), triangle.detach(), 0.5f).has_value());
}

}  // namespace
