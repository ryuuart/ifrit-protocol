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

TEST(Curves, SweptProfilesAreWellFormed) {
  curve::Spline3 arc;
  arc.points = {{0, 0, 0}, {60, 60, 0}, {120, 0, 0}};
  const Mesh t = curve::sweep(arc, curve::profile::circle(8),
                              {.segments = 24, .scale = 8, .caps = true});
  EXPECT_GT(t.triangleCount(), 0u);
  EXPECT_EQ(t.normals.size(), t.vertexCount());
  for (const glm::vec3& n : t.normals) EXPECT_NEAR(glm::length(n), 1, 1e-3);
  // A line profile sweeps to a strip: `segments` cross-sections of two
  // vertices each, and two triangles per gap between consecutive sections.
  const Mesh r = curve::sweep(arc, curve::profile::line(),
                              {.segments = 24,
                               .scale = 20,
                               .normals = curve::SweepOptions::Normals::Frame});
  EXPECT_EQ(r.vertexCount(), 48u);    // 24 * 2
  EXPECT_EQ(r.triangleCount(), 46u);  // (24 - 1) * 2
}

// A profile is a Polyline, so anything that flattens to one sweeps: the
// door the 2D shape vocabulary comes through. A closed outline wraps
// back onto its first point and forms one more quad per ring than an
// open one of the same point count.
TEST(Curves, SweepCarriesAnyFlattenedOutline) {
  curve::Spline3 arc;
  arc.type = curve::Spline3::Type::Linear;
  arc.points = {{0, 0, 0}, {0, 0, 200}};
  SkPathBuilder square;
  square.moveTo(-10, -10).lineTo(10, -10).lineTo(10, 10).lineTo(-10, 10);
  square.close();
  const path::Polyline outline = curve::profile::fromPath(square.detach());
  EXPECT_TRUE(outline.closed);
  const Mesh box = curve::sweep(
      arc, outline,
      {.segments = 8, .normals = curve::SweepOptions::Normals::Geometric});
  EXPECT_EQ(box.vertexCount(), 8u * (uint32_t)outline.points.size());
  EXPECT_EQ(box.normals.size(), box.vertexCount());
  glm::vec3 lo, hi;
  box.bounds(&lo, &hi);
  EXPECT_NEAR(hi.x - lo.x, 20, 1e-3);
  EXPECT_NEAR(hi.z - lo.z, 200, 1e-3);
}

