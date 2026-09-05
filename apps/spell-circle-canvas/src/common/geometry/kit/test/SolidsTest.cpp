/** @file
 * The solid shelf: a path lifted into a solid with its caps and walls, a
 * profile lathed, and the named surfaces closed and unit-normalled.
 */

#include <gtest/gtest.h>
#include <include/core/SkPathBuilder.h>
#include <sigilgeometry/kit/Solids.h>

#include <cmath>
#include <glm/geometric.hpp>

#include "support/Paths.h"

using namespace sigil::geometry::mesh;
using sigil::geometry::test::rect;
using sigil::geometry::test::square;

namespace {

bool normalsAreUnit(const Mesh& mesh) {
  for (const glm::vec3& n : mesh.normals)
    if (std::abs(glm::length(n) - 1.0f) > 1e-3f) return false;
  return true;
}

}  // namespace

TEST(Solids, ExtrudeLiftsAPathIntoASolidAndDroppingCapsLeavesAShell) {
  const Mesh solid = extrude(square(10), {.depth = 4});
  EXPECT_FALSE(solid.positions.empty());
  EXPECT_GT(solid.triangleCount(), 0u);
  EXPECT_EQ(solid.normals.size(), solid.positions.size());
  EXPECT_EQ(solid.uvs.size(), solid.positions.size());
  glm::vec3 lo, hi;
  solid.bounds(&lo, &hi);
  EXPECT_NEAR(hi.z - lo.z, 4.0f, 1e-4f);  // the depth is total, centred on 0

  const Mesh shell =
      extrude(square(10), {.depth = 4, .frontCap = false, .backCap = false});
  EXPECT_LT(shell.triangleCount(), solid.triangleCount());

  // A rectangle keeps its own two extents and gains the depth as the
  // third, and a four-sided profile with both caps is six quads: two caps
  // and four walls, two triangles each.
  const Mesh box = extrude(rect(0, 0, 100, 60), {.depth = 20});
  box.bounds(&lo, &hi);
  EXPECT_NEAR(hi.x - lo.x, 100.0f, 1e-3f);
  EXPECT_NEAR(hi.y - lo.y, 60.0f, 1e-3f);
  EXPECT_NEAR(hi.z - lo.z, 20.0f, 1e-3f);
  EXPECT_EQ(box.triangleCount(), 12u);
  EXPECT_TRUE(normalsAreUnit(box));
}

TEST(Solids, ExtrudeAnnulusKeepsHole) {
  SkPathBuilder ring;
  ring.addCircle(0, 0, 80);
  ring.addCircle(0, 0, 40, SkPathDirection::kCCW);
  Mesh m = extrude(ring.detach(), {.depth = 10});
  ASSERT_GT(m.triangleCount(), 0u);
  // The hole must survive triangulation, which is checked by area rather
  // than by counting triangles: the tessellation is free to change, the
  // covered area is not. Extrusion centres the profile on z, so the whole
  // front cap sits at z = +depth/2 and can be picked out by that alone.
  // The 3% slack absorbs the circle being flattened to a polygon, which
  // always under-measures.
  double area = 0;
  for (size_t t = 0; t + 2 < m.indices.size(); t += 3) {
    const glm::vec3& a = m.positions[m.indices[t]];
    const glm::vec3& b = m.positions[m.indices[t + 1]];
    const glm::vec3& c = m.positions[m.indices[t + 2]];
    if (a.z > 4.9f && b.z > 4.9f && c.z > 4.9f) {
      const glm::vec3 ab = b - a, ac = c - a;
      area += 0.5 * std::abs((double)ab.x * ac.y - (double)ab.y * ac.x);
    }
  }
  const double expected = M_PI * (80.0 * 80.0 - 40.0 * 40.0);
  EXPECT_NEAR(area / expected, 1.0, 0.03);
}

TEST(Solids, RevolveLathesAProfileAroundTheAxisAndAPartialSweepStopsShort) {
  const std::vector<glm::vec2> profile = {{1, 0}, {2, 1}, {1, 2}};
  const Mesh full = revolve(profile, {.segments = 16});
  EXPECT_EQ(full.uvs.size(), full.positions.size());
  EXPECT_TRUE(normalsAreUnit(full));
  glm::vec3 lo, hi;
  full.bounds(&lo, &hi);
  EXPECT_NEAR(hi.y - lo.y, 2.0f, 1e-4f);  // the profile's height, lathed
  EXPECT_NEAR(hi.x, 2.0f, 1e-4f);
  EXPECT_NEAR(lo.x, -2.0f, 1e-4f);  // all the way round

  // A partial sweep leaves the surface open, so it never reaches the far
  // side the full turn covers.
  const Mesh quarter = revolve(profile, {.segments = 16, .sweepDeg = 90});
  quarter.bounds(&lo, &hi);
  EXPECT_GT(lo.x, -1e-4f);
}

