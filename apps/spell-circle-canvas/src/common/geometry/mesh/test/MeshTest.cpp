/** @file
 * The mesh currency: the parametric sheet produces coherent lanes,
 * transform and append keep every lane sized to the vertices, and a
 * primitive colour lane bakes into unwelded vertices.
 */

#include <gtest/gtest.h>
#include <include/core/SkPathBuilder.h>

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

#include "sigilgeometry/mesh/Mesh.h"
#include "support/GeometrySupport.h"

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;

using sigil::geometry::test::splitQuad;

namespace {

}  // namespace

TEST(Mesh, GridUvAndIndicesCoherent) {
  Mesh m = mesh::grid(
      4, 3, [](float u, float v) -> glm::vec3 { return {u * 10, v * 10, 0}; });
  EXPECT_EQ(m.vertexCount(), 12u);
  EXPECT_EQ(m.triangleCount(), 12u);  // 3x2 cells * 2
  // UV origin is TOP-left, matching how images are addressed, while the
  // generator's v parameter runs bottom-up across the sheet. The two are
  // therefore opposite: v = 0 in the generator lands at uv.y = 1. Get this
  // backwards and every texture on a generated surface is upside down.
  EXPECT_EQ(m.uvs.front().x, 0.0f);
  EXPECT_EQ(m.uvs.front().y, 1.0f);
  EXPECT_EQ(m.uvs.back().y, 0.0f);
  for (uint32_t i : m.indices) EXPECT_LT(i, m.vertexCount());
}

TEST(Mesh, TransformMovesBoundsAndKeepsUnitNormals) {
  Mesh m = mesh::quad(10, 10);
  m.transform(glm::translate(glm::mat4(1.0f), {5, 0, 0}));
  glm::vec3 lo, hi;
  m.bounds(&lo, &hi);
  EXPECT_NEAR((lo.x + hi.x) * 0.5f, 5.0f, 1e-4f);
  for (const glm::vec3& n : m.normals) EXPECT_NEAR(glm::length(n), 1.0f, 1e-4f);
}

TEST(Mesh, TransformRotatesNormalsForward) {
  // Normals transform by the inverse transpose of the model matrix, and it
  // must be applied in row form. Dotting the columns instead is the plain
  // inverse, which rotates normals BACKWARDS — a +x normal under a +90
  // degree turn about z would come out at {0,-1,0} instead of {0,1,0}.
  Mesh m = mesh::quad(10, 10);
  m.normals.assign(m.vertexCount(), {1, 0, 0});
  m.transform(glm::rotate(glm::mat4(1.0f), (float)M_PI * 0.5f, {0, 0, 1}));
  for (const glm::vec3& n : m.normals) {
    EXPECT_NEAR(n.x, 0.0f, 1e-4f);
    EXPECT_NEAR(n.y, 1.0f, 1e-4f);
    EXPECT_NEAR(n.z, 0.0f, 1e-4f);
  }
  // Non-uniform scale keeps inverse-transpose semantics: a normal
  // along the scaled axis renormalizes back to itself.
  Mesh s = mesh::quad(10, 10);
  s.normals.assign(s.vertexCount(), {1, 0, 0});
  s.transform(glm::scale(glm::mat4(1.0f), {2, 1, 1}));
  for (const glm::vec3& n : s.normals) {
    EXPECT_NEAR(n.x, 1.0f, 1e-4f);
    EXPECT_NEAR(n.y, 0.0f, 1e-4f);
    EXPECT_NEAR(n.z, 0.0f, 1e-4f);
  }
}