// `scale` sizes the profile and `taper` reshapes it along the curve.
// A profile is a UNIT shape precisely so these two dials, not the
// profile's own construction, decide how big the sweep is.
TEST(Curves, ScaleAndTaperSizeTheProfile) {
  curve::Spline3 arc;
  arc.type = curve::Spline3::Type::Linear;
  arc.points = {{0, 0, 0}, {0, 0, 200}};
  const Mesh even = curve::sweep(arc, curve::profile::circle(16),
                                 {.segments = 32, .scale = 10});
  glm::vec3 lo, hi;
  even.bounds(&lo, &hi);
  EXPECT_NEAR(hi.x - lo.x, 20, 0.2f);

  const Mesh cone = curve::sweep(
      arc, curve::profile::circle(16),
      {.segments = 32, .scale = 10, .taper = [](float t) { return t; }});
  // The first ring collapses onto the curve and the last is full width.
  EXPECT_NEAR(glm::length(cone.positions[0] - arc.position(0)), 0, 1e-3);
  EXPECT_NEAR(glm::length(cone.positions.back() - arc.position(1)), 10, 0.2f);
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

TEST(Curves, HungRailKeepsAProfileUpright) {
  curve::Spline3 loop;
  loop.closed = true;
  for (int i = 0; i < 12; ++i) {
    const float a = (float)i / 12.0f * 2.0f * (float)M_PI;
    loop.points.emplace_back(300.0f * std::cos(a), 40.0f * std::sin(2 * a),
                             300.0f * std::sin(a));
  }
  const Mesh band = curve::sweep(
      curve::hangFrames(loop, 120), curve::profile::line(),
      {.scale = 50, .normals = curve::SweepOptions::Normals::Frame});
  ASSERT_EQ(band.vertexCount(), 240u);
  // A hung rail hangs: its across-vector is held vertical in world space
  // rather than rolling with the curve's frame, which keeps text upright
  // all the way round a closed loop. Vertices come in pairs per section, so
  // the first of every pair must sit ABOVE its partner in y everywhere.
  for (size_t i = 0; i + 1 < band.positions.size(); i += 2)
    EXPECT_GT(band.positions[i].y, band.positions[i + 1].y);
}

// The three shapes written longhand: a tube, a ribbon and a banner,
// each formed by its own dedicated body rather than by a profile on a
// rail. Holding the sweep against these is what makes "the same shape"
// mean the same floats and not merely the same picture — a rendered
// plate is compared byte for byte downstream, so a rounding step that
// moved would show up there as a changed image.
namespace reference {

Mesh tube(const curve::Spline3& spline, float radius,
          const std::function<float(float)>& taper, int segments, int sides,
          bool caps, glm::vec3 up) {
  Mesh out;
  segments = std::max(segments, 2);
  sides = std::max(sides, 3);
  const std::vector<curve::Frame3> rail = curve::frames(spline, segments, up);

  for (int i = 0; i < segments; ++i) {
    const curve::Frame3& f = rail[(size_t)i];
    const float r = radius * (taper ? std::max(taper(f.t), 0.0f) : 1.0f);
    for (int s = 0; s <= sides; ++s) {  // seam duplicated for clean UVs
      const float a = (float)s / (float)sides * 2.0f * (float)M_PI;
      const glm::vec3 dir = f.normal * std::cos(a) + f.binormal * std::sin(a);
      out.positions.push_back(f.position + dir * r);
      out.normals.push_back(dir);
      out.uvs.emplace_back((float)s / (float)sides, f.t);
    }
  }
  const int ring = sides + 1;
  for (int i = 0; i + 1 < segments; ++i)
    for (int s = 0; s < sides; ++s) {
      const uint32_t a = (uint32_t)(i * ring + s);
      const uint32_t b = a + 1;
      const uint32_t c = a + (uint32_t)ring;
      const uint32_t d = c + 1;
      out.indices.insert(out.indices.end(), {a, b, d, a, d, c});
    }

  if (caps && !spline.closed) {
    for (int end = 0; end < 2; ++end) {
      const curve::Frame3& f = rail[end == 0 ? 0 : rail.size() - 1];
      const glm::vec3 n = end == 0 ? f.tangent * -1.0f : f.tangent;
      const uint32_t center = (uint32_t)out.positions.size();
      out.positions.push_back(f.position);
      out.normals.push_back(n);
      out.uvs.emplace_back(0.5f, end == 0 ? 0.0f : 1.0f);
      const uint32_t ringStart =
          (uint32_t)((end == 0 ? 0 : segments - 1) * ring);
      for (int s = 0; s < sides; ++s) {
        const uint32_t a = ringStart + (uint32_t)s;
        const uint32_t b = ringStart + (uint32_t)s + 1;
        if (end == 0)
          out.indices.insert(out.indices.end(), {center, b, a});
        else
          out.indices.insert(out.indices.end(), {center, a, b});
      }
    }
  }
  return out;
}

Mesh ribbon(const curve::Spline3& spline, float width,
            const std::function<float(float)>& taper, int segments,
            glm::vec3 up) {
  Mesh out;
  segments = std::max(segments, 2);
  const std::vector<curve::Frame3> rail = curve::frames(spline, segments, up);
  for (int i = 0; i < segments; ++i) {
    const curve::Frame3& f = rail[(size_t)i];
    const float half =
        width * 0.5f * (taper ? std::max(taper(f.t), 0.0f) : 1.0f);
    out.positions.push_back(f.position - f.binormal * half);
    out.positions.push_back(f.position + f.binormal * half);
    out.normals.push_back(f.normal);
    out.normals.push_back(f.normal);
    out.uvs.emplace_back(0, f.t);
    out.uvs.emplace_back(1, f.t);
  }
  for (int i = 0; i + 1 < segments; ++i) {
    const uint32_t a = (uint32_t)(i * 2);
    out.indices.insert(out.indices.end(), {a, a + 1, a + 3, a, a + 3, a + 2});
  }
  return out;
}

Mesh banner(const curve::Spline3& spline, float width, float head, float span,
            int sections) {
  Mesh out;
  sections = std::max(sections, 2);
  const float half = width * 0.5f;
  const auto wrap01 = [](float t) { return t - std::floor(t); };
  glm::vec3 hang = {0, -1, 0};  // carried through vertical stretches
  for (int i = 0; i < sections; ++i) {
    const float f = (float)i / (float)(sections - 1);
    const float t = wrap01(head - span + span * f);
    const glm::vec3 p = spline.position(t);
    glm::vec3 tangent = spline.position(wrap01(t + 0.002f)) -
                        spline.position(wrap01(t - 0.002f));
    const float len = glm::length(tangent);
    if (len > 1e-6f) tangent = tangent * (1.0f / len);
    glm::vec3 down =
        glm::vec3{0, -1, 0} - tangent * glm::dot(tangent, glm::vec3{0, -1, 0});
    const float downLen = glm::length(down);
    if (downLen > 0.15f) hang = down * (1.0f / downLen);
    const glm::vec3 normal = glm::cross(hang, tangent);
    out.positions.push_back(p - hang * half);  // u = 0: top edge
    out.positions.push_back(p + hang * half);
    out.normals.push_back(normal);
    out.normals.push_back(normal);
    out.uvs.emplace_back(0, f);
    out.uvs.emplace_back(1, f);
  }
  for (int i = 0; i + 1 < sections; ++i) {
    const uint32_t a = (uint32_t)(i * 2);
    out.indices.insert(out.indices.end(), {a, a + 1, a + 3, a, a + 3, a + 2});
  }
  return out;
}

/** Every lane, value for value. */
void expectSame(const Mesh& made, const Mesh& want) {
  ASSERT_EQ(made.positions.size(), want.positions.size());
  ASSERT_EQ(made.normals.size(), want.normals.size());
  ASSERT_EQ(made.uvs.size(), want.uvs.size());
  ASSERT_EQ(made.indices, want.indices);
  for (size_t i = 0; i < want.positions.size(); ++i) {
    EXPECT_EQ(made.positions[i], want.positions[i]) << "position " << i;
    EXPECT_EQ(made.normals[i], want.normals[i]) << "normal " << i;
    EXPECT_EQ(made.uvs[i], want.uvs[i]) << "uv " << i;
  }
}

curve::Spline3 knot() {
  curve::Spline3 spline;
  spline.closed = true;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    spline.points.emplace_back(std::cos(a) * 300, std::sin(a * 3.0f) * 110,
                               std::sin(a) * 300);
  }
  return spline;
}

curve::Spline3 arc() {
  curve::Spline3 spline;
  spline.points = {
      {-820, 260, -320}, {-300, 420, 60}, {260, 300, 220}, {820, 430, -260}};
  return spline;
}

}  // namespace reference

