/** @file
 * What a profile carried along a rail forms: a round profile a tube, a
 * flat one a strip, any flattened outline a closed tube of that section,
 * and a hung rail a banner that stays upright all the way round a loop.
 * The dials that size it — scale and taper — are the only things that
 * decide how big it comes out, because a profile is a UNIT shape.
 */

#include <gtest/gtest.h>
#include <include/core/SkPathBuilder.h>

#include <cmath>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/curve/Curve.h"
#include "sigilgeometry/mesh/pop/Sweep.h"
#include <sigilgeometry/kit/Sections.h>

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;

namespace {

TEST(MeshSweep, ARoundProfileFormsATubeAndAFlatOneAStrip) {
  curve::Spline3 arc;
  arc.points = {{0, 0, 0}, {60, 60, 0}, {120, 0, 0}};
  const Mesh t = pop::sweep(arc, sections::circle(8),
                            {.segments = 24, .scale = 8, .caps = true});
  EXPECT_GT(t.triangleCount(), 0u);
  EXPECT_EQ(t.normals.size(), t.vertexCount());
  for (const glm::vec3& n : t.normals) EXPECT_NEAR(glm::length(n), 1, 1e-3);
  // A line profile sweeps to a strip: `segments` cross-sections of two
  // vertices each, and two triangles per gap between consecutive sections.
  const Mesh r = pop::sweep(arc, sections::line(),
                            {.segments = 24,
                             .scale = 20,
                             .normals = pop::SweepOptions::Normals::Frame});
  EXPECT_EQ(r.vertexCount(), 48u);    // 24 * 2
  EXPECT_EQ(r.triangleCount(), 46u);  // (24 - 1) * 2
}

// A profile is a Polyline, so anything that flattens to one sweeps: the
// door the 2D shape vocabulary comes through. A closed outline wraps
// back onto its first point and forms one more quad per ring than an
// open one of the same point count.
TEST(MeshSweep, AnyFlattenedOutlineIsAProfile) {
  curve::Spline3 arc;
  arc.type = curve::Spline3::Type::Linear;
  arc.points = {{0, 0, 0}, {0, 0, 200}};
  SkPathBuilder square;
  square.moveTo(-10, -10).lineTo(10, -10).lineTo(10, 10).lineTo(-10, 10);
  square.close();
  const path::Polyline outline = pop::profile::fromPath(square.detach());
  EXPECT_TRUE(outline.closed);
  const Mesh box = pop::sweep(
      arc, outline,
      {.segments = 8, .normals = pop::SweepOptions::Normals::Geometric});
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
TEST(MeshSweep, ScaleAndTaperSizeTheProfile) {
  curve::Spline3 arc;
  arc.type = curve::Spline3::Type::Linear;
  arc.points = {{0, 0, 0}, {0, 0, 200}};
  const Mesh even =
      pop::sweep(arc, sections::circle(16), {.segments = 32, .scale = 10});
  glm::vec3 lo, hi;
  even.bounds(&lo, &hi);
  EXPECT_NEAR(hi.x - lo.x, 20, 0.2f);

  const Mesh cone = pop::sweep(
      arc, sections::circle(16),
      {.segments = 32, .scale = 10, .taper = [](float t) { return t; }});
  // The first ring collapses onto the curve and the last is full width.
  EXPECT_NEAR(glm::length(cone.positions[0] - arc.position(0)), 0, 1e-3);
  EXPECT_NEAR(glm::length(cone.positions.back() - arc.position(1)), 10, 0.2f);
}

TEST(MeshSweep, AHungRailKeepsAProfileUpright) {
  curve::Spline3 loop;
  loop.closed = true;
  for (int i = 0; i < 12; ++i) {
    const float a = (float)i / 12.0f * 2.0f * (float)M_PI;
    loop.points.emplace_back(300.0f * std::cos(a), 40.0f * std::sin(2 * a),
                             300.0f * std::sin(a));
  }
  const Mesh band =
      pop::sweep(curve::hangFrames(loop, 120), sections::line(),
                 {.scale = 50, .normals = pop::SweepOptions::Normals::Frame});
  ASSERT_EQ(band.vertexCount(), 240u);
  // A hung rail hangs: its across-vector is held vertical in world space
  // rather than rolling with the curve's frame, which keeps text upright
  // all the way round a closed loop. Vertices come in pairs per section, so
  // the first of every pair must sit ABOVE its partner in y everywhere.
  for (size_t i = 0; i + 1 < band.positions.size(); i += 2)
    EXPECT_GT(band.positions[i].y, band.positions[i + 1].y);
}

}  // namespace
