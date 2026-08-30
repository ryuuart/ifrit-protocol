/** @file
 * Point clouds: the generators write their conventional lanes, the
 * modifiers move positions, a stamp instanced at every point keeps the
 * merged mesh's lanes sized to its vertices, and an instanced facing
 * lane agrees with the camera-facing transform.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/pop/Points.h"
#include "sigilgeometry/space/Space.h"

using namespace sigil::geometry;

TEST(Points, GeneratorsWriteLanes) {
  Spline3 line;
  line.type = Spline3::Type::Linear;
  line.points = {{0, 0, 0}, {90, 0, 0}};
  Cloud onCurve = points::onSpline(line, 10);
  EXPECT_EQ(onCurve.size(), 10u);
  ASSERT_TRUE(onCurve.scalarIf("t"));
  EXPECT_NEAR(onCurve.scalarIf("t")->back(), 1, 1e-4);
  ASSERT_TRUE(onCurve.vectorIf("normal"));

  Cloud lattice = points::grid({0, 0, 0}, {90, 0, 0}, {0, 60, 0}, 4, 3);
  EXPECT_EQ(lattice.size(), 12u);
  EXPECT_NEAR(lattice.vectorIf("normal")->front().z, 1, 1e-4);

  Cloud circle = points::ring({0, 0, 0}, 50, 8);
  EXPECT_EQ(circle.size(), 8u);
  // A ring is laid out in the plane perpendicular to its axis, and the
  // default axis is +y, so an unaxised ring lives in the xz plane at y = 0.
  for (const glm::vec3& p : circle.positions) {
    EXPECT_NEAR(glm::length(p), 50, 1e-2);
    EXPECT_NEAR(p.y, 0, 1e-3);
  }

  Cloud box = points::scatterBox({0, 0, 0}, {10, 10, 10}, 100, 3);
  EXPECT_EQ(box.size(), 100u);
  for (const glm::vec3& p : box.positions) {
    EXPECT_GE(p.x, 0);
    EXPECT_LE(p.x, 10);
  }
}

TEST(Points, OnMeshLandsOnSurface) {
  const Mesh quad = mesh::quad(100, 100);  // z = 0 plane
  Cloud cloud = points::onMesh(quad, 64, 5);
  ASSERT_EQ(cloud.size(), 64u);
  for (const glm::vec3& p : cloud.positions) {
    EXPECT_NEAR(p.z, 0, 1e-4);
    EXPECT_LE(std::abs(p.x), 50.01f);
  }
  ASSERT_TRUE(cloud.vectorIf("normal"));
  EXPECT_NEAR((*cloud.vectorIf("normal"))[0].z, 1, 1e-3);
}

TEST(Points, InstanceStampsWithLanes) {
  Cloud cloud = points::ring({0, 0, 0}, 80, 6);
  std::vector<float>& size = cloud.scalar("size", 1);
  size[0] = 2;
  std::vector<glm::vec4>& tint = cloud.color("tint");
  tint[0] = {1, 0, 0, 1};
  const Mesh stamp = mesh::quad(10, 10);
  points::InstanceOptions options;
  options.scaleLane = "size";
  options.tintLane = "tint";
  options.orientLane = "normal";
  const Mesh merged = points::instance(cloud, stamp, options);
  EXPECT_EQ(merged.vertexCount(), 6u * stamp.vertexCount());
  EXPECT_EQ(merged.triangleCount(), 6u * stamp.triangleCount());
  ASSERT_EQ(merged.colors.size(), merged.vertexCount());
  EXPECT_NEAR(merged.colors[0].r, 1, 1e-4);
  EXPECT_NEAR(merged.colors[0].g, 0, 1e-4);
  // The scale lane multiplies the stamp about its own point. An unscaled
  // 10x10 quad has a 14.1 diagonal, so the first stamp — the only one whose
  // "size" was set to 2 — has to measure more than that.
  glm::vec3 lo, hi;
  Mesh first;
  first.positions.assign(merged.positions.begin(),
                         merged.positions.begin() + 4);
  first.bounds(&lo, &hi);
  EXPECT_GT(glm::length(hi - lo), 14.0f);
}

TEST(Points, AppendPadsLanesWithConventionalDefaults) {
  // When a lane exists on only one side of an append, the other side is
  // padded by what the lane NAME means, not by a generic zero: "size" pads
  // with 1, because 0 would make those instances invisible, and "Tex" pads
  // with the identity uv window (0,0,1,1) rather than white.
  Cloud a;
  a.positions = {{0, 0, 0}, {1, 0, 0}};
  Cloud b;
  b.positions = {{2, 0, 0}, {3, 0, 0}};
  b.scalar("size", 2);
  b.color("Tex", {0.5f, 0.5f, 0.5f, 0.5f});
  a.append(b);
  ASSERT_EQ(a.size(), 4u);
  const std::vector<float>* size = a.scalarIf("size");
  ASSERT_TRUE(size);
  ASSERT_EQ(size->size(), 4u);
  EXPECT_FLOAT_EQ((*size)[0], 1.0f);  // a's side: scale 1, visible
  EXPECT_FLOAT_EQ((*size)[1], 1.0f);
  EXPECT_FLOAT_EQ((*size)[2], 2.0f);  // b's actual values
  EXPECT_FLOAT_EQ((*size)[3], 2.0f);
  const std::vector<glm::vec4>* tex = a.colorIf("Tex");
  ASSERT_TRUE(tex);
  ASSERT_EQ(tex->size(), 4u);
  for (size_t i = 0; i < 2; ++i) {  // a's side: identity uv window
    EXPECT_FLOAT_EQ((*tex)[i].x, 0.0f);
    EXPECT_FLOAT_EQ((*tex)[i].y, 0.0f);
    EXPECT_FLOAT_EQ((*tex)[i].z, 1.0f);
    EXPECT_FLOAT_EQ((*tex)[i].w, 1.0f);
  }
  EXPECT_FLOAT_EQ((*tex)[2].x, 0.5f);  // b's actual window
}

TEST(Points, BillboardsCoverPixels) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 150));
  surface->getCanvas()->clear(SK_ColorBLACK);
  Cloud cloud = points::ring({0, 0, 0}, 40, 12, {0, 0, 1});
  space::Camera camera;
  camera.eye = {0, 0, 200};
  points::BillboardStyle style;
  style.size = 24;
  style.tint = {0, 1, 0, 1};
  points::drawBillboards(*surface->getCanvas(), cloud, camera, {200, 150},
                         style);
  SkBitmap bm;
  bm.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(bm.pixmap(), 0, 0));
  // Twelve 24-pixel billboards cover thousands of pixels; the threshold is
  // set low because the point is only that the cloud reached the canvas at
  // all — an empty or entirely off-screen draw is what it must catch.
  int lit = 0;
  for (int y = 0; y < 150; ++y)
    for (int x = 0; x < 200; ++x)
      if (SkColorGetG(bm.getColor(x, y)) > 30) ++lit;
  EXPECT_GT(lit, 200);
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