// A circle profile IS the tube: the ring the old generator evaluated
// around each frame, emitted once as a unit contour with its seam point
// duplicated, then scaled on the frame. Both the open case (which grows
// end caps) and the closed one (which cannot) have to land on the same
// floats.
TEST(Curves, CircleProfileReproducesTheTube) {
  reference::expectSame(
      curve::sweep(reference::knot(), curve::profile::circle(12),
                   {.segments = 220, .scale = 9}),
      reference::tube(reference::knot(), 9, nullptr, 220, 12, true, {0, 1, 0}));
  reference::expectSame(
      curve::sweep(reference::arc(), curve::profile::circle(10),
                   {.segments = 180, .scale = 7, .caps = true}),
      reference::tube(reference::arc(), 7, nullptr, 180, 10, true, {0, 1, 0}));
  // The taper multiplies the radius exactly where the old profile
  // function did — before the offset leaves the frame, not after.
  const std::function<float(float)> pinch = [](float t) {
    return 0.2f + std::sin(t * 3.0f);
  };
  reference::expectSame(
      curve::sweep(reference::arc(), curve::profile::circle(6),
                   {.segments = 40,
                    .scale = 12,
                    .taper = pinch,
                    .up = {0, 0, 1},
                    .caps = true}),
      reference::tube(reference::arc(), 12, pinch, 40, 6, true, {0, 0, 1}));
}

// A two-point line profile IS the ribbon, and the rail's own normal is
// its facing — a flat band has one, where a round profile's outward is
// the offset itself.
TEST(Curves, LineProfileReproducesTheRibbon) {
  const std::function<float(float)> swell = [](float t) {
    return 1.0f - 0.6f * t;
  };
  for (const std::function<float(float)>& taper :
       {std::function<float(float)>{}, swell}) {
    reference::expectSame(
        curve::sweep(reference::knot(), curve::profile::line(),
                     {.segments = 220,
                      .scale = 30,
                      .taper = taper,
                      .normals = curve::SweepOptions::Normals::Frame}),
        reference::ribbon(reference::knot(), 30, taper, 220, {0, 1, 0}));
    reference::expectSame(
        curve::sweep(reference::arc(), curve::profile::line(),
                     {.segments = 96,
                      .scale = 42,
                      .taper = taper,
                      .up = {1, 0, 0},
                      .normals = curve::SweepOptions::Normals::Frame}),
        reference::ribbon(reference::arc(), 42, taper, 96, {1, 0, 0}));
  }
}

// The banner was the same line profile on a different RAIL: a window of
// a closed loop walked in parameter with its across-vector held world
// vertical. Splitting the rail from the sweep is what let the third
// generator go without changing a vertex.
TEST(Curves, LineProfileOnAHungRailReproducesTheBanner) {
  const curve::Spline3 loop = reference::knot();
  for (int k = 0; k < 4; ++k) {
    const float head = (float)(k + 1) / 4.0f;
    reference::expectSame(
        curve::sweep(
            curve::hangFrames(loop, 160, head, 0.25f), curve::profile::line(),
            {.scale = 64, .normals = curve::SweepOptions::Normals::Frame}),
        reference::banner(loop, 64, head, 0.25f, 160));
  }
}
