/** @file
 * The path leaf alone: polylines, contours, the pose along one, the path
 * operators, the shaper and profile seams with the band they cut, the
 * crossings among a set of paths, noise and the numeric routines.
 */

// SigilGeometryPath — polylines, contours, noise and the numeric routines
// under them. Links the path leaf alone: a test here that needs a mesh,
// an importer or a material is in the wrong binary.
#include <gtest/gtest.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRect.h>
#include <sigilcore/compute/Noise.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <optional>
#include <utility>
#include <vector>

#include "sigilgeometry/path/Band.h"
#include "sigilgeometry/path/Contour.h"
#include "sigilgeometry/path/Crossings.h"
#include "sigilgeometry/path/Edges.h"
#include "sigilgeometry/path/Frame.h"
#include "sigilgeometry/path/Noise.h"
#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Ops.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Pose.h"
#include "sigilgeometry/path/Profile.h"
#include "sigilgeometry/path/Shaper.h"
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

// ---------------------------------------------------------------------------
// Edges

TEST(Edges, InsetPolygonShrinksASquareByTheDistanceWhicheverWayItWinds) {
  const std::vector<glm::vec2> square = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};
  const std::vector<glm::vec2> in = insetPolygon(square, 10);
  ASSERT_EQ(in.size(), 4u);
  const glm::vec2 expected[] = {{10, 10}, {90, 10}, {90, 90}, {10, 90}};
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_NEAR(in[i].x, expected[i].x, 1e-4f) << i;
    EXPECT_NEAR(in[i].y, expected[i].y, 1e-4f) << i;
  }
  // The same polygon wound the other way insets to the same corners,
  // each still answering to the vertex it came from.
  const std::vector<glm::vec2> reversed(square.rbegin(), square.rend());
  const std::vector<glm::vec2> other = insetPolygon(reversed, 10);
  ASSERT_EQ(other.size(), 4u);
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_NEAR(other[i].x, in[3 - i].x, 1e-4f) << i;
    EXPECT_NEAR(other[i].y, in[3 - i].y, 1e-4f) << i;
  }
  // A negative distance grows.
  const std::vector<glm::vec2> out = insetPolygon(square, -10);
  EXPECT_NEAR(out[0].x, -10, 1e-4f);
  EXPECT_NEAR(out[2].y, 110, 1e-4f);
}

TEST(Edges, InsetPolygonMitresASharpCornerUntilTheLimitBluntsIt) {
  // A thin rhomb: 36° at the two ends, 144° at the two sides. The mitre
  // at a 36° corner is d / sin(18°) — over three distances — while the
  // 144° corner's is barely more than one.
  constexpr float kHalfTip = 18.0f * kPi / 180.0f;
  const float length = 100.0f;
  const float half = length * std::tan(kHalfTip);
  const std::vector<glm::vec2> rhomb = {
      {-length, 0}, {0, -half}, {length, 0}, {0, half}};
  const float d = 4.0f;
  const std::vector<glm::vec2> mitred = insetPolygon(rhomb, d, 100.0f);
  ASSERT_EQ(mitred.size(), 4u);
  EXPECT_NEAR(mitred[0].x, -length + d / std::sin(kHalfTip), 1e-3f);
  EXPECT_NEAR(mitred[0].y, 0.0f, 1e-4f);
  EXPECT_NEAR(mitred[2].x, length - d / std::sin(kHalfTip), 1e-3f);
  // Every moved edge stands exactly d inside the edge it came from: the
  // distance from a moved vertex to the source edge's line is d.
  for (size_t i = 0; i < 4; ++i) {
    const glm::vec2 a = rhomb[i], b = rhomb[(i + 1) % 4];
    const glm::vec2 edge = b - a;
    const glm::vec2 normal = glm::normalize(glm::vec2{-edge.y, edge.x});
    EXPECT_NEAR(std::abs(glm::dot(mitred[i] - a, normal)), d, 1e-3f) << i;
    EXPECT_NEAR(std::abs(glm::dot(mitred[(i + 1) % 4] - a, normal)), d, 1e-3f)
        << i;
  }
  // Capped at two distances the tips stop at 2d along the axis, the
  // vertex count stands, and the wide corners — inside the cap — are
  // exactly where they were.
  const std::vector<glm::vec2> capped = insetPolygon(rhomb, d, 2.0f);
  ASSERT_EQ(capped.size(), 4u);
  EXPECT_NEAR(capped[0].x, -length + 2 * d, 1e-3f);
  EXPECT_NEAR(capped[0].y, 0.0f, 1e-4f);
  EXPECT_NEAR(capped[1].x, mitred[1].x, 1e-4f);
  EXPECT_NEAR(capped[1].y, mitred[1].y, 1e-4f);
}

