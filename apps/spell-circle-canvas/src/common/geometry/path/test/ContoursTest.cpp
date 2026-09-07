/** @file
 * Contours addressed by arc length, and the pose read off one: where a
 * distance along a shape lands, which way the mark faces there, and what
 * a walk does at a seam it can or cannot come round through.
 */

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
#include "sigilgeometry/path/Pose.h"
#include "sigilgeometry/path/Skia.h"
#include "support/Paths.h"

using namespace sigil::geometry::path;
using sigil::geometry::test::square;

namespace {

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

TEST(Contour, ParallelClampsAResampleStepThatCannotAdvance) {
  // The step is how far apart the offset is resampled, so a zero or
  // negative one describes a walk that never advances. It is clamped to
  // something that does, rather than answering an empty path or looping.
  SkPathBuilder builder;
  builder.moveTo(10, 50);
  builder.lineTo(190, 50);
  const SkPath route = builder.detach();
  for (float step : {0.0f, -4.0f}) {
    const SkPath shifted = parallel(route, -10.0f, step);
    ASSERT_FALSE(shifted.isEmpty()) << "step=" << step;
    EXPECT_NEAR(shifted.getBounds().top(), 60.0f, 0.01f);
    EXPECT_NEAR(shifted.getBounds().bottom(), 60.0f, 0.01f);
  }
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

}  // namespace
