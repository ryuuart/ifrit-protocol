/** @file
 * Splines, frames and the swept generators: tubes, ribbons and banners.
 */

#include <gtest/gtest.h>

#include <cmath>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/camera/Camera.h"
#include "sigilgeometry/mesh/curve/Curve.h"

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;

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

TEST(Curves, TubeAndRibbonAreWellFormed) {
  curve::Spline3 arc;
  arc.points = {{0, 0, 0}, {60, 60, 0}, {120, 0, 0}};
  const Mesh t = curve::tube(arc, {.radius = 8, .segments = 24, .sides = 8});
  EXPECT_GT(t.triangleCount(), 0u);
  EXPECT_EQ(t.normals.size(), t.vertexCount());
  for (const glm::vec3& n : t.normals) EXPECT_NEAR(glm::length(n), 1, 1e-3);
  // A ribbon is a strip: `segments` cross-sections of two vertices each,
  // and two triangles per gap between consecutive sections.
  const Mesh r = curve::ribbon(arc, {.width = 20, .segments = 24});
  EXPECT_EQ(r.vertexCount(), 48u);    // 24 * 2
  EXPECT_EQ(r.triangleCount(), 46u);  // (24 - 1) * 2
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

TEST(Curves, BannerHangsGravityUpright) {
  curve::Spline3 loop;
  loop.closed = true;
  for (int i = 0; i < 12; ++i) {
    const float a = (float)i / 12.0f * 2.0f * (float)M_PI;
    loop.points.emplace_back(300.0f * std::cos(a), 40.0f * std::sin(2 * a),
                             300.0f * std::sin(a));
  }
  const Mesh band = curve::banner(loop, {.width = 50, .sections = 120});
  ASSERT_EQ(band.vertexCount(), 240u);
  // A banner hangs: its width is held vertical in world space rather than
  // rolling with the curve's frame, which is what keeps text on it upright
  // all the way round a closed loop. Vertices come in pairs per section, so
  // the first of every pair must sit ABOVE its partner in y everywhere.
  for (size_t i = 0; i + 1 < band.positions.size(); i += 2)
    EXPECT_GT(band.positions[i].y, band.positions[i + 1].y);
}
