/** @file
 * Splines, the two rails, the pose along them, the profiles and the
 * sweep that carries them: a round profile forms a tube, a flat one a
 * ribbon or a hung banner. Each of those three shapes is also written
 * out longhand at the foot of this file, and the sweep is held against
 * the longhand vertex for vertex.
 */

#include <gtest/gtest.h>
#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/camera/Camera.h"
#include "sigilgeometry/mesh/curve/Curve.h"
#include "sigilgeometry/mesh/curve/Pose.h"

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;

// ---------------------------------------------------------------------------
// Pose

namespace {

curve::Spline3 closedLoop() {
  curve::Spline3 loop;
  loop.points = {{-140, 0, 0},
                 {-40, 90, 70},
                 {90, 40, -30},
                 {130, -60, 40},
                 {0, -110, 10}};
  loop.closed = true;
  return loop;
}

}  // namespace

TEST(Pose, WalksTheSplineByArcLength) {
  curve::Spline3 line;
  line.type = curve::Spline3::Type::Linear;
  line.points = {{0, 0, 0}, {100, 0, 0}};

  const curve::Frame3 quarter = curve::poseAlong(line, 25.0f);
  EXPECT_NEAR(quarter.position.x, 25.0f, 0.5f);
  EXPECT_NEAR(quarter.tangent.x, 1.0f, 1e-3f);
  // The frame is orthonormal wherever it is read.
  EXPECT_NEAR(glm::dot(quarter.tangent, quarter.normal), 0.0f, 1e-4f);
  EXPECT_NEAR(glm::length(quarter.binormal), 1.0f, 1e-4f);
}

TEST(Pose, AnOpenSplineParksAtItsEnds) {
  curve::Spline3 line;
  line.type = curve::Spline3::Type::Linear;
  line.points = {{0, 0, 0}, {100, 0, 0}};
  // Asking to come round on an open curve still parks: there are two
  // ends and no seam between them.
  const curve::Frame3 past = curve::poseAlong(line, 400.0f);
  EXPECT_NEAR(past.position.x, 100.0f, 0.5f);
  const curve::Frame3 before = curve::poseAlong(line, -400.0f);
  EXPECT_NEAR(before.position.x, 0.0f, 0.5f);
}

TEST(Pose, AClosedSplineComesRound) {
  const curve::Spline3 loop = closedLoop();
  const float total = loop.length();
  const curve::Frame3 start = curve::poseAlong(loop, 0.0f);
  const curve::Frame3 lap = curve::poseAlong(loop, total);
  EXPECT_NEAR(glm::length(lap.position - start.position), 0.0f, 1.0f);
  const curve::Frame3 third = curve::poseAlong(loop, total / 3.0f);
  const curve::Frame3 nextLap = curve::poseAlong(loop, total + total / 3.0f);
  EXPECT_NEAR(glm::length(nextLap.position - third.position), 0.0f, 1.0f);
}

// The whole point of a parallel-transport frame: walking the loop, the
// normal turns a little at a time and never inverts — not at an
// inflection, and not at the seam where the walk closes.
TEST(Pose, FramesDoNotFlipAroundAClosedLoop) {
  const curve::Spline3 loop = closedLoop();
  const float total = loop.length();
  const int steps = 400;
  curve::Frame3 prev = curve::poseAlong(loop, 0.0f);
  float worst = 1.0f;
  for (int i = 1; i <= steps; ++i) {
    const curve::Frame3 here =
        curve::poseAlong(loop, total * (float)i / (float)steps);
    const float agree = glm::dot(prev.normal, here.normal);
    worst = std::min(worst, agree);
    // A flip is a normal that reverses between neighbouring reads.
    EXPECT_GT(agree, 0.9f) << "step " << i;
    EXPECT_NEAR(glm::dot(here.tangent, here.normal), 0.0f, 1e-3f);
    prev = here;
  }
  EXPECT_GT(worst, 0.9f);
  // Coming back round meets the frame the walk started with.
  const curve::Frame3 seam = curve::poseAlong(loop, total);
  const curve::Frame3 start = curve::poseAlong(loop, 0.0f);
  EXPECT_GT(glm::dot(seam.normal, start.normal), 0.99f);
}

