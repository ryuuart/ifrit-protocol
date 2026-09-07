/** @file
 * A mesh read by its faces: the primitive lane names them, a fanned
 * polygon answers one plane, and the face-up rotation lands the face it
 * is asked about on the axis it is asked for.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

#include "sigilgeometry/mesh/Faces.h"

using namespace sigil::geometry::mesh;

namespace {

/** A square in the xz plane looking +y, fanned into two triangles that
 *  carry one face id — the smallest mesh where a face and a triangle are
 *  different things. */
Mesh lidQuad(float y = 1.0f) {
  Mesh m;
  m.positions = {{-1, y, 1}, {1, y, 1}, {1, y, -1}, {-1, y, -1}};
  m.normals.assign(4, {0, 1, 0});
  m.indices = {0, 1, 2, 0, 2, 3};
  m.prim("Id", {0, 0, 0, 0});
  return m;
}

}  // namespace

TEST(Faces, APolygonFannedIntoTrianglesIsStillOneFace) {
  const Mesh lid = lidQuad();
  EXPECT_EQ(lid.triangleCount(), 2u);
  EXPECT_EQ(faceCount(lid), 1u);

  const glm::vec3 n = faceNormal(lid, 0);
  EXPECT_NEAR(n.y, 1.0f, 1e-5f);
  // The centroid is the polygon's, not a triangle's: the corner the fan
  // repeats must not be counted twice, which would drag it off centre.
  const glm::vec3 c = faceCentroid(lid, 0);
  EXPECT_NEAR(c.x, 0.0f, 1e-5f);
  EXPECT_NEAR(c.z, 0.0f, 1e-5f);
  EXPECT_NEAR(c.y, 1.0f, 1e-5f);

  // With no lane, every triangle is its own face and the two halves of
  // the same square answer two centroids.
  Mesh loose = lid;
  loose.prims.clear();
  EXPECT_EQ(faceCount(loose), 2u);
  EXPECT_NE(faceCentroid(loose, 0), faceCentroid(loose, 1));
}

TEST(Faces, TheOpposedFaceIsFoundFromTheCentroidsAndIsAbsentWhenThereIsNone) {
  Mesh pair = lidQuad(1.0f);
  Mesh floorFace = lidQuad(-1.0f);
  for (glm::vec4& id : floorFace.prim("Id")) id.x = 1;
  pair.append(floorFace);
  ASSERT_EQ(faceCount(pair), 2u);
  EXPECT_EQ(opposedFace(pair, 0), std::optional<size_t>(1));
  EXPECT_EQ(opposedFace(pair, 1), std::optional<size_t>(0));

  // One face on its own has nothing across from it, and a face outside
  // the count is not a face at all.
  const Mesh single = lidQuad();
  EXPECT_FALSE(opposedFace(single, 0).has_value());
  EXPECT_FALSE(opposedFace(single, 7).has_value());
}

TEST(Faces, FaceUpTurnsTheChosenFaceOntoTheAxisAndNothingElse) {
  // A face looking +x, asked to look up.
  Mesh wall;
  wall.positions = {{1, -1, -1}, {1, 1, -1}, {1, 1, 1}, {1, -1, 1}};
  wall.normals.assign(4, {1, 0, 0});
  wall.indices = {0, 1, 2, 0, 2, 3};
  wall.prim("Id", {0, 0, 0, 0});

  Mesh turned = wall;
  turned.transform(faceUp(wall, 0));
  const glm::vec3 n = faceNormal(turned, 0);
  EXPECT_NEAR(n.x, 0.0f, 1e-5f);
  EXPECT_NEAR(n.y, 1.0f, 1e-5f);
  EXPECT_NEAR(n.z, 0.0f, 1e-5f);
  // The shortest rotation from +x to +y turns about +z, so every point
  // keeps the z it stood at — a face-up pose decides the face and
  // nothing else.
  for (size_t i = 0; i < turned.positions.size(); ++i)
    EXPECT_NEAR(turned.positions[i].z, wall.positions[i].z, 1e-5f);

  // Any axis, not just up: the same face square to a viewer on +z.
  Mesh facing = wall;
  facing.transform(faceUp(wall, 0, {0, 0, 1}));
  EXPECT_NEAR(faceNormal(facing, 0).z, 1.0f, 1e-5f);

  // A face already on the axis is left where it stands, and one facing
  // exactly away still lands on it.
  const Mesh lid = lidQuad();
  EXPECT_EQ(faceUp(lid, 0), glm::mat4(1.0f));
  Mesh under = lid;
  under.transform(faceUp(lid, 0, {0, -1, 0}));
  EXPECT_NEAR(faceNormal(under, 0).y, -1.0f, 1e-5f);
}

TEST(Faces, AnEdgeIsWhereTwoFacesMeetAndAFansOwnSeamIsNot) {
  // One square as two triangles: four edges round the outside, and the
  // diagonal the fan drew is inside the face rather than an edge of it.
  const std::vector<Edge> alone = edges(lidQuad());
  EXPECT_EQ(alone.size(), 4u);
  for (const Edge& e : alone) EXPECT_EQ(e.opposite, kNoFace);

  // Fold a second face onto one of those edges — standing on the lid's
  // own two corners, which is what makes the fold an edge and not two
  // borders — and that one edge has two faces while the rest have one.
  Mesh folded = lidQuad();
  folded.positions.push_back({1, 2, -2});
  folded.positions.push_back({-1, 2, -2});
  folded.normals.resize(folded.positions.size(), {0, 1, 0});
  folded.indices.insert(folded.indices.end(), {3, 2, 4, 3, 4, 5});
  folded.prim("Id", {1, 0, 0, 0});  // the two new triangles are face 1
  ASSERT_EQ(faceCount(folded), 2u);
  size_t shared = 0;
  for (const Edge& e : edges(folded))
    if (e.opposite != kNoFace) ++shared;
  EXPECT_EQ(shared, 1u);
}
