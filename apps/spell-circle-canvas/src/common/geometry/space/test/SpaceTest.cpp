/** @file
 * The Skia painter: a camera projects through viewport pixels, a mesh
 * covers the pixels it should, the normals mode encodes device space
 * with +y down, and a primitive colour lane tints triangles flat.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/space/Space.h"
#include "support/GeometrySupport.h"

using namespace sigil::geometry;

using sigil::geometry::test::splitQuad;

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
