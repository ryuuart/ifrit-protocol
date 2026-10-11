/** @file
 * The solid shelf: a path lifted into a solid with its caps and walls, a
 * profile lathed, the named surfaces closed and unit-normalled, and the
 * five regular solids counted, closed and stood on a chosen face.
 */

#include <gtest/gtest.h>
#include <include/core/SkPathBuilder.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Faces.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <vector>

#include "support/Paths.h"

using namespace sigil::geometry::mesh;
namespace path = sigil::geometry::path;
using sigil::geometry::test::rect;
using sigil::geometry::test::square;

namespace {

bool normalsAreUnit(const Mesh& mesh) {
  for (const glm::vec3& n : mesh.normals)
    if (std::abs(glm::length(n) - 1.0f) > 1e-3f) return false;
  return true;
}

/** How many of @p mesh's triangles face toward @p centre — or, when
 *  @p away is false, away from it — by their winding or by the normals at
 *  their corners; a triangle with no area faces nowhere and is not
 *  counted. */
int facingWrongly(const Mesh& mesh, glm::vec3 centre, bool away = true) {
  int wrongly = 0;
  for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
    const glm::vec3 a = mesh.positions[mesh.indices[i]];
    const glm::vec3 b = mesh.positions[mesh.indices[i + 1]];
    const glm::vec3 c = mesh.positions[mesh.indices[i + 2]];
    glm::vec3 face = glm::cross(b - a, c - a);
    if (glm::length(face) < 1e-6f) continue;
    bool wrong =
        glm::dot(face, (a + b + c) / 3.0f - centre) * (away ? 1 : -1) <= 0;
    for (size_t k = 0; k < 3; ++k)
      wrong = wrong || glm::dot(mesh.normals[mesh.indices[i + k]], face) <= 0;
    if (wrong) ++wrongly;
  }
  return wrongly;
}

/** A square ring of @p half across each way at depth @p z, anticlockwise
 *  seen from +z, or clockwise when @p clockwise. */
std::vector<glm::vec3> squareRing(float half, float z, bool clockwise) {
  std::vector<glm::vec3> ring = {
      {-half, -half, z}, {half, -half, z}, {half, half, z}, {-half, half, z}};
  if (clockwise) std::reverse(ring.begin(), ring.end());
  return ring;
}

}  // namespace

