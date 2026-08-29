#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

#include "sigilgeometry/Materials.h"
#include "sigilgeometry/Mesh.h"
#include "sigilgeometry/Points.h"
#include "sigilgeometry/Space.h"
#include "support/GeometrySupport.h"

using namespace sigil::geometry;

using sigil::geometry::test::splitQuad;

namespace {

SkPath rect(float x, float y, float w, float h) {
  return SkPath::Rect(SkRect::MakeXYWH(x, y, w, h));
}

}  // namespace

TEST(Mesh, ExtrudeRectMakesABox) {
  Mesh m = mesh::extrude(rect(0, 0, 100, 60), {.depth = 20});
  ASSERT_GT(m.vertexCount(), 0u);
  ASSERT_EQ(m.normals.size(), m.vertexCount());
  ASSERT_EQ(m.uvs.size(), m.vertexCount());
  glm::vec3 lo, hi;
  m.bounds(&lo, &hi);
  EXPECT_NEAR(hi.x - lo.x, 100.0f, 1e-3f);
  EXPECT_NEAR(hi.y - lo.y, 60.0f, 1e-3f);
  EXPECT_NEAR(hi.z - lo.z, 20.0f, 1e-3f);
  // 2 caps (2 tris each) + 4 walls (2 tris each) = 12 triangles.
  EXPECT_EQ(m.triangleCount(), 12u);
  // All normals unit length.
  for (const glm::vec3& n : m.normals) EXPECT_NEAR(glm::length(n), 1.0f, 1e-4f);
}