TEST(Edges, InsetPolygonMovesAReflexCornerTheWayItsEdgesSayAndLeavesTooFewAlone) {
  // An L. Its reflex corner is where the bar meets the column, and the
  // inset L's reflex corner is the meeting of the two moved edges — back
  // toward the outer corner, not toward the interior of either arm.
  const std::vector<glm::vec2> ell = {{0, 0},   {100, 0},  {100, 50},
                                      {50, 50}, {50, 100}, {0, 100}};
  const std::vector<glm::vec2> in = insetPolygon(ell, 10);
  ASSERT_EQ(in.size(), 6u);
  EXPECT_NEAR(in[3].x, 40, 1e-4f);
  EXPECT_NEAR(in[3].y, 40, 1e-4f);
  EXPECT_NEAR(in[0].x, 10, 1e-4f);
  EXPECT_NEAR(in[2].x, 90, 1e-4f);
  EXPECT_NEAR(in[2].y, 40, 1e-4f);
  EXPECT_NEAR(in[4].y, 90, 1e-4f);

  const std::vector<glm::vec2> two = {{0, 0}, {5, 5}};
  const std::vector<glm::vec2> same = insetPolygon(two, 3);
  ASSERT_EQ(same.size(), 2u);
  EXPECT_EQ(same[1], glm::vec2(5, 5));
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
    const float v = sigil::core::noise::hash(7, i);
    EXPECT_GE(v, -1.0f);
    EXPECT_LE(v, 1.0f);
    EXPECT_EQ(v, sigil::core::noise::hash(7, i));
  }
  EXPECT_NE(sigil::core::noise::hash(7, 3), sigil::core::noise::hash(8, 3));
}

TEST(Noise, PcgStreamAndStatelessHashAgree) {
  uint32_t state = 42;
  const uint32_t first = sigil::core::noise::pcgNext(state);
  EXPECT_EQ(first, sigil::core::noise::pcgHash(42));
  EXPECT_EQ(state, sigil::core::noise::pcgAdvance(42));
  EXPECT_LT(sigil::core::noise::pcgUnit(42u), 1.0f);
  EXPECT_GE(sigil::core::noise::pcgUnit(42u), 0.0f);
}

TEST(Noise, Value3IsSmoothAndBounded) {
  float prev = valueNoise({0.5f, 0.5f, 0.5f}, 1);
  for (int i = 1; i <= 100; ++i) {
    const float v = valueNoise({0.5f + i * 0.01f, 0.5f, 0.5f}, 1);
    EXPECT_GE(v, -1.0f);
    EXPECT_LE(v, 1.0f);
    EXPECT_LT(std::abs(v - prev), 0.1f);  // 0.01 steps never jump
    prev = v;
  }
}

// ---------------------------------------------------------------------------
// The shaper seam.

namespace {
struct NudgeX {
  float dx = 0;
  float bleed() const { return std::abs(dx); }
  SkPath shape(const SkPath& p) const {
    return p.makeTransform(SkMatrix::Translate(dx, 0));
  }
  bool operator==(const NudgeX&) const = default;
};
struct Identity {
  SkPath shape(const SkPath& p) const { return p; }
  bool operator==(const Identity&) const = default;
};
}  // namespace

TEST(Shaper, ComparesByTheHeldSchemeAndItsParameters) {
  EXPECT_TRUE(Shaper(NudgeX{3}) == Shaper(NudgeX{3}));
  EXPECT_FALSE(Shaper(NudgeX{3}) == Shaper(NudgeX{4}));
  // Two different schemes are never equal, whatever they do.
  EXPECT_FALSE(Shaper(NudgeX{0}) == Shaper(Identity{}));
  // The empty shaper is reflexive, or every holder patches forever.
  EXPECT_TRUE(Shaper() == Shaper());
  EXPECT_FALSE(Shaper() == Shaper(Identity{}));
}