TEST(Solids, TheNamedSurfacesAreTheSheetSeamEvaluated) {
  glm::vec3 lo, hi;
  const Mesh sphere = superellipsoid({2, 2, 2}, 2.0f, 24, 16);
  EXPECT_TRUE(normalsAreUnit(sphere));
  sphere.bounds(&lo, &hi);
  EXPECT_NEAR(hi.y, 2.0f, 1e-2f);

  // A flat panel is the degenerate curve, and it is the currency's own
  // quad — the shelf's panel bends where the quad cannot.
  const Mesh flat = cylinderPanel(4, 2, 0, 8, 4);
  flat.bounds(&lo, &hi);
  EXPECT_NEAR(hi.z - lo.z, 0.0f, 1e-4f);
  const Mesh curved = cylinderPanel(4, 2, 3, 8, 4);
  curved.bounds(&lo, &hi);
  EXPECT_GT(hi.z - lo.z, 0.0f);
}

TEST(Solids, TorusNormalsPointOutward) {
  Mesh m = torus(100, 30, 32, 16);
  // Parameterization convention: the first vertex is u = v = 0, which is on
  // the outer equator facing +x, so its normal points outward along +x. An
  // inward-facing generator would put the normal at -x and light the torus
  // inside out.
  const glm::vec3 n0 = m.normals.front();
  EXPECT_GT(std::abs(n0.x), 0.7f);
  EXPECT_TRUE(normalsAreUnit(m));
  // …and the surface reaches the major radius plus the tube.
  glm::vec3 lo, hi;
  m.bounds(&lo, &hi);
  EXPECT_NEAR(hi.x, 130.0f, 1e-1f);
}

TEST(Solids, TheBoxKeepsEachFaceFlatAndDroppingOneCostsItsTriangles) {
  const Mesh whole = box({-1, -2, -3}, {1, 2, 3});
  EXPECT_EQ(whole.triangleCount(), 12u);
  // Four vertices per face, unshared, so every corner is hard: a shared
  // corner would average three normals and round the box off.
  EXPECT_EQ(whole.vertexCount(), 24u);
  EXPECT_TRUE(normalsAreUnit(whole));
  glm::vec3 lo, hi;
  whole.bounds(&lo, &hi);
  EXPECT_NEAR(hi.x - lo.x, 2.0f, 1e-4f);
  EXPECT_NEAR(hi.y - lo.y, 4.0f, 1e-4f);
  EXPECT_NEAR(hi.z - lo.z, 6.0f, 1e-4f);
  // Every face carries its own UV square rather than a slice of one.
  EXPECT_EQ(whole.uvs.size(), whole.positions.size());
  EXPECT_EQ(whole.uvs[0], (glm::vec2{0, 0}));
  EXPECT_EQ(whole.uvs[2], (glm::vec2{1, 1}));

  // The face nothing sees is the face nothing pays for.
  const Mesh open = box({-1, -2, -3}, {1, 2, 3}, {.bottom = false});
  EXPECT_EQ(open.triangleCount(), 10u);
  for (const glm::vec3& n : open.normals) EXPECT_GT(n.y, -0.5f);

  // Handed the corners the other way about, it is the same box wound the
  // same way out, not one turned inside out.
  const Mesh reversed = box({1, 2, 3}, {-1, -2, -3});
  EXPECT_EQ(reversed.positions, whole.positions);
  EXPECT_EQ(reversed.normals, whole.normals);
}

TEST(Solids, ABoxTakesColourOnlyWhenItIsAskedFor) {
  // No tint, no colors lane: the fill decides, as it does for every other
  // solid on the shelf.
  EXPECT_TRUE(box({0, 0, 0}, {1, 1, 1}).colors.empty());

  const Mesh shaded = box({0, 0, 0}, {1, 1, 1},
                          {.tint = {1.0f, 0.5f, 0.25f, 1.0f},
                           .sideShade = 0.5f});
  ASSERT_EQ(shaded.colors.size(), shaded.positions.size());
  // The four side faces darken and the two horizontal ones do not, so the
  // top of a block reads as its own colour and the walls fall away.
  for (size_t i = 0; i < shaded.positions.size(); ++i) {
    const float expected = std::abs(shaded.normals[i].y) > 0.5f ? 1.0f : 0.5f;
    EXPECT_NEAR(shaded.colors[i].r, 1.0f * expected, 1e-5f);
    EXPECT_NEAR(shaded.colors[i].g, 0.5f * expected, 1e-5f);
    EXPECT_NEAR(shaded.colors[i].a, 1.0f, 1e-5f);  // alpha is not shaded
  }
}
