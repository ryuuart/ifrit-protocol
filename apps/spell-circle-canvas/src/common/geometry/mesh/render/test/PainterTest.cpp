/** @file
 * The mesh draw: a mesh covers the pixels it should, the normals mode
 * encodes device space with +y down, and a primitive colour lane tints
 * triangles flat.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/render/Painter.h"
#include "support/GeometrySupport.h"

using namespace sigil::geometry::mesh;

using sigil::geometry::test::splitQuad;

TEST(Render, DrawMeshCoversPixels) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 150));
  surface->getCanvas()->clear(SK_ColorBLACK);
  camera::Camera camera;
  camera.eye = {0, 0, 300};
  render::MeshStyle style;
  style.baseColor = {1, 0, 0, 1};
  render::drawMesh(*surface->getCanvas(), quad(100, 100), glm::mat4(1.0f),
                   camera, {200, 150}, style);
  SkBitmap bm;
  bm.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(bm.pixmap(), 0, 0));
  // A smoke check that the whole painter pipeline reaches pixels: transform,
  // lighting and SkVertices batching all have to work for the middle of a
  // face-on quad to come out lit rather than the cleared black.
  const SkColor c = bm.getColor(100, 75);
  EXPECT_GT(SkColorGetR(c), 40u);
}

TEST(Render, NormalsModeEncodesDeviceSpaceYDown) {
  // The Normals G-buffer is DEVICE-space, +y down — the convention the
  // surface recipes read: rgb = (n.x, -n.y, n.z) * 0.5 + 0.5.
  camera::Camera camera;
  camera.eye = {0, 0, 300};
  render::MeshStyle style;
  style.mode = render::MeshStyle::Mode::Normals;

  // Face-on quad: its +z normal encodes as (128, 128, 255).
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 150));
  surface->getCanvas()->clear(SK_ColorBLACK);
  render::drawMesh(*surface->getCanvas(), quad(100, 100), glm::mat4(1.0f),
                   camera, {200, 150}, style);
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
  render::drawMesh(*tilted->getCanvas(), quad(100, 100), model, camera,
                   {200, 150}, style);
  SkBitmap tiltedBm;
  tiltedBm.allocPixels(tilted->imageInfo());
  ASSERT_TRUE(tilted->readPixels(tiltedBm.pixmap(), 0, 0));
  EXPECT_LT(SkColorGetG(tiltedBm.getColor(100, 75)), 128u);
}

TEST(Render, PrimColorLaneTintsTrianglesFlat) {
  Mesh m = splitQuad();
  m.prim("Color")[0] = {1, 0, 0, 1};  // lower-right half
  m.prim("Color")[1] = {0, 0, 1, 1};  // upper-left half

  camera::Camera camera;
  camera.eye = {0, 0, 300};
  render::MeshStyle style;
  style.baseColor = {1, 1, 1, 1};

  const auto render = [&](const render::MeshStyle& s) {
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 150));
    surface->getCanvas()->clear(SK_ColorBLACK);
    render::drawMesh(*surface->getCanvas(), m, glm::mat4(1.0f), camera,
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
  style.mode = render::MeshStyle::Mode::Normals;
  render::MeshStyle bare = style;
  bare.primColorLane.clear();
  EXPECT_EQ(render(style).getColor(120, 95), render(bare).getColor(120, 95));
}