TEST(Shaper, BleedIsReadOffTheSchemeAndIsZeroWhenNotDeclared) {
  EXPECT_FLOAT_EQ(Shaper(NudgeX{-5}).bleed(), 5.0f);
  EXPECT_FLOAT_EQ(Shaper(Identity{}).bleed(), 0.0f);
  // An empty shaper passes its path through untouched.
  SkPathBuilder b;
  b.addRect(SkRect::MakeWH(10, 10));
  const SkPath src = b.detach();
  EXPECT_EQ(Shaper().shape(src), src);
  EXPECT_EQ(Shaper(NudgeX{2}).shape(src).getBounds().left(), 2.0f);
}

// ---------------------------------------------------------------------------
// The profile seam.

namespace {
struct Taper {
  float peak = 10;
  float across(float along) const { return peak * (1.0f - along); }
  float max() const { return peak; }
  bool operator==(const Taper&) const = default;
};
struct PxTaper {
  static constexpr bool alongIsPx = true;
  float across(float alongPx) const { return alongPx * 0.1f; }
  float max() const { return 100.0f; }
  bool operator==(const PxTaper&) const = default;
};
}  // namespace

TEST(Profile, TheTwoPresetsAreTheLawsEveryOtherIsDefinedAgainst) {
  EXPECT_FLOAT_EQ(profile::self().across(0.5f), 0.0f);
  EXPECT_FLOAT_EQ(profile::self().max(), 0.0f);
  EXPECT_FLOAT_EQ(profile::offset(-7.0f).across(0.5f), -7.0f);
  // max() is a REACH, so it is the magnitude — cull is sized from it.
  EXPECT_FLOAT_EQ(profile::offset(-7.0f).max(), 7.0f);
  EXPECT_TRUE(profile::offset(4) == profile::offset(4));
  EXPECT_FALSE(profile::offset(4) == profile::offset(5));
  EXPECT_FALSE(profile::offset(0) == profile::self());
  EXPECT_TRUE(Profile() == Profile());
  EXPECT_FALSE(Profile() == profile::self());
}

TEST(Profile, APxKeyedLawIsConvertedOnceByTheSeam) {
  const Profile fraction = Taper{10};
  const Profile px = PxTaper{};
  EXPECT_FALSE(fraction.keyedInPx());
  EXPECT_TRUE(px.keyedInPx());
  // acrossAt is the one call a measured consumer makes: a fraction-keyed
  // law ignores the length, a px-keyed one is handed along * length.
  EXPECT_FLOAT_EQ(fraction.acrossAt(0.25f, 200.0f), 7.5f);
  EXPECT_FLOAT_EQ(px.acrossAt(0.25f, 200.0f), 5.0f);
}

// ---------------------------------------------------------------------------
// The band a width law cuts.

TEST(Band, AConstantProfileRidesParallelsCornerRepair) {
  SkPathBuilder b;
  b.addRect(SkRect::MakeXYWH(0, 0, 100, 60));
  const SkPath spine = b.detach();
  // Positive across is LEFT of travel, which on Skia's clockwise rect is
  // outside it: the rail's bounds grow by the offset on every side.
  const SkPath out = profileOffset(spine, profile::offset(6.0f));
  EXPECT_FALSE(out.isEmpty());
  EXPECT_LE(out.getBounds().left(), -5.0f);
  EXPECT_GE(out.getBounds().right(), 105.0f);
  // A zero profile is the boundary itself, handed back untouched.
  EXPECT_EQ(profileOffset(spine, profile::self()), spine);
}

TEST(Band, TheRegionIsBoundedByTheWidthAndEmptyWithoutOne) {
  SkPathBuilder b;
  b.moveTo(0, 50);
  b.lineTo(200, 50);
  const SkPath spine = b.detach();
  const SkPath centred = bandRegion(spine, profile::offset(20.0f));
  ASSERT_FALSE(centred.isEmpty());
  // Centred: half the width each side of the spine.
  EXPECT_NEAR(centred.getBounds().top(), 40.0f, 1.0f);
  EXPECT_NEAR(centred.getBounds().bottom(), 60.0f, 1.0f);
  // Outward puts the whole width on one side.
  const SkPath outward =
      bandRegion(spine, profile::offset(20.0f), Formation::Outward);
  ASSERT_FALSE(outward.isEmpty());
  EXPECT_NEAR(outward.getBounds().height(), 20.0f, 1.0f);
  // A profile that is zero everywhere sweeps nothing.
  EXPECT_TRUE(bandRegion(spine, profile::self()).isEmpty());
}