TEST(Mesh, AppendRepairsShortIncomingLanes) {
  // A lane can also arrive SHORTER than its own mesh's element count —
  // hand-built meshes and imported files do this, e.g. a PLY whose extra
  // property list runs out early. Append repairs both sides, because a
  // plain insert would leave the MERGED lane undersized, and every consumer
  // reads "lane sized to positions" (or to triangleCount, for prim lanes)
  // as the presence bit for the whole mesh: one short lane on one side
  // would switch tinting, lighting or texturing off for the merged result.
  Mesh a;
  a.positions = {{-50, -50, 0}, {50, -50, 0}, {50, 50, 0}, {-50, 50, 0}};
  a.indices = {0, 1, 2, 0, 2, 3};
  a.colors.assign(4, glm::vec4{1, 0, 0, 1});
  a.normals.assign(4, glm::vec3{0, 0, 1});
  a.uvs.assign(4, glm::vec2{0.25f, 0.5f});

  Mesh b;  // 4 vertices / 2 triangles, but every lane one entry short
  b.positions = {{0, 0, 10}, {10, 0, 10}, {10, 10, 10}, {0, 10, 10}};
  b.indices = {0, 1, 2, 0, 2, 3};
  b.colors = {{0, 1, 0, 1}, {0, 1, 0, 1}, {0, 1, 0, 1}};
  b.normals = {{1, 0, 0}, {1, 0, 0}, {1, 0, 0}};
  b.uvs = {{1, 1}, {1, 1}, {1, 1}};
  b.prims["heat"] = {{5, 0, 0, 0}};  // 1 entry for 2 triangles

  a.append(b);
  ASSERT_EQ(a.positions.size(), 8u);
  EXPECT_EQ(a.colors.size(), a.positions.size()) << "short colors lane";
  EXPECT_EQ(a.normals.size(), a.positions.size());
  EXPECT_EQ(a.uvs.size(), a.positions.size());
  ASSERT_EQ(a.colors.size(), 8u);
  // Ours survived, theirs landed at the right run, and the hole pads
  // WHITE, which is the identity for a colour lane that multiplies into
  // the base colour. Padding black would darken the merged half instead.
  EXPECT_EQ(a.colors[0], (glm::vec4{1, 0, 0, 1}));
  EXPECT_EQ(a.colors[4], (glm::vec4{0, 1, 0, 1}));
  EXPECT_EQ(a.colors[7], (glm::vec4{1, 1, 1, 1})) << "pad stays white";
  EXPECT_NEAR(a.normals[7].z, 1.0f, 1e-6f);  // +Z, never a zero normal
  EXPECT_NEAR(a.uvs[7].x, 0.0f, 1e-6f);
  EXPECT_NEAR(a.uvs[7].y, 0.0f, 1e-6f);
  // Prim lanes are counted per TRIANGLE rather than per vertex, and are
  // padded the same way, by the convention their lane name implies.
  ASSERT_EQ(a.triangleCount(), 4u);
  const std::vector<glm::vec4>* heat = a.primIf("heat");
  ASSERT_TRUE(heat);
  ASSERT_EQ(heat->size(), 4u) << "short incoming prim lane";
  EXPECT_FLOAT_EQ((*heat)[2].x, 5.0f);  // theirs
  EXPECT_FLOAT_EQ((*heat)[3].x, 0.0f);  // padded by name

  // Mirror case: the short lane belongs to the receiving mesh. It is padded
  // up to the old element count first, so the incoming values still land at
  // the offset the merged mesh expects.
  Mesh shortSide;
  shortSide.positions = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}};
  shortSide.indices = {0, 1, 2};
  shortSide.colors = {{0, 0, 1, 1}};  // 1 entry for 3 vertices
  Mesh full;
  full.positions = {{5, 0, 0}, {6, 0, 0}, {6, 1, 0}};
  full.indices = {0, 1, 2};
  full.colors.assign(3, glm::vec4{1, 1, 0, 1});
  shortSide.append(full);
  ASSERT_EQ(shortSide.colors.size(), 6u);
  EXPECT_EQ(shortSide.colors[1], (glm::vec4{1, 1, 1, 1}));
  EXPECT_EQ(shortSide.colors[3], (glm::vec4{1, 1, 0, 1}));
}