TEST(Mesh, ExtrudeAnnulusKeepsHole) {
  SkPathBuilder ring;
  ring.addCircle(0, 0, 80);
  ring.addCircle(0, 0, 40, SkPathDirection::kCCW);
  Mesh m = mesh::extrude(ring.detach(), {.depth = 10});
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

TEST(Mesh, TorusNormalsPointOutward) {
  Mesh m = mesh::torus(100, 30, 32, 16);
  // Parameterization convention: the first vertex is u = v = 0, which is on
  // the outer equator facing +x, so its normal points outward along +x. An
  // inward-facing generator would put the normal at -x and light the torus
  // inside out.
  const glm::vec3 n0 = m.normals.front();
  EXPECT_GT(std::abs(n0.x), 0.7f);
  for (const glm::vec3& n : m.normals) EXPECT_NEAR(glm::length(n), 1.0f, 1e-3f);
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

TEST(Mesh, AppendKeepsNormalAndUvLanesSizedToPositions) {
  // Invariant across append: an optional vertex lane ends up either absent
  // on both sides or sized to positions on the merged mesh. Consumers read
  // "lane size == positions size" as the presence bit for the whole mesh —
  // space::drawMesh takes exactly that comparison as its hasNormals test —
  // so a merge that left the lane short would turn lighting off for every
  // vertex, not only for the ones that arrived without normals.
  //
  // The mismatch is easy to author by accident: points::instance copies
  // only the lanes the stamp actually has, so a stamp of bare positions and
  // indices instances into a mesh with no normals and no uvs at all.
  Mesh stamp;  // a bare triangle: no normals, no uvs
  stamp.positions = {{-2, -2, 0}, {2, -2, 0}, {0, 2, 0}};
  stamp.indices = {0, 1, 2};
  const Cloud cloud = points::ring({0, 0, 0}, 50, 4);
  const Mesh flakes = points::instance(cloud, stamp);
  ASSERT_TRUE(flakes.normals.empty()) << "the real source of the defect";
  ASSERT_TRUE(flakes.uvs.empty());

  Mesh lit = mesh::torus(40, 8, 12, 8);
  ASSERT_EQ(lit.normals.size(), lit.positions.size());
  ASSERT_EQ(lit.uvs.size(), lit.positions.size());
  const size_t litVerts = lit.positions.size();
  const glm::vec3 keptNormal = lit.normals.front();
  const glm::vec2 keptUv = lit.uvs.front();

  lit.append(flakes);
  EXPECT_EQ(lit.normals.size(), lit.positions.size()) << "hasNormals";
  EXPECT_EQ(lit.uvs.size(), lit.positions.size());
  EXPECT_NEAR(glm::length(lit.normals.front() - keptNormal), 0.0f, 1e-6f);
  EXPECT_NEAR(glm::length(lit.uvs.front() - keptUv), 0.0f, 1e-6f);
  // The padded half carries a UNIT +Z normal, never a degenerate zero:
  // zero shades black and never recovers through Mesh::transform.
  for (size_t i = litVerts; i < lit.normals.size(); ++i) {
    EXPECT_NEAR(glm::length(lit.normals[i]), 1.0f, 1e-6f) << "vertex " << i;
    EXPECT_NEAR(lit.normals[i].z, 1.0f, 1e-6f);
    EXPECT_NEAR(lit.uvs[i].x, 0.0f, 1e-6f);
    EXPECT_NEAR(lit.uvs[i].y, 0.0f, 1e-6f);
  }

  // Same in the other order — the pad has to land at the FRONT.
  Mesh reversed = points::instance(cloud, stamp);
  const size_t flakeVerts = reversed.positions.size();
  reversed.append(mesh::torus(40, 8, 12, 8));
  ASSERT_EQ(reversed.normals.size(), reversed.positions.size());
  ASSERT_EQ(reversed.uvs.size(), reversed.positions.size());
  for (size_t i = 0; i < flakeVerts; ++i)
    EXPECT_NEAR(glm::length(reversed.normals[i]), 1.0f, 1e-6f);

  // Neither side authoring a lane still means NO lane: append pads an
  // existing lane, it does not conjure one onto a flat merge.
  Mesh bare = points::instance(cloud, stamp);
  bare.append(points::instance(cloud, stamp));
  EXPECT_TRUE(bare.normals.empty());
  EXPECT_TRUE(bare.uvs.empty());
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
// origin lands on the middle pixel of an 800x600 canvas rather than on 0,0.
TEST(Space, CameraProjectsCenterToViewportCenter) {
  space::Camera camera;
  camera.eye = {0, 0, 100};
  camera.target = {0, 0, 0};
  const glm::mat4 vp = camera.viewProjection({800, 600});
  const glm::vec4 out = vp * glm::vec4{0, 0, 0, 1};
  EXPECT_NEAR(out.x / out.w, 400.0f, 1e-2f);
  EXPECT_NEAR(out.y / out.w, 300.0f, 1e-2f);
}

TEST(Space, FaceCameraPointsTheQuadNormalAtTheEye) {
  // faceCamera(eye, at) is the billboard transform: it anchors at @p at and
  // turns mesh::quad's +z face toward the eye, wherever the eye stands. The
  // eye list deliberately includes the cases that break a naive
  // cross-product basis — an eye almost on top of the anchor, and eyes
  // directly above and below it, where the view direction is parallel to
  // the world up vector and the side vector degenerates.
  const glm::vec3 at = {40, -25, 60};
  const glm::vec3 eyes[] = {
      {0, 200, 1150},                    // an ordinary camera, well in front
      {-820, 260, -320}, {40, -25, 61},  // almost on top of the panel
      {40, 900, 60},   // directly ABOVE: dir ≈ +up, degenerate side
      {40, -900, 60},  // directly below: dir ≈ -up
  };
  for (const glm::vec3& eye : eyes) {
    const glm::mat4 m = space::faceCamera(eye, at);
    // Translation is the anchor.
    EXPECT_NEAR(m[3][0], at.x, 1e-5f);
    EXPECT_NEAR(m[3][1], at.y, 1e-5f);
    EXPECT_NEAR(m[3][2], at.z, 1e-5f);
    // The quad's +z normal lands on the unit eye direction.
    const glm::vec3 n = glm::mat3(m) * glm::vec3{0, 0, 1};
    const glm::vec3 want = glm::normalize(eye - at);
    EXPECT_NEAR(glm::dot(n, want), 1.0f, 1e-5f)
        << "eye " << eye.x << "," << eye.y << "," << eye.z;
    // And the basis stays orthonormal (no shear, no scale).
    const glm::vec3 bx = glm::mat3(m) * glm::vec3{1, 0, 0};
    const glm::vec3 by = glm::mat3(m) * glm::vec3{0, 1, 0};
    EXPECT_NEAR(glm::length(bx), 1.0f, 1e-5f);
    EXPECT_NEAR(glm::length(by), 1.0f, 1e-5f);
    EXPECT_NEAR(glm::dot(bx, by), 0.0f, 1e-5f);
    EXPECT_NEAR(glm::dot(bx, n), 0.0f, 1e-5f);
  }

  // Control: an untransformed quad's normal does NOT already point at an
  // off-axis eye, so the assertion above is capable of failing.
  const glm::vec3 offAxis = glm::normalize(eyes[1] - at);
  EXPECT_LT(glm::dot(glm::vec3{0, 0, 1}, offAxis), 0.99f);

  // The two ways of orienting a quad must agree: a one-point cloud whose
  // "facing" lane holds the eye direction, stamped through points::quads(),
  // produces the same vertices as transforming a quad by faceCamera. If they
  // drift apart, a scene mixing billboards with instanced panels shows two
  // different orientations for the same direction.
  const glm::vec3 eye = eyes[0];
  Cloud one;
  one.positions = {at};
  one.vector("facing") = {glm::normalize(eye - at)};
  points::InstanceOptions options;
  options.orientLane = "facing";
  const Mesh stamped = points::quads(one, 170, 112, options);
  const Mesh quad = mesh::quad(170, 112);
  const glm::mat4 m = space::faceCamera(eye, at);
  ASSERT_EQ(stamped.positions.size(), quad.positions.size());
  for (size_t i = 0; i < quad.positions.size(); ++i) {
    const glm::vec3 viaMatrix = glm::vec3(m * glm::vec4(quad.positions[i], 1));
    EXPECT_NEAR(glm::length(viaMatrix - stamped.positions[i]), 0.0f, 1e-4f)
        << "vertex " << i;
  }
}

TEST(Space, DrawMeshCoversPixels) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 150));
  surface->getCanvas()->clear(SK_ColorBLACK);
  space::Camera camera;
  camera.eye = {0, 0, 300};
  space::MeshStyle style;
  style.baseColor = {1, 0, 0, 1};
  space::drawMesh(*surface->getCanvas(), sigil::geometry::mesh::quad(100, 100),
                  glm::mat4(1.0f), camera, {200, 150}, style);
  SkBitmap bm;
  bm.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(bm.pixmap(), 0, 0));
  // A smoke check that the whole painter pipeline reaches pixels: transform,
  // lighting and SkVertices batching all have to work for the middle of a
  // face-on quad to come out lit rather than the cleared black.
  const SkColor c = bm.getColor(100, 75);
  EXPECT_GT(SkColorGetR(c), 40u);
}

