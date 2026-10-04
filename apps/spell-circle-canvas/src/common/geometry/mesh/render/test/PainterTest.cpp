/** @file
 * The mesh draw: a mesh covers the pixels it should, the normals mode
 * encodes device space with +y down, and a primitive colour lane tints
 * triangles flat.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilmedia/advanced/Skia.h>

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/render/Painter.h"
#include "support/GeometrySupport.h"

using namespace sigil::geometry::mesh;

using sigil::geometry::test::splitQuad;

namespace {

camera::Camera clippingCamera() {
  camera::Camera camera;
  camera.eye = {0, 0, 0};
  camera.target = {0, 0, -1};
  camera.fovYDeg = 90;
  camera.zNear = 10;
  camera.zFar = 30;
  return camera;
}

// The camera's homogeneous planes locate a boundary even when its
// projection centre differs from the eye. The fixture crosses that
// boundary symmetrically, so its new edge is exactly halfway along each
// side in world space.
float boundaryDepth(const camera::Camera& camera, bool near) {
  const glm::mat4 projection = camera.projection(1);
  const float sign = near ? 1.0f : -1.0f;
  return -(sign * projection[3][3] - projection[3][2]) /
         (projection[2][2] - sign * projection[2][3]);
}

SkBitmap clippingPlate(const Mesh& mesh, const camera::Camera& camera,
                       const render::MeshStyle& style, float density = 1) {
  const int extent = int(200 * density);
  const auto surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(extent, extent));
  surface->getCanvas()->clear(SK_ColorBLACK);
  surface->getCanvas()->scale(density, density);
  render::drawMesh(*surface->getCanvas(), mesh, glm::mat4(1), camera,
                   {200, 200}, style);
  SkBitmap bitmap;
  bitmap.allocPixels(surface->imageInfo());
  EXPECT_TRUE(surface->readPixels(bitmap.pixmap(), 0, 0));
  return bitmap;
}

}  // namespace

TEST(Render, TrianglesOutsideTheCameraDepthRangeLeaveNoPixels) {
  const auto camera = clippingCamera();
  render::MeshStyle style;
  style.lit = false;
  style.baseColor = {1, 0, 0, 1};
  for (float depth : {boundaryDepth(camera, true) - 2,
                      boundaryDepth(camera, false) + 2, -1.0f}) {
    SCOPED_TRACE(depth);
    Mesh mesh;
    mesh.positions = {{-8, -8, -depth}, {8, -8, -depth}, {0, 8, -depth}};
    mesh.indices = {0, 1, 2};
    const SkBitmap bitmap = clippingPlate(mesh, camera, style);
    for (int y = 0; y < bitmap.height(); ++y)
      for (int x = 0; x < bitmap.width(); ++x)
        ASSERT_EQ(bitmap.getColor(x, y), SK_ColorBLACK);
  }
}

TEST(Render, DepthClippingPreservesTheVisiblePartAndItsWinding) {
  const auto camera = clippingCamera();
  render::MeshStyle style;
  style.lit = false;
  style.baseColor = {1, 0, 0, 1};
  for (bool near : {true, false}) {
    const float boundary = boundaryDepth(camera, near);
    const float outside = boundary + (near ? -5 : 5);
    const float inside = boundary + (near ? 5 : -5);
    Mesh mesh;
    mesh.positions = {{0, 8, -outside}, {-8, -8, -inside}, {8, -8, -inside}};
    mesh.indices = {0, 1, 2};
    for (float density : {0.5f, 1.0f, 2.0f}) {
      SCOPED_TRACE(::testing::Message() << near << ":" << density);
      const auto bitmap = clippingPlate(mesh, camera, style, density);
      EXPECT_EQ(bitmap.getColor(int(100 * density), int(80 * density)),
                SK_ColorBLACK);
      EXPECT_EQ(bitmap.getColor(int(100 * density), int(112 * density)),
                SK_ColorRED);
      std::swap(mesh.indices[1], mesh.indices[2]);
      const auto reversed = clippingPlate(mesh, camera, style, density);
      EXPECT_EQ(reversed.getColor(int(100 * density), int(112 * density)),
                SK_ColorBLACK);
      style.backfaceCull = false;
      const auto twoSided = clippingPlate(mesh, camera, style, density);
      EXPECT_EQ(twoSided.getColor(int(100 * density), int(112 * density)),
                SK_ColorRED);
      style.backfaceCull = true;
      std::swap(mesh.indices[1], mesh.indices[2]);
    }
  }
}

TEST(Render, DepthClippingInterpolatesTextureAndColourLanes) {
  const auto camera = clippingCamera();
  render::MeshStyle style;
  style.lit = false;
  style.baseColor = {1, 1, 1, 1};
  style.primitiveColorLane = "Color";
  SkBitmap texture;
  texture.allocN32Pixels(2, 2);
  *texture.getAddr32(0, 0) = SK_ColorWHITE;
  *texture.getAddr32(1, 0) = SK_ColorRED;
  *texture.getAddr32(0, 1) = SK_ColorGREEN;
  *texture.getAddr32(1, 1) = SK_ColorBLUE;
  style.texture = texture.asImage();
  for (bool near : {true, false}) {
    SCOPED_TRACE(near);
    const float boundary = boundaryDepth(camera, near);
    const float outside = boundary + (near ? -5 : 5);
    const float inside = boundary + (near ? 5 : -5);
    Mesh crossing;
    crossing.positions = {
        {0, 8, -outside}, {-8, -8, -inside}, {8, -8, -inside}};
    crossing.indices = {0, 1, 2};
    crossing.uvs = {{0.5f, 0}, {0, 1}, {1, 1}};
    crossing.colors = {{1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}};
    crossing.primitive("Color")[0] = {0.8f, 0.6f, 0.4f, 1};

    // This independently authored trapezoid is the retained lower half
    // of the triangle. Its new vertices carry the halfway UV and tint.
    Mesh retained;
    retained.positions = {{4, 0, -boundary},
                          {-4, 0, -boundary},
                          {-8, -8, -inside},
                          {8, -8, -inside}};
    retained.indices = {0, 1, 2, 0, 2, 3};
    retained.uvs = {{0.75f, 0.5f}, {0.25f, 0.5f}, {0, 1}, {1, 1}};
    retained.colors = {
        {0.5f, 0, 0.5f, 1}, {0.5f, 0.5f, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}};
    for (auto& color : retained.primitive("Color"))
      color = crossing.primitive("Color")[0];
    const auto actual = clippingPlate(crossing, camera, style);
    const auto expected = clippingPlate(retained, camera, style);
    for (int y = 0; y < actual.height(); ++y)
      for (int x = 0; x < actual.width(); ++x) {
        const SkColor a = actual.getColor(x, y), b = expected.getColor(x, y);
        ASSERT_NEAR(SkColorGetR(a), SkColorGetR(b), 2);
        ASSERT_NEAR(SkColorGetG(a), SkColorGetG(b), 2);
        ASSERT_NEAR(SkColorGetB(a), SkColorGetB(b), 2);
      }
  }
}

TEST(Render, ClippingDiscardsNonfiniteVerticesAndHandlesBothPlanes) {
  const auto camera = clippingCamera();
  render::MeshStyle style;
  style.lit = false;
  style.baseColor = {1, 0, 0, 1};
  Mesh mesh;
  mesh.positions = {{0, 8, -1}, {-8, -8, -20}, {8, -8, -50}};
  mesh.indices = {0, 1, 2};
  const auto both = clippingPlate(mesh, camera, style);
  EXPECT_GT(SkColorGetR(both.getColor(100, 110)), 0u);
  for (float invalid : {std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN()}) {
    mesh.positions[0].x = invalid;
    const auto bitmap = clippingPlate(mesh, camera, style);
    for (int y = 0; y < bitmap.height(); ++y)
      for (int x = 0; x < bitmap.width(); ++x)
        ASSERT_EQ(bitmap.getColor(x, y), SK_ColorBLACK);
  }
}

TEST(Render, AMeshDrawnThroughTheCameraLandsInsideItsViewport) {
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

TEST(Render, TheNormalsModeEncodesDeviceSpaceWithYDown) {
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

TEST(Render, APrimitiveColourLaneTintsEachTriangleFlat) {
  Mesh m = splitQuad();
  m.primitive("Color")[0] = {1, 0, 0, 1};  // lower-right half
  m.primitive("Color")[1] = {0, 0, 1, 1};  // upper-left half

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

  style.primitiveColorLane = "Color";
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
  bare.primitiveColorLane.clear();
  EXPECT_EQ(render(style).getColor(120, 95), render(bare).getColor(120, 95));
}

// A style is a VALUE, dial for dial, and so is each of the lights in it:
// two default styles are one style, which is what lets a consumer prove
// a frame asked for the shading the frame before it asked for.
TEST(Painter, AStyleAndItsLightsAreValues) {
  render::MeshStyle style;
  EXPECT_EQ(style, render::MeshStyle{});

  style.lights = {render::Light{}, render::Light{}};
  EXPECT_NE(style, render::MeshStyle{});

  render::MeshStyle same = style;
  EXPECT_EQ(style, same);
  same.lights[1].intensity = 0.5f;
  EXPECT_NE(style, same);
  EXPECT_NE(render::Light{}, same.lights[1]);

  // The runtime takes part: two styles that would draw on different
  // executors are not the same style however their dials read.
  render::MeshStyle rough = style;
  rough.roughness = style.roughness + 0.25f;
  EXPECT_NE(style, rough);
}