TEST(Mesh, PrimLanesSizeToTrianglesAndAppendPadsByName) {
  Mesh a = splitQuad();
  EXPECT_EQ(a.triangleCount(), 2u);
  EXPECT_EQ(a.primIf("Color"), nullptr) << "absent until touched";
  a.prim("Color")[0] = {1, 0, 0, 1};
  ASSERT_TRUE(a.primIf("Color"));
  ASSERT_EQ(a.primIf("Color")->size(), 2u) << "one float4 per triangle";
  EXPECT_EQ((*a.primIf("Color"))[1], (glm::vec4{1, 1, 1, 1}))
      << "\"Color\" creates white";
  a.prim("heat", {0, 0, 0, 0})[1] = {9, 0, 0, 0};

  // Appending a mesh with no prim lanes still pads ours, by the same name
  // convention, so the lane stays sized to triangleCount().
  Mesh b = splitQuad();
  a.append(b);
  ASSERT_EQ(a.triangleCount(), 4u);
  ASSERT_EQ(a.primIf("Color")->size(), 4u);
  EXPECT_EQ((*a.primIf("Color"))[2], (glm::vec4{1, 1, 1, 1}));
  EXPECT_EQ((*a.primIf("heat"))[3], (glm::vec4{0, 0, 0, 0}));

  // ...and in the other direction, a lane only THEY have is padded back
  // over our existing triangles so their values still start at our old
  // triangle count.
  Mesh c = splitQuad();
  c.prim("heat", {0, 0, 0, 0})[0] = {5, 0, 0, 0};
  a.append(c);
  ASSERT_EQ(a.triangleCount(), 6u);
  const std::vector<glm::vec4>* heat = a.primIf("heat");
  ASSERT_EQ(heat->size(), 6u);
  EXPECT_FLOAT_EQ((*heat)[1].x, 9.0f);  // ours survived
  EXPECT_FLOAT_EQ((*heat)[2].x, 0.0f);  // padded
  EXPECT_FLOAT_EQ((*heat)[4].x, 5.0f);  // theirs landed at the right run
  EXPECT_EQ(a.primIf("Color")->size(), 6u);
}

TEST(Mesh, BakePrimColorUnweldsFlatColoursIntoVertices) {
  Mesh m = splitQuad();
  m.colors.assign(4, glm::vec4{1, 1, 1, 0.5f});
  m.prim("Color")[0] = {1, 0, 0, 1};
  m.prim("Color")[1] = {0, 0, 1, 1};
  const Mesh baked = mesh::bakePrimColor(m, "Color");
  // A flat per-face colour cannot be expressed on shared vertices, so the
  // bake unwelds: every triangle gets its own three vertices and the
  // indices are renumbered to match.
  ASSERT_EQ(baked.vertexCount(), 6u);
  ASSERT_EQ(baked.triangleCount(), 2u);
  ASSERT_EQ(baked.colors.size(), 6u);
  for (size_t k = 0; k < 3; ++k) {
    EXPECT_FLOAT_EQ(baked.colors[k].r, 1.0f);
    EXPECT_FLOAT_EQ(baked.colors[k].b, 0.0f);
    EXPECT_FLOAT_EQ(baked.colors[3 + k].b, 1.0f);
    EXPECT_FLOAT_EQ(baked.colors[3 + k].r, 0.0f);
    // The face colour MULTIPLIES into whatever vertex colour was already
    // there rather than replacing it, so the original alpha survives.
    EXPECT_FLOAT_EQ(baked.colors[k].a, 0.5f);
  }
  EXPECT_EQ(baked.positions[3], m.positions[0]) << "triangle order kept";
  EXPECT_TRUE(baked.primIf("Color")) << "lanes survive the unweld";
  // Naming a lane that does not exist leaves the mesh alone — still welded,
  // still valid — rather than unwelding it or clearing its colours.
  EXPECT_EQ(mesh::bakePrimColor(m, "absent").vertexCount(), 4u);
}

// viewProjection() already folds the viewport mapping in, so its output
// divided by w is in PIXELS, not in normalized device coordinates: the world
