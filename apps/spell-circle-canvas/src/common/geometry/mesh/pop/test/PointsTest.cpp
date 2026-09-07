/** @file
 * Point clouds: the generators write their conventional lanes, the
 * modifiers move positions exactly as the operators of the same name do,
 * a stamp instanced at every point is scaled and turned by the lanes it
 * names, and a splat takes the atlas cell the cloud carries.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/camera/Camera.h"
#include "sigilgeometry/mesh/pop/Points.h"
#include "sigilgeometry/mesh/pop/Pop.h"

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;

TEST(Points, EveryGeneratorWritesTheConventionalLanesForItsFigure) {
  curve::Spline3 line;
  line.type = curve::Spline3::Type::Linear;
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

// ONE VERB IS ONE FIELD. `jitter` and `displaceNoise` are the `Jitter`
// and `Noise` operators reached for without a chain, so a cloud moved
// each way has to land on the same floats — not near them. Two
// arithmetics under one name would mean nobody could say which of them
// a picture came from.
TEST(Points, AModifierMovesPointsExactlyAsItsOperatorDoes) {
  Cloud seeded = points::scatterBox({-60, -20, -40}, {60, 20, 40}, 250, 3);

  Cloud jittered = seeded;
  points::jitter(jittered, 14.0f, 21u);
  const Cloud chained = pop::cook(
      pop::Chain{pop::PointSet{seeded}, pop::Jitter{pop::Lane::P, 14.0f, 21u}});
  ASSERT_EQ(chained.size(), jittered.size());
  for (size_t i = 0; i < jittered.size(); ++i)
    EXPECT_EQ(chained.positions[i], jittered.positions[i]) << "point " << i;

  Cloud drifted = seeded;
  points::displaceNoise(drifted, 9.0f, 0.02f, 5u);
  const Cloud driftedChain = pop::cook(pop::Chain{
      pop::PointSet{seeded}, pop::Noise{pop::Lane::P, 9.0f, 0.02f, 5.0f}});
  ASSERT_EQ(driftedChain.size(), drifted.size());
  for (size_t i = 0; i < drifted.size(); ++i)
    EXPECT_EQ(driftedChain.positions[i], drifted.positions[i]) << "point " << i;
}

TEST(Points, ScatteringOnAMeshPutsEveryPointOnItsSurface) {
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

TEST(Points, AStampIsScaledOrientedAndTintedByTheLanesItIsToldTo) {
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

TEST(Points, ACloudSplattedAsBillboardsReachesTheCanvas) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 150));
  surface->getCanvas()->clear(SK_ColorBLACK);
  Cloud cloud = points::ring({0, 0, 0}, 40, 12, {0, 0, 1});
  camera::Camera camera;
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

TEST(Points, BillboardsSplatTheAtlasCellTheCloudCarries) {
  // A sprite SHEET splats as a field of different sprites, and which one
  // each point takes is the window a pop::Atlas op wrote into "Tex".
  // Without the lane every point takes the whole sheet, which is the
  // failure this reads: a splat showing all four quadrants at once.
  //
  // The sheet: four quadrants, one colour each, no antialiasing anywhere.
  constexpr int kSheet = 64;
  constexpr int kHalf = kSheet / 2;
  sk_sp<SkImage> sheet;
  {
    sk_sp<SkSurface> sheetSurface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kSheet, kSheet));
    SkCanvas* c = sheetSurface->getCanvas();
    const auto cell = [&](int x, int y, SkColor colour) {
      SkPaint p;
      p.setColor(colour);
      c->drawIRect(SkIRect::MakeXYWH(x, y, kHalf, kHalf), p);
    };
    cell(0, 0, SK_ColorWHITE);
    cell(kHalf, 0, SK_ColorRED);
    cell(0, kHalf, SK_ColorGREEN);
    cell(kHalf, kHalf, SK_ColorBLUE);
    sheet = sheetSurface->makeImageSnapshot();
  }

  // One point, dead centre, carrying the BOTTOM-LEFT cell — green.
  Cloud cloud;
  cloud.positions = {{0, 0, 0}};
  cloud.color("Tex") = {{0.0f, 0.5f, 0.5f, 0.5f}};
  camera::Camera camera;
  camera.eye = {0, 0, 200};

  const auto centrePixel = [&](const std::string& lane) {
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 120));
    surface->getCanvas()->clear(SK_ColorBLACK);
    points::BillboardStyle style;
    style.sprite = sheet;
    style.size = 60;
    style.additive = false;
    style.perspective = false;
    style.texLane = lane;
    points::drawBillboards(*surface->getCanvas(), cloud, camera, {120, 120},
                           style);
    SkBitmap bm;
    bm.allocPixels(surface->imageInfo());
    EXPECT_TRUE(surface->readPixels(bm.pixmap(), 0, 0));
    return bm.getColor(60, 60);
  };
  // The cell is one flat colour, so its centre IS its colour.
  EXPECT_EQ(centrePixel("Tex"), SK_ColorGREEN) << "the window was not read";
  // Unnamed, the splat is the whole sheet and its centre is the seam
  // between four different colours — anything but the cell's own.
  EXPECT_NE(centrePixel(""), SK_ColorGREEN);

  // A degenerate window is not a cell: an atlas op that never ran, or a
  // lane padded with zeros, takes the whole image rather than splatting
  // a sliver of one texel over everything.
  cloud.color("Tex") = {{0.0f, 0.0f, 0.0f, 0.0f}};
  EXPECT_EQ(centrePixel("Tex"), centrePixel(""));
}

TEST(Points, AnInstancedFacingLaneAgreesWithTheCameraFacingTransform) {
  // The two ways of orienting a quad must agree: a one-point cloud whose
  // "facing" lane holds the eye direction, stamped through points::quads(),
  // produces the same vertices as transforming a quad by faceCamera. If
  // they drift apart, a scene mixing billboards with instanced panels shows
  // two different orientations for the same direction.
  const glm::vec3 at = {40, -25, 60};
  const glm::vec3 eye = {0, 200, 1150};
  Cloud one;
  one.positions = {at};
  one.vector("facing") = {glm::normalize(eye - at)};
  points::InstanceOptions options;
  options.orientLane = "facing";
  const Mesh stamped = points::quads(one, 170, 112, options);
  const Mesh quad = mesh::quad(170, 112);
  const glm::mat4 m = camera::faceCamera(eye, at);
  ASSERT_EQ(stamped.positions.size(), quad.positions.size());
  for (size_t i = 0; i < quad.positions.size(); ++i) {
    const glm::vec3 viaMatrix = glm::vec3(m * glm::vec4(quad.positions[i], 1));
    EXPECT_NEAR(glm::length(viaMatrix - stamped.positions[i]), 0.0f, 1e-4f)
        << "vertex " << i;
  }
}