TEST(Space, NormalsModeEncodesDeviceSpaceYDown) {
  // The Normals G-buffer is DEVICE-space, +y down (the Materials.h
  // convention): rgb = (n.x, -n.y, n.z) * 0.5 + 0.5.
  space::Camera camera;
  camera.eye = {0, 0, 300};
  space::MeshStyle style;
  style.mode = space::MeshStyle::Mode::Normals;

  // Face-on quad: its +z normal encodes as (128, 128, 255).
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 150));
  surface->getCanvas()->clear(SK_ColorBLACK);
  space::drawMesh(*surface->getCanvas(), sigil::geometry::mesh::quad(100, 100),
                  glm::mat4(1.0f), camera, {200, 150}, style);
  SkBitmap bm;
  bm.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(bm.pixmap(), 0, 0));
  const SkColor faceOn = bm.getColor(100, 75);
  EXPECT_NEAR(SkColorGetR(faceOn), 128, 2);
  EXPECT_NEAR(SkColorGetG(faceOn), 128, 2);
  EXPECT_NEAR(SkColorGetB(faceOn), 255, 2);

  // Tilt the quad so its normal points toward world +y (screen-up).
  // Under +y down that encodes BELOW mid-grey green; a view-space
  // no-flip buffer would put it above.
  sk_sp<SkSurface> tilted =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 150));
  tilted->getCanvas()->clear(SK_ColorBLACK);
  const glm::mat4 model =
      glm::rotate(glm::mat4(1.0f), glm::radians(-45.0f), glm::vec3{1, 0, 0});
  space::drawMesh(*tilted->getCanvas(), sigil::geometry::mesh::quad(100, 100),
                  model, camera, {200, 150}, style);
  SkBitmap tiltedBm;
  tiltedBm.allocPixels(tilted->imageInfo());
  ASSERT_TRUE(tilted->readPixels(tiltedBm.pixmap(), 0, 0));
  EXPECT_LT(SkColorGetG(tiltedBm.getColor(100, 75)), 128u);
}

