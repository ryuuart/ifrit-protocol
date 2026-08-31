/** @file
 * The path leaf alone: polylines, contours, the pose along one, the path
 * operators, noise and the numeric routines.
 */

// SigilGeometryPath — polylines, contours, noise and the numeric routines
// under them. Links the path leaf alone: a test here that needs a mesh,
// an importer or a material is in the wrong binary.
#include <gtest/gtest.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRect.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <optional>
#include <utility>
#include <vector>

#include "sigilgeometry/path/Contour.h"
#include "sigilgeometry/path/Noise.h"
#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Ops.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Pose.h"
#include "sigilgeometry/path/Skia.h"

using namespace sigil::geometry::path;

namespace {

SkPath square(float size) { return SkPath::Rect(SkRect::MakeWH(size, size)); }

SkPath rect(float x, float y, float w, float h) {
  return SkPath::Rect(SkRect::MakeXYWH(x, y, w, h));
}

// ---------------------------------------------------------------------------
// Pose

// An independent walk of the same contours, written out longhand: a
// fraction of the total arc length, wrapped or clamped, then measured
// contour by contour with Skia's own measure. It is what poseAlong must
// reproduce to the bit, because a pose that lands anywhere else moves
// every node travelling a curve.
SkPoint referenceWalk(const SkPath& path, float u) {
  std::vector<sk_sp<SkContourMeasure>> contours;
  std::vector<float> starts;
  float total = 0;
  bool closed = true;
  SkContourMeasureIter iter(path, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    if (!(len > 0)) continue;
    closed = closed && contour->isClosed();
    starts.push_back(total);
    total += len;
    contours.push_back(std::move(contour));
  }
  if (contours.empty()) closed = false;
  if (!(total > 0)) return {0, 0};

  float w = closed ? std::fmod(u, 1.0f) : std::clamp(u, 0.0f, 1.0f);
  if (closed && w < 0.0f) w += 1.0f;
  const float want = w * total;
  size_t i = contours.size() - 1;
  for (size_t c = 0; c + 1 < contours.size(); ++c)
    if (want < starts[c + 1]) {
      i = c;
      break;
    }
  SkPoint pos{0, 0};
  SkVector tan{0, 0};
  const float len = contours[i]->length();
  (void)contours[i]->getPosTan(std::clamp(want - starts[i], 0.0f, len), &pos,
                               &tan);
  return pos;
}

SkPath twoContours() {
  SkPathBuilder b;
  b.addOval(SkRect::MakeXYWH(0, 0, 200, 120));
  b.addRect(SkRect::MakeXYWH(300, 40, 90, 60));
  return b.detach();
}

TEST(Pose, MatchesTheContourItIsTakenFrom) {
  const std::vector<Contour> contours = Contour::of(square(100));
  ASSERT_EQ(contours.size(), 1u);
  const Pose pose = poseAlong(contours[0], 150.0f);
  const std::optional<Contour::Sample> sample = contours[0].at(150.0f);
  ASSERT_TRUE(sample.has_value());
  EXPECT_EQ(pose.position, sample->position);
  EXPECT_EQ(pose.tangent, sample->tangent);
  EXPECT_EQ(pose.distance, 150.0f);
}

TEST(Pose, NormalIsTheTangentTurnedTowardPositiveY) {
  const std::vector<Contour> contours = Contour::of(square(100));
  const Pose pose = poseAlong(contours[0], 10.0f);
  EXPECT_NEAR(glm::dot(pose.tangent, pose.normal), 0.0f, 1e-6f);
  EXPECT_NEAR(pose.normal.x, -pose.tangent.y, 1e-6f);
  EXPECT_NEAR(pose.normal.y, pose.tangent.x, 1e-6f);
}

TEST(Pose, ClampParksAndAroundComesRoundOnAClosedContour) {
  const std::vector<Contour> contours = Contour::of(square(100));
  const float len = contours[0].length();
  ASSERT_TRUE(contours[0].closed());
  EXPECT_EQ(poseAlong(contours[0], len + 30, Wrap::Clamp).distance, len);
  EXPECT_NEAR(poseAlong(contours[0], len + 30, Wrap::Around).distance, 30.0f,
              1e-3f);
  EXPECT_NEAR(poseAlong(contours[0], -10, Wrap::Around).distance, len - 10,
              1e-3f);

  // An open contour has no seam to come round through: it parks either
  // way.
  SkPathBuilder open;
  open.moveTo(0, 0).lineTo(60, 0);
  const std::vector<Contour> line = Contour::of(open.detach());
  ASSERT_EQ(line.size(), 1u);
  EXPECT_EQ(poseAlong(line[0], 90, Wrap::Around).distance, 60.0f);
}

TEST(Pose, EveryContourIsOneCoordinate) {
  const SkPath path = twoContours();
  const std::vector<Contour> contours = Contour::of(path);
  ASSERT_EQ(contours.size(), 2u);
  const float total = totalLength(contours);
  EXPECT_FLOAT_EQ(total, contours[0].length() + contours[1].length());
  EXPECT_TRUE(closedThroughout(contours));
  EXPECT_FALSE(closedThroughout({}));

  // Just past the first contour's end is the second contour's start.
  const Pose second = poseAlong(contours, contours[0].length() + 5.0f);
  const Pose direct = poseAlong(contours[1], 5.0f);
  EXPECT_EQ(second.position, direct.position);
}

TEST(Pose, ReproducesTheMotionPathWalkExactly) {
  for (const SkPath& path : {square(100), twoContours()}) {
    const std::vector<Contour> contours = Contour::of(path);
    const float total = totalLength(contours);
    const bool closed = closedThroughout(contours);
    for (int step = -40; step <= 240; ++step) {
      const float u = (float)step / 100.0f;
      float w = closed ? std::fmod(u, 1.0f) : std::clamp(u, 0.0f, 1.0f);
      if (closed && w < 0.0f) w += 1.0f;
      const SkPoint want = referenceWalk(path, u);
      const Pose got = poseAlong(contours, w * total);
      EXPECT_EQ(got.position.x, want.fX) << "u = " << u;
      EXPECT_EQ(got.position.y, want.fY) << "u = " << u;
    }
  }
}

TEST(Pose, AnUnmeasurablePathHasNoPose) {
  const Pose none = poseAlong(Contour{}, 5.0f);
  EXPECT_EQ(none.position, (glm::vec2{0, 0}));
  EXPECT_EQ(poseAlong(std::vector<Contour>{}, 5.0f).position,
            (glm::vec2{0, 0}));
}

// ---------------------------------------------------------------------------
// Contour

TEST(Contour, OfSkipsDegenerateContoursAndReportsLengthAndClosure) {
  SkPathBuilder b;
  b.moveTo(0, 0);  // a lone moveTo has no length
  b.moveTo(0, 0).lineTo(10, 0);
  b.addRect(SkRect::MakeWH(4, 4));
  const std::vector<Contour> contours = Contour::of(b.detach());
  ASSERT_EQ(contours.size(), 2u);
  EXPECT_FLOAT_EQ(contours[0].length(), 10.0f);
  EXPECT_FALSE(contours[0].closed());
  EXPECT_FLOAT_EQ(contours[1].length(), 16.0f);
  EXPECT_TRUE(contours[1].closed());
}

TEST(Contour, AtClampsAndAroundWraps) {
  const Contour c = Contour::of(square(10))[0];
  const auto end = c.at(100);
  ASSERT_TRUE(end);
  const auto start = c.at(0);
  ASSERT_TRUE(start);
  EXPECT_NEAR(glm::distance(end->position, start->position), 0, 1e-3f);
  // 45 units around a 40-unit square is 5 units in: on the top edge.
  const Contour::Sample s = c.around(45);
  EXPECT_NEAR(s.position.x, 5, 1e-3f);
  EXPECT_NEAR(s.position.y, 0, 1e-3f);
  EXPECT_NEAR(s.tangent.x, 1, 1e-4f);
}

TEST(Contour, CornersOfASquareAreItsFourVertices) {
  const Contour c = Contour::of(square(10))[0];
  float sharpest = 0;
  const std::vector<Contour::Corner> corners = c.corners(30, 3, 2, &sharpest);
  ASSERT_EQ(corners.size(), 4u);
  EXPECT_NEAR(sharpest, 90, 0.5f);
  EXPECT_NEAR(corners[0].distance, 0, 1e-3f);  // the seam counts
  EXPECT_NEAR(corners[1].distance, 10, 0.5f);
  EXPECT_NEAR(corners[2].distance, 20, 0.5f);
  EXPECT_NEAR(corners[3].distance, 30, 0.5f);
  // Arriving along +x, leaving along +y at the first vertex after the seam.
  EXPECT_NEAR(corners[1].in.x, 1, 1e-3f);
  EXPECT_NEAR(corners[1].out.y, 1, 1e-3f);
}

TEST(Contour, NoCornersOnACircleButTheSharpestTurnIsReported) {
  const Contour c = Contour::of(SkPath::Circle(0, 0, 50))[0];
  float sharpest = -1;
  EXPECT_TRUE(c.corners(30, 3, 2, &sharpest).empty());
  EXPECT_GE(sharpest, 0);
  EXPECT_LT(sharpest, 30);
}

TEST(Contour, SegmentIsThePieceBetweenTwoDistances) {
  const Contour c = Contour::of(square(10))[0];
  const SkPath piece = c.segment(2, 8);
  const SkRect bounds = piece.getBounds();
  EXPECT_NEAR(bounds.left(), 2, 1e-3f);
  EXPECT_NEAR(bounds.right(), 8, 1e-3f);
  EXPECT_NEAR(bounds.height(), 0, 1e-3f);
}

TEST(Contour, ParallelOfALineSitsAcrossToTheLeft) {
  SkPathBuilder b;
  b.moveTo(0, 0).lineTo(100, 0);
  const SkPath left = parallel(b.detach(), 5);
  const SkRect bounds = left.getBounds();
  // Facing +x in y-down space, left is -y.
  EXPECT_NEAR(bounds.top(), -5, 1e-3f);
  EXPECT_NEAR(bounds.bottom(), -5, 1e-3f);
  EXPECT_NEAR(bounds.left(), 0, 1e-3f);
  EXPECT_NEAR(bounds.right(), 100, 1e-3f);
}

TEST(Contour, ParallelOfASquareGrowsOutwardOnTheLeftOfAClockwiseWalk) {
  // Skia's rect walks clockwise in y-down space; its left side is outside.
  const SkRect bounds = parallel(square(10), 3, 1).getBounds();
  EXPECT_NEAR(bounds.left(), -3, 0.1f);
  EXPECT_NEAR(bounds.right(), 13, 0.1f);
  EXPECT_NEAR(bounds.top(), -3, 0.1f);
  EXPECT_NEAR(bounds.bottom(), 13, 0.1f);
}

TEST(Contour, DisplaceKeepsOpenEndpointsOnTheSourceCurve) {
  SkPathBuilder b;
  b.moveTo(0, 0).lineTo(96, 0);
  const SkPath wave = displace(b.detach(), 4, 24, false);
  SkPoint first = wave.getPoint(0);
  SkPoint last = wave.getPoint(wave.countPoints() - 1);
  EXPECT_NEAR(first.fY, 0, 1e-3f);
  EXPECT_NEAR(last.fY, 0, 1e-3f);
  EXPECT_NEAR(last.fX, 96, 1e-3f);
  const SkRect bounds = wave.getBounds();
  EXPECT_NEAR(bounds.height(), 8, 0.2f);
}

TEST(Contour, CornerWindowsPartitionTheOutline) {
  const SkPath outline = square(10);
  const SkPath near = cornerWindows(outline, 2, true, 30);
  const SkPath far = cornerWindows(outline, 2, false, 30);
  float nearLen = 0, farLen = 0;
  for (const Contour& c : Contour::of(near)) nearLen += c.length();
  for (const Contour& c : Contour::of(far)) farLen += c.length();
  EXPECT_NEAR(nearLen, 4 * 4, 0.1f);  // four corners, radius 2 each side
  EXPECT_NEAR(farLen, 40 - 16, 0.1f);
}

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
  EXPECT_EQ(circle.points.size(), 65u);
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

// ---------------------------------------------------------------------------
// Ops

TEST(Ops, PathfinderBooleans) {
  const SkPath a = rect(0, 0, 100, 100);
  const SkPath b = rect(50, 0, 100, 100);
  EXPECT_NEAR(ops::unite(a, b).computeTightBounds().width(), 150, 1e-3);
  EXPECT_NEAR(ops::subtract(a, b).computeTightBounds().width(), 50, 1e-3);
  EXPECT_NEAR(ops::intersect(a, b).computeTightBounds().width(), 50, 1e-3);
  // Exclude keeps the union's outline but removes the overlap, so bounds
  // alone cannot tell it from unite — the interior probe is what separates
  // them.
  const SkPath xr = ops::exclude(a, b);
  EXPECT_NEAR(xr.computeTightBounds().width(), 150, 1e-3);
  EXPECT_FALSE(xr.contains(75, 50));  // the overlap is punched out
  EXPECT_TRUE(ops::unite({a, b, rect(140, 0, 100, 100)}).contains(200, 50));
}

// Offset distance is a radius, not a diameter: a positive amount grows the
// outline by that much on every side, a negative one eats into it, so a
// circle of radius 50 offset by 10 spans 120 across and by -15 spans 70.
TEST(Ops, OffsetGrowsAndShrinks) {
  const SkPath circle = SkPath::Circle(0, 0, 50);
  const SkRect grown = ops::offset(circle, 10).computeTightBounds();
  EXPECT_NEAR(grown.width(), 120, 1.5f);
  const SkRect shrunk = ops::offset(circle, -15).computeTightBounds();
  EXPECT_NEAR(shrunk.width(), 70, 1.5f);
}

// Distorts are shape-preserving in the large: they displace the outline but
// must not translate the shape or run away in size. Each bound below is the
// distort's own amplitude budget — Roughen's amplitude of 6 can move a point
// at most 6 outward, so the width cannot exceed the diameter plus twice that.
TEST(Ops, DistortsKeepBoundsSane) {
  const SkPath base = SkPath::Circle(100, 100, 60);
  const SkRect roughened =
      ops::Roughen{6, 8, 42}.apply(base).computeTightBounds();
  EXPECT_LT(std::abs(roughened.centerX() - 100), 4);
  EXPECT_LT(roughened.width(), 120 + 2 * 6 + 2);
  const SkPath twirled = ops::Twirl{90}.apply(base);
  EXPECT_LT(std::abs(twirled.computeTightBounds().centerX() - 100), 4);
  const SkRect bloated =
      ops::PuckerBloat{0.8f}.apply(base).computeTightBounds();
  EXPECT_GT(bloated.width(), 118);  // bloat pushes outward, never inward
  const SkPath chained =
      ops::chain({ops::offsetBy(6), ops::Zigzag{4, 20}})(base);
  EXPECT_FALSE(chained.isEmpty());
}

// ---------------------------------------------------------------------------
// Numeric

TEST(Numeric, BisectReturnsTheFarSideOfTheTransition) {
  const float at = bisect(0.0f, 1.0f, [](float x) { return x < 0.3f; }, 20);
  EXPECT_GE(at, 0.3f);
  EXPECT_NEAR(at, 0.3f, 1e-5f);
}

TEST(Numeric, WrapIsPeriodicAndNonNegative) {
  EXPECT_FLOAT_EQ(wrap(5, 4), 1);
  EXPECT_FLOAT_EQ(wrap(-1, 4), 3);
  EXPECT_FLOAT_EQ(wrap(4, 4), 0);
}

// ---------------------------------------------------------------------------
// Noise

TEST(Noise, HashIsDeterministicBoundedAndSeedSensitive) {
  for (uint32_t i = 0; i < 1000; ++i) {
    const float v = noise::hash(7, i);
    EXPECT_GE(v, -1.0f);
    EXPECT_LE(v, 1.0f);
    EXPECT_EQ(v, noise::hash(7, i));
  }
  EXPECT_NE(noise::hash(7, 3), noise::hash(8, 3));
}

TEST(Noise, PcgStreamAndStatelessHashAgree) {
  uint32_t state = 42;
  const uint32_t first = noise::pcgNext(state);
  EXPECT_EQ(first, noise::pcgHash(42));
  EXPECT_EQ(state, noise::pcgAdvance(42));
  EXPECT_LT(noise::pcgUnit(42u), 1.0f);
  EXPECT_GE(noise::pcgUnit(42u), 0.0f);
}

TEST(Noise, Value3IsSmoothAndBounded) {
  float prev = noise::value3({0.5f, 0.5f, 0.5f}, 1);
  for (int i = 1; i <= 100; ++i) {
    const float v = noise::value3({0.5f + i * 0.01f, 0.5f, 0.5f}, 1);
    EXPECT_GE(v, -1.0f);
    EXPECT_LE(v, 1.0f);
    EXPECT_LT(std::abs(v - prev), 0.1f);  // 0.01 steps never jump
    prev = v;
  }
}

}  // namespace