TEST(Solids, ExtrudeLiftsAPathIntoASolidAndDroppingCapsLeavesAShell) {
  const Mesh solid = extrude(path::fromSk(square(10)), {.depth = 4});
  EXPECT_FALSE(solid.positions.empty());
  EXPECT_GT(solid.triangleCount(), 0u);
  EXPECT_EQ(solid.normals.size(), solid.positions.size());
  EXPECT_EQ(solid.uvs.size(), solid.positions.size());
  glm::vec3 lo, hi;
  solid.bounds(&lo, &hi);
  EXPECT_NEAR(hi.z - lo.z, 4.0f, 1e-4f);  // the depth is total, centred on 0

  const Mesh shell = extrude(path::fromSk(square(10)),
                             {.depth = 4, .frontCap = false, .backCap = false});
  EXPECT_LT(shell.triangleCount(), solid.triangleCount());

  // A rectangle keeps its own two extents and gains the depth as the
  // third, and a four-sided profile with both caps is six quads: two caps
  // and four walls, two triangles each.
  const Mesh box = extrude(path::fromSk(rect(0, 0, 100, 60)), {.depth = 20});
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
  Mesh m = extrude(path::fromSk(ring.detach()), {.depth = 10});
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

// A round of radius r turns each wall into its cap through a quarter
// circle: every rim vertex stands on the circle of radius r about the
// corner it rounds, and its normal is that circle's, so going up the rim
// the normal turns from the wall's straight out to the cap's straight up
// in steps no wider than a quarter turn over the segments.
TEST(Solids, ExtrudeRoundsTheEdgeFromTheCapToTheWall) {
  const float radius = 3;
  const int steps = 6;
  const Mesh rounded =
      extrude(path::fromSk(square(40)), {.depth = 20,
                                         .edge = ExtrudeEdge::Round,
                                         .edgeSize = radius,
                                         .edgeSegments = steps});
  ASSERT_GT(rounded.triangleCount(), 12u);
  EXPECT_TRUE(normalsAreUnit(rounded));
  EXPECT_EQ(facingWrongly(rounded, {0, 0, 0}), 0);
  glm::vec3 lo, hi;
  rounded.bounds(&lo, &hi);
  EXPECT_NEAR(hi.x - lo.x, 40.0f, 1e-3f);
  EXPECT_NEAR(hi.z - lo.z, 20.0f, 1e-3f);

  // The rim along the +x wall at the front: the segment's own vertices,
  // whose normals have no part along y.
  std::vector<float> turns;
  for (size_t i = 0; i < rounded.positions.size(); ++i) {
    const glm::vec3 p = rounded.positions[i];
    const glm::vec3 n = rounded.normals[i];
    if (n.x < -1e-3f || std::abs(n.y) > 1e-3f || n.z < -1e-3f) continue;
    if (p.z < 10 - radius - 1e-3f || p.x < 20 - radius - 1e-3f) continue;
    const glm::vec2 fromCentre{p.x - (20 - radius), p.z - (10 - radius)};
    EXPECT_NEAR(glm::length(fromCentre), radius, 1e-3f);
    EXPECT_NEAR(n.x, fromCentre.x / radius, 1e-3f);
    EXPECT_NEAR(n.z, fromCentre.y / radius, 1e-3f);
    turns.push_back(std::atan2(n.z, n.x));
  }
  std::sort(turns.begin(), turns.end());
  turns.erase(std::unique(turns.begin(), turns.end(),
                          [](float a, float b) { return b - a < 1e-4f; }),
              turns.end());
  ASSERT_EQ(turns.size(), (size_t)steps + 1);
  EXPECT_NEAR(turns.front(), 0.0f, 1e-4f);
  EXPECT_NEAR(turns.back(), (float)M_PI / 2, 1e-4f);
  for (size_t k = 1; k < turns.size(); ++k)
    EXPECT_NEAR(turns[k] - turns[k - 1], (float)M_PI / 2 / steps, 1e-3f);

  // The front cap is the outline inset by the radius.
  float capReach = 0;
  for (size_t i = 0; i < rounded.positions.size(); ++i)
    if (rounded.normals[i].z > 0.999f)
      capReach = std::max(capReach, rounded.positions[i].x);
  EXPECT_NEAR(capReach, 20 - radius, 1e-3f);
}

// The edge goes round a hole as it goes round the outline: the hole's rim
// faces into the hole and up, and the solid still faces outward.
TEST(Solids, ExtrudeRoundsAHoleAndABevelIsOneFlatChamfer) {
  SkPathBuilder ring;
  ring.addCircle(0, 0, 80);
  ring.addCircle(0, 0, 40, SkPathDirection::kCCW);
  const Mesh rounded =
      extrude(path::fromSk(ring.detach()),
              {.depth = 10, .edge = ExtrudeEdge::Round, .edgeSize = 2});
  int holeRim = 0;
  for (size_t i = 0; i < rounded.positions.size(); ++i) {
    const glm::vec3 p = rounded.positions[i];
    const glm::vec3 n = rounded.normals[i];
    const float radial = glm::length(glm::vec2{p.x, p.y});
    if (radial > 60 || n.z < 0.2f || n.z > 0.95f) continue;
    ++holeRim;
    EXPECT_LT(glm::dot(glm::vec2{n.x, n.y}, glm::vec2{p.x, p.y}), 0.0f);
  }
  EXPECT_GT(holeRim, 0);

  const Mesh bevelled =
      extrude(path::fromSk(square(40)),
              {.depth = 20, .edge = ExtrudeEdge::Bevel, .edgeSize = 4});
  EXPECT_TRUE(normalsAreUnit(bevelled));
  EXPECT_EQ(facingWrongly(bevelled, {0, 0, 0}), 0);
  int chamfer = 0;
  for (const glm::vec3& n : bevelled.normals) {
    if (std::abs(n.z) < 1e-3f || std::abs(n.z) > 0.999f) continue;
    ++chamfer;
    EXPECT_NEAR(std::abs(n.z), 0.70710678f, 1e-4f);
  }
  // Four segments, one chamfer quad at each end of each.
  EXPECT_EQ(chamfer, 4 * 2 * 4);
}

// A page projected down the walls: every wall vertex reads the page where
// the outline crosses it, so the right wall reads the page's right edge
// and the top wall its top edge, where the walls' own coordinates run the
// whole width of the page along each wall.
TEST(Solids, ExtrudedWallsCanWearTheCapsPageProjectedDown) {
  const auto page = [](glm::vec2 uv) { return uv; };
  const Mesh projected =
      extrude(path::fromSk(square(40)),
              {.depth = 20, .wallTexture = WallTexture::Projected});
  const Mesh along = extrude(path::fromSk(square(40)), {.depth = 20});
  int walls = 0;
  for (size_t i = 0; i < projected.positions.size(); ++i) {
    const glm::vec3 n = projected.normals[i];
    if (n.x > 0.999f) {
      EXPECT_NEAR(page(projected.uvs[i]).x, 1.0f, 1e-5f);
      ++walls;
    }
    if (n.y > 0.999f) EXPECT_NEAR(page(projected.uvs[i]).y, 0.0f, 1e-5f);
  }
  EXPECT_EQ(walls, 4);
  float least = 1, most = 0;
  for (size_t i = 0; i < along.positions.size(); ++i)
    if (along.normals[i].x > 0.999f) {
      least = std::min(least, along.uvs[i].x);
      most = std::max(most, along.uvs[i].x);
    }
  EXPECT_GT(most - least, 0.1f);
}

// Two face sets: the caps are slot 0 and the walls with their edges slot
// 1, in the lane a body filled per slot reads; left alone, no lane.
TEST(Solids, AnExtrudedSolidCarriesItsCapsAndWallsAsTwoFaceSets) {
  EXPECT_EQ(
      extrude(path::fromSk(square(40)), {.depth = 20}).primitiveIf("Material"),
      nullptr);
  const Mesh solid =
      extrude(path::fromSk(square(40)), {.depth = 20,
                                         .edge = ExtrudeEdge::Round,
                                         .edgeSize = 3,
                                         .faceSets = true});
  const std::vector<glm::vec4>* sets = solid.primitiveIf("Material");
  ASSERT_NE(sets, nullptr);
  ASSERT_EQ(sets->size(), solid.triangleCount());
  int caps = 0, sides = 0;
  for (size_t t = 0; t < solid.triangleCount(); ++t) {
    const bool flat =
        std::abs(solid.normals[solid.indices[t * 3]].z) > 0.999f &&
        std::abs(solid.normals[solid.indices[t * 3 + 1]].z) > 0.999f &&
        std::abs(solid.normals[solid.indices[t * 3 + 2]].z) > 0.999f;
    const float slot = (*sets)[t].x;
    if (slot == 0) {
      ++caps;
      EXPECT_TRUE(flat);
    } else {
      ++sides;
      EXPECT_EQ(slot, 1.0f);
    }
  }
  EXPECT_GT(caps, 0);
  EXPECT_GT(sides, 0);
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

TEST(Solids, ALatheFacesOutWhicheverWayItsProfileRuns) {
  // A closed can: out along the floor, up the wall, in across the lid.
  // Anticlockwise in (radius, height) one way, clockwise the other.
  std::vector<glm::vec2> profile = {{0, 0}, {10, 0}, {10, 20}, {0, 20}};
  const glm::vec3 centre{0, 10, 0};
  const Mesh anticlockwise = revolve(profile, {.segments = 24});
  EXPECT_EQ(facingWrongly(anticlockwise, centre), 0);
  std::reverse(profile.begin(), profile.end());
  const Mesh clockwise = revolve(profile, {.segments = 24});
  EXPECT_EQ(facingWrongly(clockwise, centre), 0);
  EXPECT_TRUE(normalsAreUnit(clockwise));

  // Stated inward, it faces into the solid instead.
  const Mesh inward = revolve(profile, {.segments = 24, .inward = true});
  EXPECT_EQ(facingWrongly(inward, centre, false), 0);
}

TEST(Solids, ALoftFacesOutWhicheverWayItsRingsWindAndRun) {
  const glm::vec3 centre{0, 0, 5};
  for (const bool clockwise : {false, true})
    for (const bool backToFront : {false, true})
      for (const bool capEnds : {false, true}) {
        std::vector<std::vector<glm::vec3>> sections = {
            squareRing(10, 0, clockwise), squareRing(10, 10, clockwise)};
        if (!backToFront) std::reverse(sections.begin(), sections.end());
        const Mesh skin = loft(sections, {.capEnds = capEnds});
        ASSERT_GT(skin.triangleCount(), 0u);
        EXPECT_EQ(facingWrongly(skin, centre), 0)
            << "clockwise " << clockwise << " back to front " << backToFront
            << " capped " << capEnds;
        const Mesh inward =
            loft(sections, {.capEnds = capEnds, .inward = true});
        EXPECT_EQ(facingWrongly(inward, centre, false), 0);
      }
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

  const Mesh shaded =
      box({0, 0, 0}, {1, 1, 1},
          {.tint = {1.0f, 0.5f, 0.25f, 1.0f}, .sideShade = 0.5f});
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

TEST(Solids, EveryRegularSolidCarriesItsOwnFacesAndClosesOnItself) {
  struct Expected {
    Platonic solid;
    size_t faces, corners, sides;  ///< sides per face
    const char* name;
  };
  const Expected kSolids[] = {
      {Platonic::Tetrahedron, 4, 4, 3, "tetrahedron"},
      {Platonic::Cube, 6, 8, 4, "cube"},
      {Platonic::Octahedron, 8, 6, 3, "octahedron"},
      {Platonic::Dodecahedron, 12, 20, 5, "dodecahedron"},
      {Platonic::Icosahedron, 20, 12, 3, "icosahedron"}};

  for (const Expected& want : kSolids) {
    SCOPED_TRACE(want.name);
    const Mesh cage =
        platonic(want.solid, {.circumradius = 3.0f, .sharedVertices = true});
    EXPECT_EQ(faceCount(cage), want.faces);
    EXPECT_EQ(cage.vertexCount(), want.corners);
    // A polygon face is fanned, so its triangles are two fewer than its
    // sides — the count that separates twelve pentagons from thirty-six
    // loose triangles.
    EXPECT_EQ(cage.triangleCount(), want.faces * (want.sides - 2));

    // Euler: V - E + F = 2 for any solid that closes on itself. An edge
    // is where two faces meet, so the seam a fan leaves across a pentagon
    // is not one and does not count.
    const std::vector<Edge> rim = edges(cage);
    EXPECT_EQ(cage.vertexCount() - rim.size() + want.faces, 2u);
    for (const Edge& e : rim) EXPECT_NE(e.opposite, kNoFace);

    // Regular means regular: every corner on one sphere, every face at
    // one distance, every edge one length.
    for (const glm::vec3& p : cage.positions)
      EXPECT_NEAR(glm::length(p), 3.0f, 1e-4f);
    const float inradius = glm::length(faceCentroid(cage, 0));
    const float edgeLength =
        glm::length(cage.positions[rim[0].from] - cage.positions[rim[0].to]);
    for (const Edge& e : rim)
      EXPECT_NEAR(glm::length(cage.positions[e.from] - cage.positions[e.to]),
                  edgeLength, 1e-3f);

    for (size_t f = 0; f < want.faces; ++f) {
      EXPECT_NEAR(glm::length(faceCentroid(cage, f)), inradius, 1e-3f);
      // Wound outward: the face looks the way it stands from the centre.
      EXPECT_GT(glm::dot(faceNormal(cage, f), faceCentroid(cage, f)), 0.0f);
    }

    // The hard-cornered form is the same solid with every face's corners
    // its own, so it carries a UV square per face and no shared normal.
    const Mesh hard = platonic(want.solid, {.circumradius = 3.0f});
    EXPECT_EQ(faceCount(hard), want.faces);
    EXPECT_EQ(hard.vertexCount(), want.faces * want.sides);
    EXPECT_EQ(hard.uvs.size(), hard.positions.size());
    EXPECT_TRUE(normalsAreUnit(hard));
    // Same solid, so the two forms look the same ways — the order the
    // faces are emitted in is each form's own.
    for (size_t f = 0; f < want.faces; ++f) {
      bool matched = false;
      for (size_t g = 0; g < want.faces; ++g)
        matched = matched || glm::length(faceNormal(hard, f) -
                                         faceNormal(cage, g)) < 1e-3f;
      EXPECT_TRUE(matched);
    }
  }
}

TEST(Solids, TheHardCorneredCubeIsTheBoxItself) {
  // Nothing re-derives what box() already is: the cube's corners stand at
  // the circumradius, which is the box's half-diagonal.
  const Mesh cube = platonic(Platonic::Cube, {.circumradius = std::sqrt(3.0f)});
  const Mesh same = box({-1, -1, -1}, {1, 1, 1});
  EXPECT_EQ(cube.positions, same.positions);
  EXPECT_EQ(cube.normals, same.normals);
  EXPECT_EQ(cube.uvs, same.uvs);
  EXPECT_EQ(cube.indices, same.indices);
}

TEST(Solids, ChosenFaceUpTurnsThatFaceOntoTheAxisAndTheOneAcrossAway) {
  const Mesh die = platonic(Platonic::Icosahedron,
                            {.circumradius = 1.0f, .sharedVertices = true});
  for (size_t face = 0; face < faceCount(die); ++face) {
    SCOPED_TRACE(face);
    Mesh settled = die;
    settled.transform(faceUp(die, face));
    const glm::vec3 up = faceNormal(settled, face);
    EXPECT_NEAR(up.y, 1.0f, 1e-4f);

    // An icosahedron's faces pair off, and the pair is what a die's
    // numbering stands on: the face across from the one standing up is
    // the one standing down.
    const std::optional<size_t> across = opposedFace(die, face);
    ASSERT_TRUE(across.has_value());
    EXPECT_NEAR(faceNormal(settled, *across).y, -1.0f, 1e-4f);
  }
}
