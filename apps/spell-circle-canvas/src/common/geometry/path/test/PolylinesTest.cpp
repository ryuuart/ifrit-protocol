/** @file
 * Polylines: flattening an outline to one, resampling it evenly along arc
 * length, aligning two of them, the smooth curve through a run of points,
 * subdividing with a lane carried along, and what a ring contains.
 */

#include <gtest/gtest.h>
#include <include/core/SkPath.h>
#include <include/core/SkRect.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <span>
#include <vector>

#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Skia.h"
#include "support/Paths.h"

using namespace sigil::geometry::path;
using sigil::geometry::test::rect;

namespace {

// ---------------------------------------------------------------------------
// Polyline

TEST(Polyline, FlattenRectKeepsCornersAndClosure) {
  const std::vector<Polyline> contours = flatten(rect(0, 0, 100, 50));
  ASSERT_EQ(contours.size(), 1u);
  EXPECT_TRUE(contours[0].closed);
  EXPECT_EQ(contours[0].points.size(), 4u);
  EXPECT_NEAR(contours[0].length(), 300.0f, 1e-3f);
}

TEST(Polyline, FlattenCircleHitsTolerance) {
  const std::vector<Polyline> contours =
      flatten(SkPath::Circle(0, 0, 100), 0.1f);
  ASSERT_EQ(contours.size(), 1u);
  // Flattening emits points sampled ON the curve, so every vertex keeps the
  // radius; the tolerance argument bounds how far the straight chords
  // BETWEEN them may sag inside the arc. That sag is also why the polyline
  // perimeter comes in slightly under the true circumference.
  for (const glm::vec2& p : contours[0].points) {
    const float r = glm::length(p);
    EXPECT_NEAR(r, 100.0f, 0.5f);
  }
  EXPECT_NEAR(contours[0].length(), 2.0f * (float)M_PI * 100.0f, 4.0f);
}

TEST(Polyline, ResampleUniformSpacing) {
  const std::vector<Sampled> sampled = resample(SkPath::Circle(0, 0, 50), 64);
  ASSERT_EQ(sampled.size(), 1u);
  ASSERT_EQ(sampled[0].points.size(), 64u);
  // resample() spaces its N points evenly along ARC LENGTH, not evenly in
  // the underlying segment parameter — the two differ on any curved path,
  // so a parameter-uniform sampler would show up here as an uneven spread.
  float minD = 1e9f, maxD = 0;
  for (size_t i = 0; i < 64; ++i) {
    const glm::vec2 a = sampled[0].points[i];
    const glm::vec2 b = sampled[0].points[(i + 1) % 64];
    const float d = glm::distance(a, b);
    minD = std::min(minD, d);
    maxD = std::max(maxD, d);
  }
  EXPECT_LT(maxD - minD, 0.6f);
}

// bestAlignment searches for the index offset (and direction) that pairs two
// sampled contours with the least total distance. Blending relies on it: two
// shapes sampled from different start points would otherwise twist through
// the interpolation. A cyclic shift is the case with an exact answer, so it
// is the one that can be checked to within float noise.
TEST(Polyline, AlignmentFindsRotation) {
  Sampled a;
  a.closed = true;
  Sampled b;
  b.closed = true;
  const int n = 16;
  for (int i = 0; i < n; ++i) {
    const float t = (float)i / (float)n * 2.0f * (float)M_PI;
    a.points.emplace_back(std::cos(t) * 10, std::sin(t) * 10);
  }
  // b is a rotated by 4 slots.
  for (int i = 0; i < n; ++i)
    b.points.push_back(a.points[(size_t)((i + 4) % n)]);
  const Alignment alignment = bestAlignment(a, b);
  const Sampled aligned = applyAlignment(b, alignment);
  for (int i = 0; i < n; ++i) {
    EXPECT_NEAR(aligned.points[(size_t)i].x, a.points[(size_t)i].x, 1e-4f);
    EXPECT_NEAR(aligned.points[(size_t)i].y, a.points[(size_t)i].y, 1e-4f);
  }
}

TEST(Polyline, SampleWalksTheParameterEvenlyAndCloses) {
  const Polyline circle =
      sample([](float t) { return glm::vec2{std::cos(t), std::sin(t)}; }, 0,
             kTau, 64, true);
  // A ring spells its closure rather than repeating the seam vertex, so
  // 64 steps round the whole turn are 64 points.
  EXPECT_EQ(circle.points.size(), 64u);
  EXPECT_TRUE(circle.closed);
  EXPECT_NEAR(circle.points.front().x, 1, 1e-5f);
  EXPECT_NEAR(circle.points[16].y, 1, 1e-5f);
}

TEST(Polyline, ToPathRoundTripsThroughFlatten) {
  Polyline tri;
  tri.points = {{0, 0}, {10, 0}, {0, 10}};
  tri.closed = true;
  const std::vector<Polyline> back = flatten(toPath(tri));
  ASSERT_EQ(back.size(), 1u);
  EXPECT_EQ(back[0].points.size(), 3u);
  EXPECT_TRUE(back[0].closed);
  EXPECT_NEAR(back[0].signedArea(), tri.signedArea(), 1e-4f);
}

TEST(Polyline, SmoothThroughTwoPointsIsALineAndFewerIsNothing) {
  EXPECT_TRUE(smoothThrough(std::vector<glm::vec2>{{3, 4}}).isEmpty());
  const SkPath line = smoothThrough(std::vector<glm::vec2>{{0, 0}, {10, 0}});
  EXPECT_EQ(line.countVerbs(), 2);
  EXPECT_EQ(line.getPoint(0), SkPoint::Make(0, 0));
  EXPECT_EQ(line.getPoint(1), SkPoint::Make(10, 0));
  EXPECT_FALSE(line.isLastContourClosed());
}

TEST(Polyline, SmoothThroughIsOneQuadPerInteriorPointEndingOnTheNextMidpoint) {
  const std::vector<glm::vec2> points = {{0, 0}, {40, 0}, {40, 40}, {80, 40}};
  const SkPath path = smoothThrough(points);
  SkPath::Iter iter(path, false);
  SkPoint p[4];
  ASSERT_EQ(iter.next(p), SkPath::kMove_Verb);
  EXPECT_EQ(p[0], SkPoint::Make(0, 0));
  // Each interior point is a CONTROL, never an on-curve point: the curve
  // is tangent to the edge after it at that edge's midpoint.
  ASSERT_EQ(iter.next(p), SkPath::kQuad_Verb);
  EXPECT_EQ(p[1], SkPoint::Make(40, 0));
  EXPECT_EQ(p[2], SkPoint::Make(40, 20));
  ASSERT_EQ(iter.next(p), SkPath::kQuad_Verb);
  EXPECT_EQ(p[1], SkPoint::Make(40, 40));
  EXPECT_EQ(p[2], SkPoint::Make(60, 40));
  ASSERT_EQ(iter.next(p), SkPath::kLine_Verb);
  EXPECT_EQ(p[1], SkPoint::Make(80, 40));
  EXPECT_EQ(iter.next(p), SkPath::kDone_Verb);
  // A quad lies in the hull of its three points, so the whole curve lies
  // in the hull of the polyline — which a Catmull-Rom through the same
  // points does not promise.
  const SkRect bounds = path.computeTightBounds();
  EXPECT_GE(bounds.left(), 0.0f);
  EXPECT_GE(bounds.top(), 0.0f);
  EXPECT_LE(bounds.right(), 80.0f);
  EXPECT_LE(bounds.bottom(), 40.0f);
}

TEST(Polyline, SmoothThroughClosedComesRoundFromTheSeamsMidpoint) {
  Polyline diamond;
  diamond.points = {{0, -10}, {10, 0}, {0, 10}, {-10, 0}};
  diamond.closed = true;
  const SkPath loop = smoothThrough(diamond);
  EXPECT_TRUE(loop.isLastContourClosed());
  // Every point steers a quad, so a closed loop of n points is n quads,
  // starting where the seam edge — last point to first — is crossed.
  EXPECT_EQ(loop.getPoint(0), SkPoint::Make(-5, -5));
  int quads = 0;
  SkPath::Iter iter(loop, false);
  SkPoint p[4];
  for (SkPath::Verb verb = iter.next(p); verb != SkPath::kDone_Verb;
       verb = iter.next(p))
    quads += verb == SkPath::kQuad_Verb;
  EXPECT_EQ(quads, 4);
  // …and it touches no point, since each is a control: the loop stays
  // strictly inside the diamond at every vertex.
  EXPECT_LT(loop.computeTightBounds().right(), 10.0f);
  EXPECT_GT(loop.computeTightBounds().top(), -10.0f);
}

TEST(Polyline, SubdivideCapsTheStepKeepsTheVerticesAndCarriesTheLane) {
  Polyline line;
  line.points = {{0, 0}, {10, 0}, {10, 3}};
  line.lane = {0.0f, 1.0f, 0.5f};
  const Polyline cut = subdivide(line, 2.0f);
  // Five steps across the first edge, two across the second, and every
  // source vertex still on the result.
  ASSERT_EQ(cut.points.size(), 8u);
  ASSERT_EQ(cut.lane.size(), cut.points.size());
  EXPECT_EQ(cut.points.front(), glm::vec2(0, 0));
  EXPECT_EQ(cut.points[5], glm::vec2(10, 0));
  EXPECT_EQ(cut.points.back(), glm::vec2(10, 3));
  for (size_t i = 1; i < cut.points.size(); ++i)
    EXPECT_LE(distance(cut.points[i - 1], cut.points[i]), 2.0f + 1e-4f);
  // The lane arrives at each vertex's own value and runs between them.
  EXPECT_FLOAT_EQ(cut.lane.front(), 0.0f);
  EXPECT_FLOAT_EQ(cut.lane[5], 1.0f);
  EXPECT_FLOAT_EQ(cut.lane.back(), 0.5f);
  EXPECT_FLOAT_EQ(cut.lane[1], 0.2f);

  // A repeated vertex is not a step.
  Polyline doubled;
  doubled.points = {{0, 0}, {0, 0}};
  EXPECT_EQ(subdivide(doubled, 1.0f).points.size(), 1u);
}

TEST(Polyline, SubdivideClosedComesHomeWithoutRepeatingTheSeam) {
  Polyline square;
  square.points = {{0, 0}, {8, 0}, {8, 8}, {0, 8}};
  square.closed = true;
  const Polyline cut = subdivide(square, 4.0f);
  EXPECT_TRUE(cut.closed);
  EXPECT_EQ(cut.points.size(), 8u);
  EXPECT_EQ(cut.points.front(), glm::vec2(0, 0));
  EXPECT_NE(cut.points.back(), cut.points.front());
}

TEST(Polyline, CatmullRomPassesThroughTheControlsAndZeroCurvatureIsTheChords) {
  Polyline controls;
  controls.points = {{0, 0}, {10, 10}, {20, 0}};
  controls.lane = {0.4f, 1.0f, 0.6f};

  const Polyline curve = catmullRom(controls, 2.0f, 1.0f);
  ASSERT_GT(curve.points.size(), controls.points.size());
  EXPECT_EQ(curve.points.front(), controls.points.front());
  EXPECT_EQ(curve.points.back(), controls.points.back());
  ASSERT_EQ(curve.lane.size(), curve.points.size());
  EXPECT_FLOAT_EQ(curve.lane.front(), 0.4f);
  EXPECT_FLOAT_EQ(curve.lane.back(), 0.6f);

  // No curvature is the chords themselves: every sample sits on the two
  // straight legs.
  const Polyline chords = catmullRom(controls, 5.0f, 0.0f);
  for (const glm::vec2 p : chords.points)
    EXPECT_NEAR(p.y, 10.0f - std::abs(p.x - 10.0f), 1e-4f);

  // One control is itself, and none is nothing.
  Polyline single;
  single.points = {{3, 4}};
  EXPECT_EQ(catmullRom(single, 1.0f).points.size(), 1u);
  EXPECT_TRUE(catmullRom(Polyline{}, 1.0f).points.empty());
}

TEST(Polyline, ContainsIsTheEvenOddRayTestAndBoundsSpanEveryPoint) {
  Polyline ring;
  ring.points = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
  EXPECT_TRUE(ring.contains({5, 5}));
  EXPECT_FALSE(ring.contains({15, 5}));
  EXPECT_FALSE(ring.contains({5, -1}));
  EXPECT_EQ(ring.bounds(), SkRect::MakeLTRB(0, 0, 10, 10));

  // Fewer than three points bound nothing.
  Polyline thin;
  thin.points = {{0, 0}, {10, 0}};
  EXPECT_FALSE(thin.contains({5, 0}));

  Polyline hole;
  hole.points = {{3, 3}, {7, 3}, {7, 7}, {3, 7}};
  const std::vector<Polyline> rings = {ring, hole};
  EXPECT_TRUE(containsEvenOdd(rings, {1, 1}));
  EXPECT_FALSE(containsEvenOdd(rings, {5, 5}));
  EXPECT_EQ(bounds(rings), SkRect::MakeLTRB(0, 0, 10, 10));
  EXPECT_TRUE(bounds(std::span<const Polyline>{}).isEmpty());
}

TEST(Polyline, EdgeCrossingsComeNearestTheStartFirst) {
  Polyline ring;
  ring.points = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
  ring.closed = true;
  const std::vector<glm::vec2> hits = edgeCrossings(ring, {-5, 5}, {15, 5});
  ASSERT_EQ(hits.size(), 2u);
  EXPECT_NEAR(hits[0].x, 0.0f, 1e-4f);
  EXPECT_NEAR(hits[1].x, 10.0f, 1e-4f);
  // A segment that stops short of the ring crosses nothing.
  EXPECT_TRUE(edgeCrossings(ring, {-5, 5}, {-1, 5}).empty());
  // An open polyline has no seam edge, so the side that edge would have
  // closed is not there to be crossed.
  Polyline open = ring;
  open.closed = false;
  const std::vector<glm::vec2> half = edgeCrossings(open, {-5, 5}, {15, 5});
  ASSERT_EQ(half.size(), 1u);
  EXPECT_NEAR(half[0].x, 10.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
// The parametric walk and the curve through a run of controls.

// `sample` steps evenly IN THE PARAMETER and emits count + 1 points; a
// closed curve arrives back where it started, and a ring spells its
// closure instead of repeating that vertex.
TEST(Polyline, SampleEmitsOneMoreThanItsCountAndClosesWithoutARepeat) {
  const auto unitCircle = [](float t) {
    return glm::vec2{std::cos(t), std::sin(t)};
  };
  // An open walk occupies both ends of its parameter range, so `count`
  // steps are `count + 1` points.
  const Polyline open = sample(unitCircle, 0, kPi, 64, false);
  EXPECT_EQ(open.points.size(), 65u);
  EXPECT_FALSE(open.closed);
  EXPECT_NEAR(open.points.back().x, -1.0f, 1e-5f);

  // A ring's seam is its closure and not a repeated vertex — the same
  // answer `subdivide` and `catmullRom` give.
  const Polyline ring = sample(unitCircle, 0, kTau, 64, true);
  EXPECT_EQ(ring.points.size(), 64u);
  EXPECT_GT(distance(ring.points.front(), ring.points.back()), 1e-3f);
}

// A closed run of controls is a RING: it carries a chord across its seam
// like any other, so the curve comes back closed and uncut.
TEST(Polyline, CatmullRomThroughAClosedRingComesBackClosedAndUncut) {
  Polyline controls;
  controls.closed = true;
  for (int i = 0; i < 8; ++i) {
    const float a = kTau * (float)i / 8.0f;
    controls.points.push_back({100.0f * std::cos(a), 100.0f * std::sin(a)});
  }
  const Polyline curve = catmullRom(controls, 5.0f, 1.0f);
  EXPECT_TRUE(curve.closed);
  // The seam edge is as short as every other edge: a ring cut at the seam
  // would leave one chord the length of a whole control span.
  float longest = 0;
  const size_t n = curve.points.size();
  for (size_t i = 0; i < n; ++i)
    longest =
        std::max(longest, distance(curve.points[i], curve.points[(i + 1) % n]));
  EXPECT_LT(longest, 6.0f);
  // Every control is still on the curve, the seam control included.
  for (const glm::vec2 control : controls.points) {
    float nearest = std::numeric_limits<float>::infinity();
    for (const glm::vec2 point : curve.points)
      nearest = std::min(nearest, distance(point, control));
    EXPECT_LT(nearest, 1.0f);
  }
}

TEST(Polyline, CatmullRomThroughAnOpenChainStaysOpen) {
  Polyline controls;
  controls.points = {{0, 0}, {50, 60}, {100, 0}, {150, 60}};
  const Polyline curve = catmullRom(controls, 5.0f, 1.0f);
  EXPECT_FALSE(curve.closed);
  EXPECT_EQ(curve.points.front(), controls.points.front());
  EXPECT_EQ(curve.points.back(), controls.points.back());
}

// The centroid is LENGTH-WEIGHTED over the edges, not the plain average
// of the vertices: a side cut into many nodes must not drag it.
TEST(Polyline, TheCentroidIsWeightedByEdgeLengthAndNotByVertexCount) {
  Polyline square;
  square.closed = true;
  square.points = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};
  const glm::vec2 plain = square.centroid();
  EXPECT_NEAR(plain.x, 50.0f, 1e-3f);
  EXPECT_NEAR(plain.y, 50.0f, 1e-3f);

  Polyline cut = square;
  cut.points = {{0, 0},   {25, 0},    {50, 0}, {75, 0},
                {100, 0}, {100, 100}, {0, 100}};
  const glm::vec2 weighted = cut.centroid();
  EXPECT_NEAR(weighted.x, plain.x, 1e-3f);
  EXPECT_NEAR(weighted.y, plain.y, 1e-3f);
}

TEST(Polyline, ReverseTurnsTheWindingAndKeepsTheShape) {
  Polyline ring;
  ring.closed = true;
  ring.points = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};
  const float area = ring.signedArea();
  ring.reverse();
  EXPECT_FLOAT_EQ(ring.signedArea(), -area);
  EXPECT_EQ(ring.points.front(), glm::vec2(0, 100));
}

}  // namespace