TEST(Space, PrimColorLaneTintsTrianglesFlat) {
  Mesh m = splitQuad();
  m.prim("Color")[0] = {1, 0, 0, 1};  // lower-right half
  m.prim("Color")[1] = {0, 0, 1, 1};  // upper-left half

  space::Camera camera;
  camera.eye = {0, 0, 300};
  space::MeshStyle style;
  style.baseColor = {1, 1, 1, 1};

  const auto render = [&](const space::MeshStyle& s) {
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 150));
    surface->getCanvas()->clear(SK_ColorBLACK);
    space::drawMesh(*surface->getCanvas(), m, glm::mat4(1.0f), camera,
                    {200, 150}, s);
    SkBitmap bm;
    bm.allocPixels(surface->imageInfo());
    EXPECT_TRUE(surface->readPixels(bm.pixmap(), 0, 0));
    return bm;
  };

  // Control: with no lane named the two halves are the same colour, so the
  // difference measured below can only come from the lane.
  const SkBitmap plain = render(style);
  EXPECT_EQ(plain.getColor(120, 95), plain.getColor(79, 54));

  style.primColorLane = "Color";
  const SkBitmap tinted = render(style);
  const SkColor lowerRight = tinted.getColor(120, 95);
  const SkColor upperLeft = tinted.getColor(79, 54);
  EXPECT_GT(SkColorGetR(lowerRight), SkColorGetB(lowerRight) + 40u);
  EXPECT_GT(SkColorGetB(upperLeft), SkColorGetR(upperLeft) + 40u);

  // The Normals mode writes a G-buffer, whose pixels are data to be decoded
  // by a later shading pass, not a picture. A colour tint applied there
  // would silently corrupt the normals it encodes, so the lane must be
  // ignored outside lit rendering.
  style.mode = space::MeshStyle::Mode::Normals;
  space::MeshStyle bare = style;
  bare.primColorLane.clear();
  EXPECT_EQ(render(style).getColor(120, 95), render(bare).getColor(120, 95));
}

TEST(Materials, EffectsCompileAndShade) {
  const materials::Environment env = materials::Environment::studio(128);
  ASSERT_TRUE(env.valid());
  const SkPath shape = SkPath::Circle(40, 40, 30);
  sk_sp<SkImage> normals =
      materials::bevelNormals(shape, SkIRect::MakeWH(80, 80), 6);
  ASSERT_TRUE(normals);
  EXPECT_TRUE(materials::gold(normals, env, {0, 0}));
  EXPECT_TRUE(materials::chrome(normals, env, {0, 0}));
  sk_sp<SkImage> backdrop;
  {
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(80, 80));
    s->getCanvas()->clear(SK_ColorCYAN);
    backdrop = s->makeImageSnapshot();
  }
  EXPECT_TRUE(materials::glass(normals, env, backdrop, {0, 0}));
}

TEST(Materials, DrawChromeShadesInsideShapeOnly) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 120));
  surface->getCanvas()->clear(SK_ColorTRANSPARENT);
  const materials::Environment env = materials::Environment::studio(128);
  materials::drawChrome(*surface->getCanvas(), SkPath::Circle(60, 60, 40), env,
                        8);
  SkBitmap bm;
  bm.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(bm.pixmap(), 0, 0));
  // The shader is clipped to the path: a material fills its shape and
  // leaves the rest of the canvas at whatever was already there. Checked on
  // alpha so it holds whatever colour the environment happens to reflect.
  EXPECT_NE(bm.getColor(60, 60) & 0xff000000, 0u);  // inside: painted
  EXPECT_EQ(bm.getColor(5, 5) & 0xff000000, 0u);    // outside: untouched
}

TEST(Materials, BevelNormalsFlatInteriorTiltedRim) {
  const SkPath shape = SkPath::Circle(50, 50, 40);
  sk_sp<SkImage> img =
      materials::bevelNormals(shape, SkIRect::MakeWH(100, 100), 10);
  ASSERT_TRUE(img);
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(100, 100));
  ASSERT_TRUE(img->readPixels(nullptr, bm.pixmap(), 0, 0));
  // Normal-map encoding: rgb = n * 0.5 + 0.5, so a flat normal pointing
  // straight out of the surface is (128, 128, 255) and the mid-grey 128 is
  // the zero of each axis. The interior of a bevel is flat.
  const SkColor center = bm.getColor(50, 50);
  EXPECT_GT(SkColorGetB(center), 240u);
  EXPECT_NEAR(SkColorGetR(center), 128, 6);
  // x runs to the right, so the LEFT rim tilts toward -x and its red
  // channel drops below the 128 zero point. A sign flip here would light
  // every bevelled shape from the wrong side.
  const SkColor rim = bm.getColor(13, 50);
  EXPECT_LT(SkColorGetR(rim), 110u);
}

TEST(Materials, EnvironmentRoughnessBlursAndCaches) {
  const materials::Environment env = materials::Environment::sunset(128);
  sk_sp<SkImage> sharp = env.image(0);
  sk_sp<SkImage> rough = env.image(0.6f);
  ASSERT_TRUE(sharp);
  ASSERT_TRUE(rough);
  EXPECT_NE(sharp.get(), rough.get());
  EXPECT_EQ(rough->width(), sharp->width());
  // Roughness is quantized into buckets and each bucket's blurred image is
  // built once and kept, so asking twice for the same roughness returns the
  // identical object rather than re-blurring the environment per draw.
  EXPECT_EQ(env.image(0.6f).get(), rough.get());
}