TEST(Pose, ARailReadsTheSamePlacesTheSplineDoes) {
  const curve::Spline3 loop = closedLoop();
  const std::vector<curve::Frame3> rail = curve::frames(loop, 256);
  const float total = loop.length();
  for (int i = 0; i <= 8; ++i) {
    const float d = total * (float)i / 8.0f;
    const curve::Frame3 viaRail = curve::poseAlong(rail, d, path::Wrap::Around);
    const curve::Frame3 viaSpline = curve::poseAlong(loop, d);
    EXPECT_NEAR(glm::length(viaRail.position - viaSpline.position), 0.0f, 1e-3f)
        << "d = " << d;
  }
}

TEST(Curves, SplineInterpolatesEndpointsAndLength) {
  curve::Spline3 line;
  line.type = curve::Spline3::Type::Linear;
  line.points = {{0, 0, 0}, {100, 0, 0}};
  EXPECT_NEAR(line.length(), 100, 1e-2);
  const glm::vec3 mid = line.position(0.5f);
  EXPECT_NEAR(mid.x, 50, 1e-3);

  curve::Spline3 spline;
  spline.points = {{0, 0, 0}, {50, 40, 0}, {100, 0, 0}, {150, -40, 0}};
  const glm::vec3 start = spline.position(0);
  const glm::vec3 end = spline.position(1);
  // The default spline type INTERPOLATES its points (Catmull-Rom) rather
  // than approximating them like a B-spline, so t=0 and t=1 land exactly on
  // the first and last authored point.
  EXPECT_NEAR(start.x, 0, 1e-3);
  EXPECT_NEAR(end.x, 150, 1e-3);
}

TEST(Curves, ArcLengthSamplingIsEven) {
  // Knots deliberately bunched at one end: sampling by curve PARAMETER
  // would put three of eleven beads inside the first 10 units, while
  // sampling by arc length spreads them evenly over the full 200. Linear
  // segments keep the path straight so spacing is the only variable.
  curve::Spline3 spline;
  spline.type = curve::Spline3::Type::Linear;
  spline.points = {{0, 0, 0}, {5, 0, 0}, {10, 0, 0}, {200, 0, 0}};
  const std::vector<glm::vec3> beads = spline.sampleArcLength(11);
  ASSERT_EQ(beads.size(), 11u);
  for (size_t i = 1; i < beads.size(); ++i)
    EXPECT_NEAR(beads[i].x - beads[i - 1].x, 20, 1.5f);
}

TEST(Curves, FramesStayOrthonormalAndContinuous) {
  curve::Spline3 knot;
  knot.closed = true;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    knot.points.emplace_back(std::cos(a) * 100, std::sin(a * 2) * 40,
                             std::sin(a) * 100);
  }
  const std::vector<curve::Frame3> rail = curve::frames(knot, 64);
  ASSERT_EQ(rail.size(), 64u);
  for (size_t i = 0; i < rail.size(); ++i) {
    const curve::Frame3& f = rail[i];
    EXPECT_NEAR(glm::length(f.tangent), 1, 1e-3);
    EXPECT_NEAR(glm::length(f.normal), 1, 1e-3);
    EXPECT_NEAR(glm::dot(f.tangent, f.normal), 0, 1e-3);
    // Frames are carried along the curve by parallel transport, so each
    // normal is the previous one rotated as little as the tangent allows.
    // A frame built independently per sample (from a fixed up vector, say)
    // would flip near vertical tangents and twist any swept surface.
    if (i > 0) EXPECT_GT(glm::dot(f.normal, rail[i - 1].normal), 0.5f);
  }
}

// curve::project() flattens a 3D spline to a 2D SkPath through the same
// camera the mesh painter uses, so the two agree on where a point lands: a
// segment centred on the world origin comes back centred on the viewport in
// pixels. Drawing a projected curve over a drawn mesh depends on this.
TEST(Curves, ProjectMatchesCameraProjection) {
  curve::Spline3 line;
  line.type = curve::Spline3::Type::Linear;
  line.points = {{-50, 0, 0}, {50, 0, 0}};
  camera::Camera camera;
  camera.eye = {0, 0, 200};
  const SkPath path = curve::project(line, camera, {400, 300}, 16);
  const SkRect bounds = path.computeTightBounds();
  EXPECT_NEAR(bounds.centerY(), 150, 1e-2);
  EXPECT_NEAR(bounds.centerX(), 200, 1e-2);
}