// ---------------------------------------------------------------------------
// Crossings.

namespace {
SkPath segment(float x0, float y0, float x1, float y1) {
  SkPathBuilder b;
  b.moveTo(x0, y0);
  b.lineTo(x1, y1);
  return b.detach();
}
}  // namespace

TEST(Crossings, OnlyProperCrossingsAreReported) {
  // An X: one crossing, at the middle of both strands.
  const std::vector<SkPath> x{segment(0, 0, 100, 100), segment(0, 100, 100, 0)};
  const std::vector<Crossing> found = discoverCrossings(x);
  ASSERT_EQ(found.size(), 1u);
  EXPECT_EQ(found[0].a, 0u);
  EXPECT_EQ(found[0].b, 1u);
  EXPECT_NEAR(found[0].at.fX, 50.0f, 1.0f);
  EXPECT_NEAR(found[0].alongA, 0.5f, 0.02f);

  // A shared endpoint is a MEETING, not a crossing — otherwise every
  // polygon corner would be a knot.
  EXPECT_TRUE(
      discoverCrossings({segment(0, 0, 50, 50), segment(50, 50, 100, 0)})
          .empty());
  // Coincident strands never cross: that is what a stack of layers is.
  EXPECT_TRUE(discoverCrossings({segment(0, 0, 100, 0), segment(0, 0, 100, 0)})
                  .empty());
  // Fewer than two strands cannot cross.
  EXPECT_TRUE(discoverCrossings({segment(0, 0, 100, 100)}).empty());
}

TEST(Crossings, TheyAreNumberedAlongTheLowerIndexedStrand) {
  const std::vector<SkPath> ladder{segment(0, 50, 300, 50),
                                   segment(200, 0, 200, 100),
                                   segment(100, 0, 100, 100)};
  const std::vector<Crossing> found = discoverCrossings(ladder);
  ASSERT_EQ(found.size(), 2u);
  // Sorted by position on strand 0, then numbered — so the crossing at
  // x = 100 is index 0 even though its strand was added last.
  EXPECT_EQ(found[0].index, 0u);
  EXPECT_NEAR(found[0].at.fX, 100.0f, 1.0f);
  EXPECT_EQ(found[1].index, 1u);
  EXPECT_NEAR(found[1].at.fX, 200.0f, 1.0f);
}

TEST(CrossingRuleDecides, ListOrderAlternateSequencePairsAndPins) {
  const auto knot = [](size_t index, size_t a, size_t b) {
    Crossing c;
    c.index = index;
    c.a = a;
    c.b = b;
    return c;
  };
  // The default: `b` is later in the list, so `a` passes under.
  EXPECT_EQ(CrossingRule().decide(knot(0, 0, 1)), Order::Under);
  // alternate() IS sequence({Over, Under}) — two names, one machine.
  EXPECT_TRUE(crossing::alternate() ==
              crossing::sequence({Order::Over, Order::Under}));
  EXPECT_EQ(crossing::alternate().decide(knot(0, 0, 1)), Order::Over);
  EXPECT_EQ(crossing::alternate().decide(knot(1, 0, 1)), Order::Under);
  // Dominance, cycles legal — the impossible braid.
  const CrossingRule cyclic = crossing::pairs({{0, 1}, {1, 2}, {2, 0}});
  EXPECT_EQ(cyclic.decide(knot(0, 0, 1)), Order::Over);
  EXPECT_EQ(cyclic.decide(knot(0, 1, 2)), Order::Over);
  EXPECT_EQ(cyclic.decide(knot(0, 0, 2)), Order::Under);
  // A pin beats the rule beneath it, and re-pinning replaces rather than
  // stacks — there is one `.crossing` field and this is how it takes
  // exceptions.
  CrossingRule pinned = crossing::alternate();
  pinned.except(0, Order::Under).except(0, Order::Over);
  EXPECT_EQ(pinned.decide(knot(0, 0, 1)), Order::Over);
  EXPECT_FALSE(pinned == crossing::alternate());
}

TEST(CrossingPatch, TheLensIsBoundedByTheKnotsOwnTerritory) {
  const SkPath a = segment(0, 50, 200, 50);
  const SkPath b = segment(100, 0, 100, 100);
  const SkPath lens = crossingPatch(a, 8.0f, b, 8.0f, {100, 50}, 20.0f);
  ASSERT_FALSE(lens.isEmpty());
  EXPECT_TRUE(lens.getBounds().contains(SkRect::MakeLTRB(99, 49, 101, 51)));
  EXPECT_LE(lens.getBounds().width(), 41.0f);
  // Non-overlapping input still answers: a disc at the point, inside the
  // radius the caller allowed.
  const SkPath far =
      crossingPatch(a, 1.0f, segment(0, 900, 10, 900), 1.0f, {5, 900}, 6.0f);
  EXPECT_FALSE(far.isEmpty());
  EXPECT_LE(far.getBounds().width(), 13.0f);
}

// ---------------------------------------------------------------------------
// The two coordinate systems a figure is measured in.

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

TEST(Frame, PolarPointsAndTheArcLengthFractionRoundTrip) {
  const Frame f{
      .centre = {0, 0}, .radius = 100, .zero = Zero::North, .sense = Sense::CW};
  const SkPoint north = f.at(0, 1.0f);
  EXPECT_NEAR(north.fX, 0.0f, 1e-3f);
  EXPECT_NEAR(north.fY, -100.0f, 1e-3f);
  const SkPoint east = f.at(90, 1.0f);
  EXPECT_NEAR(east.fX, 100.0f, 1e-3f);
  EXPECT_NEAR(east.fY, 0.0f, 1e-3f);
  // fraction() and its inverse are the one place the circle's contour
  // start leaks, and they compose back to the angle they were given (mod
  // a turn, since a fraction has no memory of which lap it was on).
  for (float deg : {0.0f, 37.5f, 180.0f, 359.0f}) {
    float back = std::fmod(f.degOf(f.fraction(deg)) + 720.0f, 360.0f);
    EXPECT_NEAR(back, std::fmod(deg, 360.0f), 1e-2f);
  }
}

TEST(Frame, DerivedFramesKeepTheConventionTheyCameFrom) {
  const Frame f{.centre = {10, 20},
                .radius = 80,
                .zero = Zero::North,
                .sense = Sense::CCW,
                .originDeg = 4.5f};
  EXPECT_FLOAT_EQ(f.scaled(0.5f).radius, 40.0f);
  EXPECT_EQ(f.scaled(0.5f).zero, f.zero);
  EXPECT_EQ(f.about({0, 0}).sense, f.sense);
  // Turning composes and inverts, which is what makes a nudge safe.
  EXPECT_TRUE(f.turned(9.0f).turned(-9.0f) == f);
  // The box a silhouette inscribes itself in is centred on the frame.
  EXPECT_EQ(f.box(0.5f), SkRect::MakeXYWH(10 - 40, 20 - 40, 80, 80));
}

TEST(Grid, TheUnitMapScalesLengthsAndPositionsAndSnapsTheResult) {
  const Grid g{.scale = 4.0f, .origin = {10, 20}};
  EXPECT_FLOAT_EQ(g.s(3.0f), 12.0f);
  EXPECT_FLOAT_EQ(g.x(3.0f), 22.0f);
  EXPECT_FLOAT_EQ(g.y(3.0f), 32.0f);
  // Snap rounds half away from zero: 0.7 units is 2.8 px, which is more
  // than half a 5 px step, and 0.6 is not.
  const Grid snapped{.scale = 4.0f, .origin = {0, 0}, .snap = 5.0f};
  EXPECT_FLOAT_EQ(snapped.x(0.7f), 5.0f);
  EXPECT_FLOAT_EQ(snapped.x(0.6f), 0.0f);
  EXPECT_FLOAT_EQ(snapped.scaled(0.5f).scale, 2.0f);
  EXPECT_FLOAT_EQ(snapped.scaled(0.5f).snap, 5.0f);
}

}  // namespace
