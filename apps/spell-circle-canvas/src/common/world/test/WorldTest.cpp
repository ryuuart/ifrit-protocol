#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/Compose.h>
#include <sigilmotion/Ticker.h>
#include <sigilshape/Mesh.h>
#include <sigilshape/Points.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <map>
#include <optional>
#include <vector>

#include "sigilworld/Animation.h"
#include "sigilworld/Components.h"
#include "sigilworld/Easel.h"
#include "sigilworld/Scene.h"
#include "sigilworld/TextureSet.h"
#include "sigilworld/World.h"

using namespace sigil;

namespace {

/** Device bring-up needs a Vulkan runtime. Tests SKIP rather than fail
 *  when the machine has none, so a machine without a GPU stays green.
 *  Anything that must be checked everywhere belongs in one of the
 *  device-free tests further down, which run over a bare registry. */
#define MAKE_WORLD_OR_SKIP(w, config)                        \
  std::unique_ptr<world::World> w;                           \
  {                                                          \
    std::string worldError;                                  \
    w = world::World::create(config, &worldError);           \
    if (!w) GTEST_SKIP() << "no 3D backend: " << worldError; \
  }

}  // namespace

TEST(World, CreatesHeadlessDevice) {
  world::WorldConfig config;
  config.width = 160;
  config.height = 120;
  MAKE_WORLD_OR_SKIP(w, config);
  EXPECT_STREQ(w->backendName(), "Vulkan");
}

TEST(World, RendersClearColorWhenEmpty) {
  world::WorldConfig config;
  config.width = 64;
  config.height = 64;
  config.clearColor = {1, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);
  ASSERT_TRUE(w->render());
  sk_sp<SkImage> frame = w->readback();
  ASSERT_TRUE(frame);
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(64, 64));
  ASSERT_TRUE(frame->readPixels(nullptr, bm.pixmap(), 0, 0));
  const SkColor c = bm.getColor(32, 32);
  EXPECT_GT(SkColorGetR(c), 200u);
  EXPECT_LT(SkColorGetG(c), 40u);
}

// The COLOUR SPACE of WorldConfig::clearColor.
//
// clearColor is ENCODED sRGB, while every other colour in the API is
// linear. The clear writes straight into the RGBA8_UNORM target with no
// shader in the way, so its components are the bytes the background
// pixel gets. RendersClearColorWhenEmpty above cannot see this: it uses
// 0 and 1, the two fixed points of the sRGB curve. This test picks
// mid-tones, where the two readings are far apart.
//
// It exists to fail loudly if someone "fixes" the asymmetry by running
// clearColor through the encode, which would move the background of
// every scene rendered by this library.
TEST(World, ClearColorIsEncodedSrgbNotLinear) {
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  config.clearColor = {0.5f, 0.25f, 0.75f, 1};
  MAKE_WORLD_OR_SKIP(w, config);
  ASSERT_TRUE(w->render());
  sk_sp<SkImage> frame = w->readback();
  ASSERT_TRUE(frame);
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(32, 32));
  ASSERT_TRUE(frame->readPixels(nullptr, bm.pixmap(), 0, 0));
  const SkColor c = bm.getColor(16, 16);
  // Encoded reading: the components ARE the bytes, times 255.
  EXPECT_NEAR((int)SkColorGetR(c), 128, 2) << "clearColor is encoded sRGB";
  EXPECT_NEAR((int)SkColorGetG(c), 64, 2);
  EXPECT_NEAR((int)SkColorGetB(c), 191, 2);
  // The linear reading — what treating this value as linear and encoding
  // it would give — is far higher in every channel. These bounds sit
  // between the two, so either interpretation is distinguishable.
  EXPECT_LT(SkColorGetR(c), 160u) << "clearColor must NOT be encoded";
  EXPECT_LT(SkColorGetG(c), 100u);
  EXPECT_LT(SkColorGetB(c), 208u);
}

TEST(World, RendersLitQuadCoveringCenter) {
  world::WorldConfig config;
  config.width = 200;
  config.height = 150;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);

  shape::space::Camera camera;
  camera.eye = {0, 0, 300};
  camera.target = {0, 0, 0};
  w->setCamera(camera);

  world::Material material;
  material.baseColor = {0.9f, 0.2f, 0.2f, 1};
  material.roughness = 0.6f;
  const uint32_t id =
      w->addSurface(shape::mesh::quad(200, 200), glm::mat4(1.0f), material);
  ASSERT_NE(id, 0u);

  ASSERT_TRUE(w->render());
  sk_sp<SkImage> frame = w->readback();
  ASSERT_TRUE(frame);
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(200, 150));
  ASSERT_TRUE(frame->readPixels(nullptr, bm.pixmap(), 0, 0));
  // Center: lit red-ish, definitely not clear color.
  const SkColor center = bm.getColor(100, 75);
  EXPECT_GT(SkColorGetR(center), 60u);
  EXPECT_GT(SkColorGetR(center), SkColorGetB(center));
  // Far corner: outside the quad's projection, still background.
  const SkColor corner = bm.getColor(3, 3);
  EXPECT_LT(SkColorGetR(corner), 30u);
}

TEST(World, TexturedUnlitPanelShowsTexture) {
  world::WorldConfig config;
  config.width = 200;
  config.height = 150;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);

  shape::space::Camera camera;
  camera.eye = {0, 0, 300};
  w->setCamera(camera);

  // Half green / half blue texture; unlit, so colors pass through.
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 64));
  surface->getCanvas()->clear(SK_ColorGREEN);
  SkPaint paint;
  paint.setColor(SK_ColorBLUE);
  surface->getCanvas()->drawRect(SkRect::MakeXYWH(32, 0, 32, 64), paint);

  world::Material material;
  material.unlit = true;
  material.texture = surface->makeImageSnapshot();
  ASSERT_NE(
      w->addSurface(shape::mesh::quad(220, 160), glm::mat4(1.0f), material),
      0u);

  ASSERT_TRUE(w->render());
  sk_sp<SkImage> frame = w->readback();
  ASSERT_TRUE(frame);
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(200, 150));
  ASSERT_TRUE(frame->readPixels(nullptr, bm.pixmap(), 0, 0));
  // Left half green-dominant, right half blue-dominant. If these come
  // back swapped the u axis flipped; if vertically odd, y did.
  const SkColor left = bm.getColor(60, 75);
  const SkColor right = bm.getColor(140, 75);
  EXPECT_GT(SkColorGetG(left), SkColorGetB(left));
  EXPECT_GT(SkColorGetB(right), SkColorGetG(right));
}

namespace {

/** A solid-colour raster image, or a two-column one when @p right is
 *  given. */
sk_sp<SkImage> solidImage(SkColor color, int w = 8, int h = 8,
                          std::optional<SkColor> right = std::nullopt) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
  surface->getCanvas()->clear(color);
  if (right) {
    SkPaint paint;
    paint.setColor(*right);
    surface->getCanvas()->drawRect(SkRect::MakeXYWH(w / 2.0f, 0, w / 2.0f, h),
                                   paint);
  }
  return surface->makeImageSnapshot();
}

SkBitmap readFrame(world::World& w) {
  sk_sp<SkImage> frame = w.readback();
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(frame->width(), frame->height()));
  frame->readPixels(nullptr, bm.pixmap(), 0, 0);
  return bm;
}

float luma(SkColor c) {
  return 0.2126f * SkColorGetR(c) + 0.7152f * SkColorGetG(c) +
         0.0722f * SkColorGetB(c);
}

}  // namespace

TEST(World, SwappingATextureIsLive) {
  // The image pointer on a MaterialComponent is as live as its colours:
  // move it and the next render draws the new image, no re-add.
  world::WorldConfig config;
  config.width = 64;
  config.height = 64;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);
  shape::space::Camera camera;
  camera.eye = {0, 0, 300};
  w->setCamera(camera);
  world::Material material;
  material.unlit = true;
  material.texture = solidImage(SK_ColorRED);
  const uint32_t id =
      w->addSurface(shape::mesh::quad(400, 400), glm::mat4(1.0f), material);
  ASSERT_NE(id, 0u);
  ASSERT_TRUE(w->render());
  SkColor c = readFrame(*w).getColor(32, 32);
  EXPECT_GT(SkColorGetR(c), 200);
  EXPECT_LT(SkColorGetB(c), 30);

  w->registry()
      .get<world::MaterialComponent>(world::entity(id))
      .material.texture = solidImage(SK_ColorBLUE);
  ASSERT_TRUE(w->render());
  c = readFrame(*w).getColor(32, 32);
  EXPECT_GT(SkColorGetB(c), 200) << "the swapped image must be bound";
  EXPECT_LT(SkColorGetR(c), 30);

  // Instanced surfaces take the same door.
  shape::Cloud one;
  one.positions = {{0, 0, 0}};
  world::Material inst;
  inst.unlit = true;
  inst.texture = solidImage(SK_ColorGREEN);
  w->removeSurface(id);
  const uint32_t flock =
      w->addInstanced(shape::mesh::quad(400, 400), one, inst);
  ASSERT_NE(flock, 0u);
  ASSERT_TRUE(w->render());
  c = readFrame(*w).getColor(32, 32);
  EXPECT_GT(SkColorGetG(c), 200);
  w->registry()
      .get<world::MaterialComponent>(world::entity(flock))
      .material.texture = solidImage(SK_ColorRED);
  ASSERT_TRUE(w->render());
  c = readFrame(*w).getColor(32, 32);
  EXPECT_GT(SkColorGetR(c), 200);
  EXPECT_LT(SkColorGetG(c), 30);
}

TEST(World, TileRepeatsWhereClampSmears) {
  // A two-column image (red | blue) under uvScale {2, 1}: at u = 0.625
  // the window reads 1.25 — a tiling material wraps to 0.25 (red), a
  // clamping one smears the last column (blue).
  world::WorldConfig config;
  config.width = 64;
  config.height = 64;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);
  shape::space::Camera camera;
  camera.eye = {0, 0, 300};
  camera.fovYDeg = 40;
  w->setCamera(camera);
  world::Material material;
  material.unlit = true;
  material.texture = solidImage(SK_ColorRED, 8, 8, SK_ColorBLUE);
  material.uvScale = {2, 1};
  material.tile = true;
  const uint32_t id =
      w->addSurface(shape::mesh::quad(1000, 1000), glm::mat4(1.0f), material);
  ASSERT_NE(id, 0u);
  ASSERT_TRUE(w->render());
  // The quad fills the frame; x = 40 of 64 is u = 0.625.
  SkColor c = readFrame(*w).getColor(40, 32);
  EXPECT_GT(SkColorGetR(c), 200) << "tile wraps back to the first column";
  EXPECT_LT(SkColorGetB(c), 80);

  world::Material& live =
      w->registry().get<world::MaterialComponent>(world::entity(id)).material;
  live.tile = false;
  ASSERT_TRUE(w->render());
  c = readFrame(*w).getColor(40, 32);
  EXPECT_GT(SkColorGetB(c), 200) << "clamp smears the edge column";
  EXPECT_LT(SkColorGetR(c), 80);
}

TEST(World, NormalMapTiltsShadingInItsConvention) {
  // A quad facing the camera, one directional light from ABOVE at a
  // grazing angle. A normal map that tilts every normal toward its own
  // "up" (green) brightens the quad under an OpenGL-convention read and
  // darkens it under a DirectX read of the same pixels — the two
  // conventions disagree by exactly the sign of green, and the flat map
  // sits between them.
  world::WorldConfig config;
  config.width = 64;
  config.height = 64;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);
  shape::space::Camera camera;
  camera.eye = {0, 0, 300};
  w->setCamera(camera);
  world::Lighting lighting;
  lighting.sunDirection = {0, -1, -0.25f};  // from above, grazing
  lighting.sunIntensity = 3;
  lighting.ambient = 0;
  w->setLighting(lighting);

  // Tilted normal: (0, +0.7, 0.71) encoded -> green high.
  const sk_sp<SkImage> tilted = solidImage(SkColorSetARGB(255, 128, 218, 218));
  world::Material flat;
  flat.baseColor = {0.8f, 0.8f, 0.8f, 1};
  world::Material gl = flat;
  gl.normalMap = tilted;
  world::Material dx = gl;
  dx.normalMapDirectX = true;

  const auto lumaOf = [&](const world::Material& m) {
    const uint32_t id =
        w->addSurface(shape::mesh::quad(400, 400), glm::mat4(1.0f), m);
    EXPECT_NE(id, 0u);
    EXPECT_TRUE(w->render());
    const float l = luma(readFrame(*w).getColor(32, 32));
    w->removeSurface(id);
    return l;
  };
  const float lFlat = lumaOf(flat);
  const float lGl = lumaOf(gl);
  const float lDx = lumaOf(dx);
  EXPECT_GT(lGl, lFlat + 15) << "OpenGL green tilts toward the light";
  EXPECT_LT(lDx, lFlat - 5) << "DirectX green tilts away";
  // normalScale 0 is the flat result again.
  world::Material off = gl;
  off.normalScale = 0;
  EXPECT_NEAR(lumaOf(off), lFlat, 3);
}

TEST(World, EmissiveMapGlowsWhereItIsLit) {
  // No lights at all: only emission can brighten a pixel. A two-column
  // emissive map (green | black) under a white emissive colour glows on
  // the left and stays black on the right; strength 0 turns it off; the
  // unlit branch reads the same map.
  world::WorldConfig config;
  config.width = 64;
  config.height = 64;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);
  shape::space::Camera camera;
  camera.eye = {0, 0, 300};
  w->setCamera(camera);
  world::Lighting dark;
  dark.sunIntensity = 0;
  dark.ambient = 0;
  w->setLighting(dark);
  world::Material m;
  m.baseColor = {0, 0, 0, 1};
  m.emissive = {1, 1, 1, 1};
  m.emissiveStrength = 1;
  m.emissiveMap = solidImage(SK_ColorGREEN, 8, 8, SK_ColorBLACK);
  // The quad just fills the frame, so x = 16 and 48 read u = 0.25 and
  // 0.75 — well inside each column, clear of the bilinear seam.
  const uint32_t id =
      w->addSurface(shape::mesh::quad(240, 240), glm::mat4(1.0f), m);
  ASSERT_NE(id, 0u);
  ASSERT_TRUE(w->render());
  SkBitmap bm = readFrame(*w);
  EXPECT_GT(SkColorGetG(bm.getColor(16, 32)), 120)
      << "the map's green half glows";
  EXPECT_LT(SkColorGetR(bm.getColor(16, 32)), 20);
  EXPECT_LT(luma(bm.getColor(48, 32)), 8) << "the black half emits nothing";

  world::Material& live =
      w->registry().get<world::MaterialComponent>(world::entity(id)).material;
  live.emissiveStrength = 0;
  ASSERT_TRUE(w->render());
  EXPECT_LT(luma(readFrame(*w).getColor(16, 32)), 8) << "strength 0 is off";

  live.emissiveStrength = 1;
  live.unlit = true;
  ASSERT_TRUE(w->render());
  bm = readFrame(*w);
  EXPECT_GT(SkColorGetG(bm.getColor(16, 32)), 200) << "unlit adds the map too";
  EXPECT_LT(luma(bm.getColor(48, 32)), 8);
}

TEST(World, GlassShowsTheOpaquePassThroughItself) {
  // A red quad behind, a white glass quad in front. With transmission 1
  // and an ior of 1 (no bend) the centre reads red — the opaque pass
  // seen through the glass, tinted white — and opaque, not blended;
  // transmission 0 on the same material is a plain white surface; a
  // blue tint colours what is seen.
  world::WorldConfig config;
  config.width = 64;
  config.height = 64;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);
  shape::space::Camera camera;
  camera.eye = {0, 0, 300};
  w->setCamera(camera);
  world::Lighting lighting;
  lighting.sunDirection = {0, 0, -1};
  lighting.sunIntensity = 2;
  lighting.ambient = 0.3f;
  w->setLighting(lighting);
  world::Material red;
  red.baseColor = {1, 0, 0, 1};
  red.roughness = 1;
  ASSERT_NE(w->addSurface(shape::mesh::quad(400, 400),
                          glm::translate(glm::mat4(1.0f), {0, 0, -100}), red),
            0u);
  world::Material glass;
  glass.baseColor = {1, 1, 1, 1};
  glass.roughness = 0.1f;
  glass.transmission = 1;
  glass.ior = 1;
  const uint32_t pane =
      w->addSurface(shape::mesh::quad(400, 400),
                    glm::translate(glm::mat4(1.0f), {0, 0, 50}), glass);
  ASSERT_NE(pane, 0u);
  ASSERT_TRUE(w->render());
  SkColor c = readFrame(*w).getColor(20, 44);  // off the specular centre
  EXPECT_GT(SkColorGetR(c), 120) << "the red quad shows through";
  EXPECT_GT((int)SkColorGetR(c) - (int)SkColorGetG(c), 60);

  world::Material& live =
      w->registry().get<world::MaterialComponent>(world::entity(pane)).material;
  live.transmission = 0;
  ASSERT_TRUE(w->render());
  c = readFrame(*w).getColor(20, 44);
  EXPECT_LT((int)SkColorGetR(c) - (int)SkColorGetG(c), 20) << "opaque white";
  EXPECT_GT(luma(c), 60);

  live.transmission = 1;
  live.baseColor = {0, 0, 1, 1};  // a blue tint over red shows dark blue-ish
  ASSERT_TRUE(w->render());
  c = readFrame(*w).getColor(20, 44);
  EXPECT_LT(SkColorGetR(c), 60) << "the tint multiplies what is seen";
}

TEST(World, OpacityMapAndCutoutRouteAndDiscard) {
  // A white quad with a two-column opacity map (black | white): the
  // left half is transparent onto the clear colour, the right opaque.
  // With a cutoff the left half is discarded outright — the depth
  // buffer sees nothing there, so a surface behind it shows.
  world::WorldConfig config;
  config.width = 64;
  config.height = 64;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);
  shape::space::Camera camera;
  camera.eye = {0, 0, 300};
  w->setCamera(camera);
  world::Material m;
  m.unlit = true;
  m.baseColor = {1, 1, 1, 1};
  m.opacityMap = solidImage(SK_ColorBLACK, 8, 8, SK_ColorWHITE);
  EXPECT_TRUE(m.blended());
  const uint32_t id =
      w->addSurface(shape::mesh::quad(240, 240), glm::mat4(1.0f), m);
  ASSERT_NE(id, 0u);
  ASSERT_TRUE(w->render());
  SkBitmap bm = readFrame(*w);
  EXPECT_LT(luma(bm.getColor(16, 32)), 8) << "transparent half";
  EXPECT_GT(luma(bm.getColor(48, 32)), 240) << "opaque half";

  // Cutout: an unlit green quad BEHIND; the discarded half lets it
  // through, the kept half hides it.
  world::Material green;
  green.unlit = true;
  green.baseColor = {0, 1, 0, 1};
  ASSERT_NE(w->addSurface(shape::mesh::quad(240, 240),
                          glm::translate(glm::mat4(1.0f), {0, 0, -50}), green),
            0u);
  world::Material& live =
      w->registry().get<world::MaterialComponent>(world::entity(id)).material;
  live.alphaCutoff = 0.5f;
  ASSERT_TRUE(w->render());
  bm = readFrame(*w);
  EXPECT_GT(SkColorGetG(bm.getColor(16, 32)), 200) << "cut away: green shows";
  EXPECT_LT(SkColorGetR(bm.getColor(16, 32)), 30);
  EXPECT_GT(SkColorGetR(bm.getColor(48, 32)), 240) << "kept: white";
}

TEST(World, PackedMapChannelsReachTheirSlots) {
  // One RGB image standing in for an occlusion-roughness-metallic pack:
  // occlusion in R, roughness in G, metallic in B. Metallic 1 through
  // channel 2 (blue = 255) darkens the diffuse lobe of a grey sphere
  // versus the same map read through channel 0 (red = 0, so metallic 0).
  world::WorldConfig config;
  config.width = 64;
  config.height = 64;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);
  shape::space::Camera camera;
  camera.eye = {0, 0, 300};
  w->setCamera(camera);
  world::Lighting lighting;
  lighting.sunDirection = {0, 0, -1};
  lighting.ambient = 0.2f;
  w->setLighting(lighting);
  const sk_sp<SkImage> orm = solidImage(SkColorSetARGB(255, 0, 255, 255));
  world::Material dielectric;
  dielectric.baseColor = {0.6f, 0.6f, 0.6f, 1};
  dielectric.metallic = 1;
  dielectric.metallicMap = orm;
  dielectric.metallicChannel = 0;  // reads red = 0 -> metallic 0
  world::Material metal = dielectric;
  metal.metallicChannel = 2;  // reads blue = 1 -> metallic 1
  const auto lumaOf = [&](const world::Material& m) {
    const uint32_t id = w->addSurface(
        shape::mesh::superellipsoid({90, 90, 90}, 1), glm::mat4(1.0f), m);
    EXPECT_NE(id, 0u);
    EXPECT_TRUE(w->render());
    // Off-centre, away from the specular highlight.
    const float l = luma(readFrame(*w).getColor(22, 40));
    w->removeSurface(id);
    return l;
  };
  EXPECT_GT(lumaOf(dielectric), lumaOf(metal) + 10);
}

TEST(World, EnvironmentPanoramaLightsFromItsBrightSide) {
  // No sun, no lights: only the panorama. Its left half is white and
  // its right half black, and u = 0.25 faces -X, so a rough white sphere
  // is lit on its left. Turning the panorama 180 degrees swaps the
  // sides; removing it (and the hemisphere fallback with ambient 0)
  // leaves the sphere black.
  world::WorldConfig config;
  config.width = 96;
  config.height = 64;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);
  shape::space::Camera camera;
  camera.eye = {0, 0, 300};
  w->setCamera(camera);
  world::Lighting lighting;
  lighting.sunIntensity = 0;
  lighting.ambient = 1;
  lighting.skyColor = lighting.groundColor = {0, 0, 0, 1};
  lighting.environment = solidImage(SK_ColorWHITE, 64, 32, SK_ColorBLACK);
  w->setLighting(lighting);
  world::Material white;
  white.baseColor = {1, 1, 1, 1};
  white.roughness = 0.9f;
  ASSERT_NE(
      w->addSurface(shape::mesh::superellipsoid({120, 120, 120}, 2, 64, 48),
                    glm::mat4(1.0f), white),
      0u);
  ASSERT_TRUE(w->render());
  SkBitmap bm = readFrame(*w);
  const float left = luma(bm.getColor(24, 32));
  const float right = luma(bm.getColor(72, 32));
  EXPECT_GT(left, right + 40) << "lit from the panorama's bright half";
  EXPECT_GT(left, 60);

  lighting.environmentRotationDeg = 180;
  w->setLighting(lighting);
  ASSERT_TRUE(w->render());
  bm = readFrame(*w);
  EXPECT_GT(luma(bm.getColor(72, 32)), luma(bm.getColor(24, 32)) + 40)
      << "turned about +Y, the bright half faces the other side";

  lighting.environment = nullptr;
  w->setLighting(lighting);
  ASSERT_TRUE(w->render());
  bm = readFrame(*w);
  EXPECT_LT(luma(bm.getColor(24, 32)), 8) << "no panorama, no light";
}

TEST(WorldTextureSet, ClassifiesTheToolsNames) {
  using world::textures::classify;
  using world::textures::Role;
  // Substance Painter / Designer.
  EXPECT_EQ(classify("Rock_BaseColor.png").role, Role::BaseColor);
  EXPECT_EQ(classify("Rock_BaseColor.png").set, "Rock");
  EXPECT_EQ(classify("Rock_Normal.png").role, Role::Normal);
  EXPECT_FALSE(classify("Rock_Normal.png").directX);
  EXPECT_TRUE(classify("Rock_NormalDX.png").directX);
  EXPECT_TRUE(classify("Rock_Normal_DirectX.png").directX);
  EXPECT_EQ(classify("Rock_Roughness.png").role, Role::Roughness);
  EXPECT_EQ(classify("Rock_Metallic.png").role, Role::Metallic);
  EXPECT_EQ(classify("Rock_Height.png").role, Role::Height);
  EXPECT_EQ(classify("Rock_Emissive.png").role, Role::Emissive);
  EXPECT_EQ(classify("Rock_OcclusionRoughnessMetallic.png").role, Role::Packed);
  // Poly Haven.
  EXPECT_EQ(classify("metal_plate_diff_1k.png").role, Role::BaseColor);
  EXPECT_EQ(classify("metal_plate_diff_1k.png").set, "metal_plate");
  EXPECT_EQ(classify("metal_plate_nor_gl_1k.png").role, Role::Normal);
  EXPECT_FALSE(classify("metal_plate_nor_gl_1k.png").directX);
  EXPECT_EQ(classify("metal_plate_nor_gl_1k.png").set, "metal_plate");
  EXPECT_TRUE(classify("metal_plate_nor_dx_2k.png").directX);
  EXPECT_EQ(classify("metal_plate_rough_1k.png").role, Role::Roughness);
  EXPECT_EQ(classify("metal_plate_metal_1k.png").role, Role::Metallic);
  EXPECT_EQ(classify("metal_plate_ao_1k.png").role, Role::Occlusion);
  EXPECT_EQ(classify("metal_plate_arm_1k.png").role, Role::Packed);
  EXPECT_EQ(classify("metal_plate_disp_1k.png").role, Role::Height);
  // ambientCG.
  EXPECT_EQ(classify("Metal049A_1K-PNG_Color.png").role, Role::BaseColor);
  EXPECT_EQ(classify("Metal049A_1K-PNG_NormalGL.png").role, Role::Normal);
  EXPECT_TRUE(classify("Metal049A_1K-PNG_NormalDX.png").directX);
  EXPECT_EQ(classify("Metal049A_1K-PNG_Metalness.png").role, Role::Metallic);
  EXPECT_EQ(classify("Metal049A_1K-PNG_AmbientOcclusion.png").role,
            Role::Occlusion);
  EXPECT_EQ(classify("Metal049A_1K-PNG_Displacement.png").role, Role::Height);
  EXPECT_EQ(classify("Metal049A_1K-PNG_Color.png").set, "Metal049A_1K_PNG");
  // glTF-ish.
  EXPECT_EQ(classify("thing_orm.png").role, Role::Packed);
  EXPECT_EQ(classify("thing_albedo.jpg").role, Role::BaseColor);
  // Nothing recognizable.
  EXPECT_EQ(classify("photo.png").role, Role::Unknown);
  EXPECT_EQ(classify("IMG_2048.png").role, Role::Unknown);
}

TEST(WorldTextureSet, BuildsAMaterialAndWiresThePack) {
  // A set discovered from a scratch directory: a base colour, a DirectX
  // normal, a packed ARM, and a separate roughness that outranks the
  // pack's green channel. The decoder is a stub that stamps the role
  // colour so slots can be told apart by pointer identity.
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path() / "sigilworld_texset";
  fs::create_directories(dir);
  for (const char* name :
       {"tiles_diff_1k.png", "tiles_nor_dx_1k.png", "tiles_arm_1k.png",
        "tiles_rough_1k.png", "other_BaseColor.png", "notes.txt"}) {
    FILE* f = std::fopen((dir / name).string().c_str(), "wb");
    ASSERT_TRUE(f);
    std::fputs("x", f);
    std::fclose(f);
  }
  const std::vector<world::textures::TextureSet> sets =
      world::textures::discover(dir);
  ASSERT_EQ(sets.size(), 2u);
  EXPECT_EQ(sets[0].name, "other");
  EXPECT_EQ(sets[1].name, "tiles");
  const world::textures::TextureSet& tiles = sets[1];
  EXPECT_TRUE(tiles.normalDirectX);
  EXPECT_EQ(tiles.files.size(), 4u);

  std::map<std::string, sk_sp<SkImage>> decoded;
  const auto decode = [&](const fs::path& p) {
    sk_sp<SkImage>& img = decoded[p.filename().string()];
    if (!img) img = solidImage(SK_ColorWHITE, 2, 2);
    return img;
  };
  const world::Material m = world::textures::material(tiles, decode);
  EXPECT_EQ(m.texture.get(), decoded["tiles_diff_1k.png"].get());
  EXPECT_EQ(m.normalMap.get(), decoded["tiles_nor_dx_1k.png"].get());
  EXPECT_TRUE(m.normalMapDirectX);
  EXPECT_EQ(m.roughnessMap.get(), decoded["tiles_rough_1k.png"].get());
  EXPECT_EQ(m.roughnessChannel, 0);
  EXPECT_EQ(m.metallicMap.get(), decoded["tiles_arm_1k.png"].get());
  EXPECT_EQ(m.metallicChannel, 2);
  EXPECT_EQ(m.occlusionMap.get(), decoded["tiles_arm_1k.png"].get());
  EXPECT_EQ(m.occlusionChannel, 0);
  EXPECT_FLOAT_EQ(m.metallic, 1);
  EXPECT_FLOAT_EQ(m.roughness, 1);
  EXPECT_TRUE(m.tile);
  EXPECT_FALSE(m.emissiveMap);
  fs::remove_all(dir);

  // The by-usage door: what a rendered Substance graph hands over. A
  // "diffuse" key lands on the base slot, an "arm" pack fills the three
  // channel slots, and the Substance default is a DirectX normal.
  const sk_sp<SkImage> a = solidImage(SK_ColorWHITE, 2, 2);
  const sk_sp<SkImage> b = solidImage(SK_ColorWHITE, 2, 2);
  const sk_sp<SkImage> c = solidImage(SK_ColorWHITE, 2, 2);
  const world::Material u = world::textures::material(
      {{"diffuse", a}, {"normal", b}, {"arm", c}, {"height", a}});
  EXPECT_EQ(u.texture.get(), a.get());
  EXPECT_EQ(u.normalMap.get(), b.get());
  EXPECT_TRUE(u.normalMapDirectX);
  EXPECT_EQ(u.roughnessMap.get(), c.get());
  EXPECT_EQ(u.roughnessChannel, 1);
  EXPECT_EQ(u.metallicMap.get(), c.get());
  EXPECT_EQ(u.metallicChannel, 2);
  EXPECT_EQ(u.occlusionMap.get(), c.get());
  EXPECT_EQ(u.occlusionChannel, 0);
  EXPECT_EQ(world::textures::roleForUsage("ambientOcclusion"),
            world::textures::Role::Occlusion);
  EXPECT_EQ(world::textures::roleForUsage("baseColor"),
            world::textures::Role::BaseColor);
  EXPECT_EQ(world::textures::roleForUsage("wibble"),
            world::textures::Role::Unknown);

  // The imported-part door: factors become the scalars, the pack and a
  // separate occlusion naming the same bytes share one decode, glTF
  // normals read as OpenGL, and the sampler tiles.
  shape::import::Part part;
  part.baseColor = {0.5f, 0.6f, 0.7f, 1};
  part.metallic = 0.25f;
  part.roughness = 0.8f;
  part.textureBytes = {std::byte{1}};
  part.textures["normal"].bytes = {std::byte{2}};
  part.textures["orm"].bytes = {std::byte{3}};
  part.textures["occlusion"].bytes = {std::byte{3}};
  int decodes = 0;
  std::map<int, sk_sp<SkImage>> byByte;
  const auto decodeBytes = [&](const std::vector<std::byte>& bytes,
                               std::string_view) {
    ++decodes;
    sk_sp<SkImage>& img = byByte[(int)bytes[0]];
    if (!img) img = solidImage(SK_ColorWHITE, 2, 2);
    return img;
  };
  const world::Material p = world::textures::material(part, decodeBytes);
  EXPECT_EQ(p.texture.get(), byByte[1].get());
  EXPECT_EQ(p.normalMap.get(), byByte[2].get());
  EXPECT_FALSE(p.normalMapDirectX);
  EXPECT_EQ(p.roughnessMap.get(), byByte[3].get());
  EXPECT_EQ(p.roughnessChannel, 1);
  EXPECT_EQ(p.metallicMap.get(), byByte[3].get());
  EXPECT_EQ(p.metallicChannel, 2);
  EXPECT_EQ(p.occlusionMap.get(), byByte[3].get());
  EXPECT_EQ(p.occlusionChannel, 0);
  EXPECT_FLOAT_EQ(p.metallic, 0.25f) << "the factor multiplies the map";
  EXPECT_FLOAT_EQ(p.roughness, 0.8f);
  EXPECT_FLOAT_EQ(p.baseColor.g, 0.6f);
  EXPECT_TRUE(p.tile);
}

TEST(World, BakedVertexColorsTintBothPipelines) {
  world::WorldConfig config;
  config.width = 200;
  config.height = 150;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);

  shape::space::Camera camera;
  camera.eye = {0, 0, 400};
  w->setCamera(camera);

  auto grab = [&](SkBitmap* bm) {
    if (!w->render()) return false;
    sk_sp<SkImage> frame = w->readback();
    if (!frame) return false;
    bm->allocPixels(SkImageInfo::MakeN32Premul(200, 150));
    return frame->readPixels(nullptr, bm->pixmap(), 0, 0);
  };

  // Plain pipeline: a mesh's baked colour lane reaches the fragment
  // directly. Left half red, right half blue.
  shape::Mesh mesh = shape::mesh::quad(220, 160);
  mesh.colors.resize(mesh.positions.size());
  for (size_t i = 0; i < mesh.positions.size(); ++i)
    mesh.colors[i] =
        mesh.positions[i].x < 0 ? glm::vec4{1, 0, 0, 1} : glm::vec4{0, 0, 1, 1};
  world::Material material;
  material.unlit = true;
  const uint32_t id = w->addSurface(mesh, glm::mat4(1.0f), material);
  ASSERT_NE(id, 0u);

  SkBitmap bm;
  ASSERT_TRUE(grab(&bm));
  const SkColor left = bm.getColor(60, 75);
  const SkColor right = bm.getColor(140, 75);
  EXPECT_GT(SkColorGetR(left), SkColorGetB(left));
  EXPECT_GT(SkColorGetB(right), SkColorGetR(right));
  w->removeSurface(id);

  // Instanced pipeline: a stamp's baked colour MULTIPLIES the
  // per-instance tint. Yellow stamp times cyan tint is green — a colour
  // neither input shows on its own, so passing either through
  // unmultiplied fails both counts below.
  shape::Mesh stamp = shape::mesh::quad(14, 14);
  stamp.colors.assign(stamp.positions.size(), glm::vec4{1, 1, 0, 1});
  shape::Cloud field =
      shape::points::grid({-150, -100, 0}, {300, 0, 0}, {0, 200, 0}, 10, 8);
  field.color("tint", {0, 1, 1, 1});
  world::InstanceLanes lanes;
  lanes.tintLane = "tint";
  ASSERT_NE(w->addInstanced(stamp, field, material, lanes), 0u);

  ASSERT_TRUE(grab(&bm));
  int green = 0, passthrough = 0;
  for (int y = 0; y < 150; ++y)
    for (int x = 0; x < 200; ++x) {
      const SkColor c = bm.getColor(x, y);
      const bool g = SkColorGetG(c) > 120;
      const bool r = SkColorGetR(c) > 120;
      const bool b = SkColorGetB(c) > 120;
      if (g && !r && !b)
        ++green;
      else if (g && (r != b))
        ++passthrough;  // yellow (color only) or cyan (tint only)
    }
  EXPECT_GT(green, 200) << "stamps must render color*tint green";
  EXPECT_EQ(passthrough, 0) << "neither lane may pass through alone";
}

TEST(World, QuadAtPositiveYAppearsInTopHalf) {
  world::WorldConfig config;
  config.width = 100;
  config.height = 100;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);
  shape::space::Camera camera;
  camera.eye = {0, 0, 500};
  camera.target = {0, 0, 0};
  w->setCamera(camera);
  world::Material material;
  material.unlit = true;
  material.baseColor = {0, 1, 0, 1};
  ASSERT_NE(
      w->addSurface(shape::mesh::quad(100, 100),
                    glm::translate(glm::mat4(1.0f), {0, 150, 0}), material),
      0u);
  ASSERT_TRUE(w->render());
  sk_sp<SkImage> frame = w->readback();
  ASSERT_TRUE(frame);
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(100, 100));
  ASSERT_TRUE(frame->readPixels(nullptr, bm.pixmap(), 0, 0));
  int top = 0, bottom = 0;
  for (int y = 0; y < 100; ++y)
    for (int x = 0; x < 100; ++x)
      if (SkColorGetG(bm.getColor(x, y)) > 100) (y < 50 ? top : bottom)++;
  EXPECT_GT(top, 0);
  EXPECT_EQ(bottom, 0) << "world +y must render at the TOP of the frame";
}

TEST(World, SavePngWritesFile) {
  world::WorldConfig config;
  config.width = 96;
  config.height = 64;
  MAKE_WORLD_OR_SKIP(w, config);
  ASSERT_TRUE(w->render());
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("sigilworld_test_" + std::to_string(::getpid()) + ".png");
  EXPECT_TRUE(w->savePng(path));
  EXPECT_GT(std::filesystem::file_size(path), 100u);
  std::filesystem::remove(path);
}

TEST(Scene, ReconcilesInsteadOfRebuilding) {
  world::WorldConfig config;
  config.width = 96;
  config.height = 64;
  MAKE_WORLD_OR_SKIP(w, config);

  auto mesh = std::make_shared<const shape::Mesh>(shape::mesh::quad(50, 50));
  world::Material red;
  red.baseColor = {1, 0, 0, 1};

  auto describe = [&](float x) {
    return world::scene::group().key("root").child(
        world::scene::group().key("rig").at({x, 0, 0}).child(
            world::scene::surface(mesh, red).key("card")));
  };

  world::scene::Scene scene(*w);
  world::scene::Scene::Stats first = scene.render(describe(0));
  EXPECT_EQ(first.added, 1);
  EXPECT_EQ(w->surfaceCount(), 1u);

  // Same description: everything kept, nothing touched.
  world::scene::Scene::Stats second = scene.render(describe(0));
  EXPECT_EQ(second.added, 0);
  EXPECT_EQ(second.kept, 1);
  EXPECT_EQ(second.moved, 0);

  // A parent transform change is a move, not a rebuild.
  world::scene::Scene::Stats third = scene.render(describe(40));
  EXPECT_EQ(third.added, 0);
  EXPECT_EQ(third.moved, 1);
  EXPECT_EQ(w->surfaceCount(), 1u);

  // A material change rebuilds that one surface.
  world::Material blue = red;
  blue.baseColor = {0, 0, 1, 1};
  world::scene::Node swapped = world::scene::group().key("root").child(
      world::scene::group()
          .key("rig")
          .at({40, 0, 0})
          .child(world::scene::surface(mesh, blue).key("card")));
  world::scene::Scene::Stats fourth = scene.render(swapped);
  EXPECT_EQ(fourth.removed, 1);
  EXPECT_EQ(fourth.added, 1);

  // Dropping the leaf removes its surface.
  world::scene::Scene::Stats fifth =
      scene.render(world::scene::group().key("root"));
  EXPECT_EQ(fifth.removed, 1);
  EXPECT_EQ(w->surfaceCount(), 0u);
}

TEST(Scene, PanelsAreIdentityStable) {
  world::WorldConfig config;
  config.width = 96;
  config.height = 64;
  MAKE_WORLD_OR_SKIP(w, config);

  sk_sp<SkImage> image;
  {
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(16, 16));
    surface->getCanvas()->clear(SK_ColorGREEN);
    image = surface->makeImageSnapshot();
  }
  world::scene::Scene scene(*w);
  auto describe = [&](float y) {
    return world::scene::group().child(
        world::scene::panel(image, 100, 60).key("hud").at({0, y, 0}));
  };
  EXPECT_EQ(scene.render(describe(0)).added, 1);
  // Panels resolve to a cached quad per size: re-describing must not
  // re-upload.
  world::scene::Scene::Stats stats = scene.render(describe(25));
  EXPECT_EQ(stats.added, 0);
  EXPECT_EQ(stats.moved, 1);
}

TEST(World, PointLightBrightensAndTintsItsSide) {
  world::WorldConfig config;
  config.width = 200;
  config.height = 100;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);

  shape::space::Camera camera;
  camera.eye = {0, 0, 400};
  w->setCamera(camera);

  // Kill the sun and ambient so the point light is the only source.
  world::Lighting dark;
  dark.sunIntensity = 0;
  dark.ambient = 0;
  w->setLighting(dark);

  world::Material material;
  material.baseColor = {0.9f, 0.9f, 0.9f, 1};
  material.roughness = 0.8f;
  ASSERT_NE(
      w->addSurface(shape::mesh::quad(600, 300), glm::mat4(1.0f), material),
      0u);

  // A red point light hovering near the quad's LEFT half.
  world::LightComponent light;
  light.type = world::LightComponent::Type::Point;
  light.color = {1, 0.1f, 0.1f, 1};
  light.intensity = 4;
  light.position = {-150, 0, 60};
  light.range = 260;
  ASSERT_NE(w->addLight(light), 0u);

  ASSERT_TRUE(w->render());
  sk_sp<SkImage> frame = w->readback();
  ASSERT_TRUE(frame);
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(200, 100));
  ASSERT_TRUE(frame->readPixels(nullptr, bm.pixmap(), 0, 0));
  // Left of center (under the light): lit, red-dominant. Right edge:
  // outside the falloff radius, still black.
  const SkColor lit = bm.getColor(50, 50);
  const SkColor darkSide = bm.getColor(190, 50);
  EXPECT_GT(SkColorGetR(lit), 90u);
  EXPECT_GT(SkColorGetR(lit), SkColorGetG(lit) + 30u);
  EXPECT_LT(SkColorGetR(darkSide), 25u);

  // Registry mutation is LIVE: move the light to the right half.
  entt::registry& registry = w->registry();
  for (auto [e, l] : registry.view<world::LightComponent>().each())
    l.position.x = 150;
  ASSERT_TRUE(w->render());
  frame = w->readback();
  ASSERT_TRUE(frame);
  ASSERT_TRUE(frame->readPixels(nullptr, bm.pixmap(), 0, 0));
  EXPECT_GT(SkColorGetR(bm.getColor(150, 50)), 90u);
  EXPECT_LT(SkColorGetR(bm.getColor(10, 50)), 25u);
}

TEST(World, ActiveCameraComponentOverridesSetCamera) {
  world::WorldConfig config;
  config.width = 100;
  config.height = 100;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);

  // The fallback camera looks AWAY from the quad.
  shape::space::Camera away;
  away.eye = {0, 0, 400};
  away.target = {0, 0, 800};
  w->setCamera(away);

  world::Material material;
  material.unlit = true;
  material.baseColor = {0, 1, 0, 1};
  ASSERT_NE(
      w->addSurface(shape::mesh::quad(200, 200), glm::mat4(1.0f), material),
      0u);

  // An active camera entity faces it; that one must drive the frame.
  shape::space::Camera facing;
  facing.eye = {0, 0, 400};
  facing.target = {0, 0, 0};
  entt::registry& registry = w->registry();
  const entt::entity camEntity = registry.create();
  registry.emplace<world::CameraComponent>(camEntity,
                                           world::CameraComponent{facing});

  auto greenPixels = [&]() {
    sk_sp<SkImage> frame = w->readback();
    if (!frame) return -1;
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(100, 100));
    if (!frame->readPixels(nullptr, bm.pixmap(), 0, 0)) return -1;
    int count = 0;
    for (int y = 0; y < 100; ++y)
      for (int x = 0; x < 100; ++x)
        if (SkColorGetG(bm.getColor(x, y)) > 100) ++count;
    return count;
  };

  ASSERT_TRUE(w->render());
  EXPECT_GT(greenPixels(), 1000);

  // Deactivating falls back to setCamera — the quad leaves the frame.
  registry.get<world::CameraComponent>(camEntity).active = false;
  ASSERT_TRUE(w->render());
  EXPECT_EQ(greenPixels(), 0);
}

TEST(World, InstancedFieldRendersAndCounts) {
  world::WorldConfig config;
  config.width = 200;
  config.height = 150;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);

  shape::space::Camera camera;
  camera.eye = {0, 0, 500};
  w->setCamera(camera);

  // A lattice of small quads spanning the middle of the frame, tinted
  // red through the tint lane over a white unlit material.
  shape::Cloud field =
      shape::points::grid({-150, -100, 0}, {300, 0, 0}, {0, 200, 0}, 10, 8);
  field.color("tint", {1, 0, 0, 1});
  world::Material material;
  material.unlit = true;
  material.baseColor = {1, 1, 1, 1};
  world::InstanceLanes lanes;
  lanes.tintLane = "tint";
  const uint32_t id =
      w->addInstanced(shape::mesh::quad(14, 14), field, material, lanes);
  ASSERT_NE(id, 0u);
  // The whole flock is ONE surface.
  EXPECT_EQ(w->surfaceCount(), 1u);

  auto redPixels = [&](int* quadrants) {
    sk_sp<SkImage> frame = w->readback();
    if (!frame) return -1;
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(200, 150));
    if (!frame->readPixels(nullptr, bm.pixmap(), 0, 0)) return -1;
    int count = 0;
    for (int i = 0; i < 4; ++i) quadrants[i] = 0;
    for (int y = 0; y < 150; ++y)
      for (int x = 0; x < 200; ++x) {
        const SkColor c = bm.getColor(x, y);
        if (SkColorGetR(c) > 120 && SkColorGetG(c) < 60) {
          ++count;
          ++quadrants[(y < 75 ? 0 : 2) + (x < 100 ? 0 : 1)];
        }
      }
    return count;
  };

  ASSERT_TRUE(w->render());
  int quadrants[4] = {};
  const int covered = redPixels(quadrants);
  EXPECT_GT(covered, 400) << "the field must render as many stamps";
  for (int i = 0; i < 4; ++i)
    EXPECT_GT(quadrants[i], 0) << "instances must spread quadrant " << i;

  // setInstances with a different count recreates the stream.
  shape::Cloud fewer =
      shape::points::grid({-150, -100, 0}, {300, 0, 0}, {0, 200, 0}, 2, 2);
  fewer.color("tint", {1, 0, 0, 1});
  w->setInstances(id, fewer, lanes);
  ASSERT_TRUE(w->render());
  const int sparse = redPixels(quadrants);
  EXPECT_GT(sparse, 0);
  EXPECT_LT(sparse, covered);

  // removeSurface tears the flock down like any surface.
  w->removeSurface(id);
  EXPECT_EQ(w->surfaceCount(), 0u);
  ASSERT_TRUE(w->render());
  EXPECT_EQ(redPixels(quadrants), 0);
}

TEST(World, SurfacesAreEntities) {
  world::WorldConfig config;
  config.width = 100;
  config.height = 100;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);

  shape::space::Camera camera;
  camera.eye = {0, 0, 500};
  w->setCamera(camera);
  world::Material material;
  material.unlit = true;
  material.baseColor = {0, 1, 0, 1};
  const uint32_t id =
      w->addSurface(shape::mesh::quad(100, 100), glm::mat4(1.0f), material);
  ASSERT_NE(id, 0u);

  // The id is an entity carrying the public components.
  entt::registry& registry = w->registry();
  const entt::entity e = world::entity(id);
  ASSERT_TRUE(registry.valid(e));
  ASSERT_TRUE((
      registry.all_of<world::TransformComponent, world::MaterialComponent>(e)));

  // Systems attach their own components alongside rendering state.
  struct Velocity {
    float y;
  };
  registry.emplace<Velocity>(e, Velocity{150.0f});
  for (auto [entity, velocity, transform] :
       registry.view<Velocity, world::TransformComponent>().each())
    transform.model = glm::translate(glm::mat4(1.0f), {0, velocity.y, 0});

  // The registry mutation IS the scene: the quad renders high.
  ASSERT_TRUE(w->render());
  sk_sp<SkImage> frame = w->readback();
  ASSERT_TRUE(frame);
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(100, 100));
  ASSERT_TRUE(frame->readPixels(nullptr, bm.pixmap(), 0, 0));
  int top = 0, bottom = 0;
  for (int y = 0; y < 100; ++y)
    for (int x = 0; x < 100; ++x)
      if (SkColorGetG(bm.getColor(x, y)) > 100) (y < 50 ? top : bottom)++;
  EXPECT_GT(top, 0);
  EXPECT_EQ(bottom, 0);
}

TEST(Easel, CommitReconciles) {
  world::WorldConfig config;
  config.width = 96;
  config.height = 64;
  MAKE_WORLD_OR_SKIP(w, config);

  world::easel::Stage stage = world::easel::stage(*w);
  world::Material red;
  red.baseColor = {1, 0, 0, 1};

  auto describe = [&](float x) {
    stage.sun({-0.4f, -0.8f, -0.5f}, 2.0f)
        .light({0, 120, 60}, {0.2f, 0.9f, 1.0f, 1}, 3)
        .place(shape::mesh::quad(50, 50), red)
        .key("card")
        .at({x, 0, 0})
        .turned(15);
    return stage.commit();
  };

  // First commit: the card and the light arrive.
  world::scene::Scene::Stats first = describe(0);
  EXPECT_EQ(first.added, 2);
  EXPECT_EQ(w->surfaceCount(), 1u);
  EXPECT_EQ(w->registry().view<world::LightComponent>().size(), 1u);
  ASSERT_TRUE(w->render());

  // Same description: by-value mesh content-hashes to the same
  // identity, so everything is kept — nothing re-uploads.
  world::scene::Scene::Stats second = describe(0);
  EXPECT_EQ(second.added, 0);
  EXPECT_EQ(second.moved, 0);
  EXPECT_EQ(second.kept, 2);
  EXPECT_EQ(w->surfaceCount(), 1u);

  // A moved .at() is a move, not a rebuild; the light stays kept.
  world::scene::Scene::Stats third = describe(40);
  EXPECT_EQ(third.added, 0);
  EXPECT_EQ(third.moved, 1);
  EXPECT_EQ(third.kept, 1);
  EXPECT_EQ(w->surfaceCount(), 1u);

  // An empty description clears the stage's things from the world.
  world::scene::Scene::Stats fourth = stage.commit();
  EXPECT_EQ(fourth.removed, 2);
  EXPECT_EQ(w->surfaceCount(), 0u);
  EXPECT_EQ(w->registry().view<world::LightComponent>().size(), 0u);
}

TEST(Easel, SwarmsAreKeyStable) {
  world::WorldConfig config;
  config.width = 96;
  config.height = 64;
  MAKE_WORLD_OR_SKIP(w, config);

  world::easel::Stage stage = world::easel::stage(*w);
  world::Material glow;
  glow.unlit = true;

  auto describe = [&](float y) {
    shape::Cloud cloud =
        shape::points::grid({-40, -20, 0}, {80, 0, 0}, {0, 40, 0}, 6, 4);
    stage.swarm(cloud, shape::mesh::quad(5, 5), glow)
        .key("sparks")
        .at({0, y, 0});
    return stage.commit();
  };

  // One flock, one surface.
  world::scene::Scene::Stats first = describe(0);
  EXPECT_EQ(first.added, 1);
  EXPECT_EQ(w->surfaceCount(), 1u);
  ASSERT_TRUE(w->render());

  // Unchanged cloud + placement: kept, no re-upload.
  world::scene::Scene::Stats second = describe(0);
  EXPECT_EQ(second.added, 0);
  EXPECT_EQ(second.kept, 1);

  // Moving the flock is a transform touch, not a rebuild.
  world::scene::Scene::Stats third = describe(30);
  EXPECT_EQ(third.added, 0);
  EXPECT_EQ(third.moved, 1);
  EXPECT_EQ(w->surfaceCount(), 1u);

  // Dropping it removes the instanced surface.
  world::scene::Scene::Stats fourth = stage.commit();
  EXPECT_EQ(fourth.removed, 1);
  EXPECT_EQ(w->surfaceCount(), 0u);
}

TEST(Easel, SwarmCloudEditsRefreshInstancesInPlace) {
  // The case SwarmsAreKeyStable leaves untested: the SAME key with
  // CHANGED cloud content must refresh the instance stream in place,
  // never tear the surface down and re-add it.
  world::WorldConfig config;
  config.width = 96;
  config.height = 64;
  MAKE_WORLD_OR_SKIP(w, config);

  world::easel::Stage stage = world::easel::stage(*w);
  world::Material glow;
  glow.unlit = true;

  auto describe = [&](float originX) {
    shape::Cloud cloud = shape::points::grid({originX - 40, -20, 0}, {80, 0, 0},
                                             {0, 40, 0}, 6, 4);
    stage.swarm(cloud, shape::mesh::quad(5, 5), glow).key("sparks");
    return stage.commit();
  };

  world::scene::Scene::Stats first = describe(0);
  EXPECT_EQ(first.added, 1);
  EXPECT_EQ(w->surfaceCount(), 1u);
  ASSERT_TRUE(w->render());

  // The grid's origin slid +15 in x: a content edit, not a rebuild.
  world::scene::Scene::Stats second = describe(15);
  EXPECT_EQ(second.added, 0);
  EXPECT_EQ(second.removed, 0);
  EXPECT_EQ(second.moved, 1);
  EXPECT_EQ(w->surfaceCount(), 1u);
  ASSERT_TRUE(w->render());
}

TEST(Easel, PointsReconcileByChainValue) {
  // A declared point chain is a keep while its value stands, a move
  // when a parameter (the window) changes, and a re-add when its stamp
  // changes; an undeclared one leaves.
  world::WorldConfig config;
  config.width = 64;
  config.height = 64;
  MAKE_WORLD_OR_SKIP(w, config);
  world::easel::Stage stage(*w);
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.push_back({150.0f * std::cos(a), 0, 150.0f * std::sin(a)});
  }
  const shape::Mesh stamp = shape::mesh::quad(6, 6);
  const auto declare = [&](float head, const shape::Mesh& withStamp) {
    return stage
        .points(shape::pop::on(loop).count(300).window(head, 0.5f).noise(9),
                withStamp, world::Material{})
        .key("comet")
        .commit();
  };
  world::scene::Scene::Stats stats = declare(1.0f, stamp);
  EXPECT_EQ(stats.added, 1);
  ASSERT_EQ(w->surfaceCount(), 1u);
  stats = declare(1.0f, stamp);
  EXPECT_EQ(stats.kept, 1);
  EXPECT_EQ(stats.added + stats.removed, 0);
  stats = declare(0.4f, stamp);
  EXPECT_EQ(stats.moved, 1) << "a window slide is a chain edit";
  EXPECT_EQ(stats.added + stats.removed, 0);
  stats = declare(0.4f, shape::mesh::quad(9, 9));
  EXPECT_EQ(stats.added, 1) << "a new stamp is a rebuild";
  EXPECT_EQ(stats.removed, 1);
  stats = stage.commit();  // nothing declared: it leaves
  EXPECT_EQ(stats.removed, 1);
  EXPECT_EQ(w->surfaceCount(), 0u);
}

TEST(World, UvScaleOffsetSelectsTexelLiveAcrossFrames) {
  // uvScale {0,0} collapses sampling to the single texel uvOffset names,
  // which makes this independent of filtering and of the quad's
  // orientation. Mutating the LIVE MaterialComponent between frames must
  // move that texel: scrolling content across a surface with no texture
  // uploads at all.
  world::WorldConfig config;
  config.width = 64;
  config.height = 64;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);
  shape::space::Camera camera;
  camera.eye = {0, 0, 500};
  camera.target = {0, 0, 0};
  w->setCamera(camera);

  SkBitmap texels;  // green | blue, texel centers u = 0.25 / 0.75
  texels.allocPixels(SkImageInfo::MakeN32Premul(2, 1));
  texels.eraseColor(SK_ColorGREEN);
  texels.erase(SK_ColorBLUE, SkIRect::MakeXYWH(1, 0, 1, 1));
  texels.setImmutable();

  world::Material screen;
  screen.unlit = true;
  screen.baseColor = {1, 1, 1, 1};
  screen.texture = texels.asImage();
  screen.uvScale = {0, 0};
  screen.uvOffset = {0.25f, 0.5f};
  const uint32_t id =
      w->addSurface(shape::mesh::quad(400, 400), glm::mat4(1.0f), screen);
  ASSERT_NE(id, 0u);

  const auto centerColor = [&]() {
    sk_sp<SkImage> frame = w->readback();
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(64, 64));
    if (!frame || !frame->readPixels(nullptr, bm.pixmap(), 0, 0))
      return SK_ColorTRANSPARENT;
    return bm.getColor(32, 32);
  };

  ASSERT_TRUE(w->render());
  const SkColor first = centerColor();
  EXPECT_GT(SkColorGetG(first), 128u);
  EXPECT_LT(SkColorGetB(first), 100u);

  auto& material =
      w->registry().get<world::MaterialComponent>(world::entity(id));
  material.material.uvOffset = {0.75f, 0.5f};
  ASSERT_TRUE(w->render());
  const SkColor second = centerColor();
  EXPECT_GT(SkColorGetB(second), 128u);
  EXPECT_LT(SkColorGetG(second), 100u);
}

TEST(World, UnlitSrgbTexelSurvivesTheRoundTrip) {
  // The transfer function, both ways round. Panel textures are uploaded
  // RGBA8_UNORM_SRGB, so the sampler DECODES with the piecewise sRGB
  // curve, and the render target is plain UNORM, so the shader ENCODES
  // by hand. Those two must be exact inverses, or an unlit panel — the
  // pure pass-through path — does not pass through. A pow(c, 1/2.2)
  // encode is not the inverse and fails this test through the darks.
  //
  // uvScale {0,0} collapses sampling to the single texel uvOffset names,
  // so the result depends on neither filtering nor geometry: whatever
  // byte goes in must come back out, within the 1/255 the UNORM target
  // rounds to.
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);
  shape::space::Camera camera;
  camera.eye = {0, 0, 500};
  camera.target = {0, 0, 0};
  w->setCamera(camera);

  // Greys across the range, weighted toward the dark end, where a power
  // curve and the piecewise standard curve diverge hardest — the
  // standard curve's linear toe lives down there.
  const uint8_t levels[] = {2, 8, 24, 55, 96, 128, 170, 210, 250};
  const int count = (int)(sizeof(levels) / sizeof(levels[0]));
  SkBitmap texels;
  texels.allocPixels(SkImageInfo::MakeN32Premul(count, 1));
  for (int i = 0; i < count; ++i)
    texels.erase(SkColorSetARGB(255, levels[i], levels[i], levels[i]),
                 SkIRect::MakeXYWH(i, 0, 1, 1));
  texels.setImmutable();

  world::Material screen;
  screen.unlit = true;
  screen.baseColor = {1, 1, 1, 1};  // no tint: the texel, undiluted
  screen.texture = texels.asImage();
  screen.uvScale = {0, 0};
  screen.uvOffset = {0.5f / (float)count, 0.5f};
  const uint32_t id =
      w->addSurface(shape::mesh::quad(600, 600), glm::mat4(1.0f), screen);
  ASSERT_NE(id, 0u);
  auto& material =
      w->registry().get<world::MaterialComponent>(world::entity(id));

  for (int i = 0; i < count; ++i) {
    material.material.uvOffset = {((float)i + 0.5f) / (float)count, 0.5f};
    ASSERT_TRUE(w->render());
    sk_sp<SkImage> frame = w->readback();
    ASSERT_TRUE(frame);
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(32, 32));
    ASSERT_TRUE(frame->readPixels(nullptr, bm.pixmap(), 0, 0));
    const SkColor c = bm.getColor(16, 16);
    EXPECT_NEAR((int)SkColorGetR(c), (int)levels[i], 1)
        << "level " << (int)levels[i] << " R";
    EXPECT_NEAR((int)SkColorGetG(c), (int)levels[i], 1)
        << "level " << (int)levels[i] << " G";
    EXPECT_NEAR((int)SkColorGetB(c), (int)levels[i], 1)
        << "level " << (int)levels[i] << " B";
  }
}

TEST(World, SetSurfaceMeshMovesGeometryInPlace) {
  // Same topology updates the GPU buffers in place; a different shape
  // recreates them; the material and the entity survive both.
  world::WorldConfig config;
  config.width = 100;
  config.height = 100;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);
  shape::space::Camera camera;
  camera.eye = {0, 0, 500};
  camera.target = {0, 0, 0};
  w->setCamera(camera);
  world::Material material;
  material.unlit = true;
  material.baseColor = {0, 1, 0, 1};
  const uint32_t id =
      w->addSurface(shape::mesh::quad(100, 100), glm::mat4(1.0f), material);
  ASSERT_NE(id, 0u);

  const auto greenRows = [&](int* top, int* bottom) {
    sk_sp<SkImage> frame = w->readback();
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(100, 100));
    ASSERT_TRUE(frame && frame->readPixels(nullptr, bm.pixmap(), 0, 0));
    *top = *bottom = 0;
    for (int y = 0; y < 100; ++y)
      for (int x = 0; x < 100; ++x)
        if (SkColorGetG(bm.getColor(x, y)) > 100)
          (y < 50 ? *top : *bottom) += 1;
  };

  ASSERT_TRUE(w->render());
  int top = 0, bottom = 0;
  greenRows(&top, &bottom);
  EXPECT_GT(top, 0);
  EXPECT_GT(bottom, 0);  // centered quad spans both halves

  // Same topology, vertices moved up: the in-place update path.
  shape::Mesh moved = shape::mesh::quad(100, 100);
  moved.transform(glm::translate(glm::mat4(1.0f), {0, 150, 0}));
  w->setSurfaceMesh(id, moved);
  ASSERT_TRUE(w->render());
  greenRows(&top, &bottom);
  EXPECT_GT(top, 0);
  EXPECT_EQ(bottom, 0) << "geometry must have moved to the top half";

  // Different topology: the recreate path still renders.
  w->setSurfaceMesh(id, shape::mesh::torus(80, 24));
  ASSERT_TRUE(w->render());
  greenRows(&top, &bottom);
  EXPECT_GT(top + bottom, 0);
}

TEST(World, GpuSweepGeneratesAndSlidesOnTheGpu) {
  // The compute generator path: a circular loop lives on the GPU, the
  // sweep writes the ribbon's vertices in place, and sliding the window
  // with two floats moves the arc to the other side. No CPU mesh for the
  // ribbon ever exists.
  world::WorldConfig config;
  config.width = 64;
  config.height = 64;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);
  shape::space::Camera camera;
  camera.eye = {0, 0, 500};
  camera.target = {0, 0, 0};
  w->setCamera(camera);

  world::World::SweepDesc desc;
  for (int i = 0; i < 12; ++i) {
    const float a = (float)i / 12.0f * 2.0f * (float)M_PI;
    desc.loop.push_back({160.0f * std::cos(a), 160.0f * std::sin(a), 0});
  }
  desc.width = 46;
  desc.sections = 64;
  desc.head = 0.25f;  // a quarter arc ending at the loop's top-right
  desc.span = 0.25f;
  world::Material material;
  material.unlit = true;
  material.baseColor = {0, 1, 0, 1};
  const uint32_t id = w->addSweep(desc, material);
  ASSERT_NE(id, 0u);

  const auto greenHalves = [&](int* left, int* right) {
    sk_sp<SkImage> frame = w->readback();
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(64, 64));
    ASSERT_TRUE(frame && frame->readPixels(nullptr, bm.pixmap(), 0, 0));
    *left = *right = 0;
    for (int y = 0; y < 64; ++y)
      for (int x = 0; x < 64; ++x)
        if (SkColorGetG(bm.getColor(x, y)) > 100)
          (x < 32 ? *left : *right) += 1;
  };

  ASSERT_TRUE(w->render());
  int left = 0, right = 0;
  greenHalves(&left, &right);
  EXPECT_GT(right, 0) << "quarter arc t in [0, 0.25] starts at +x";
  EXPECT_EQ(left, 0);

  w->setSweepWindow(id, 0.75f, 0.25f);  // the opposite quarter
  ASSERT_TRUE(w->render());
  greenHalves(&left, &right);
  EXPECT_GT(left, 0) << "slid window must land on the -x side";
  EXPECT_EQ(right, 0);
}

TEST(World, GpuFlockStreamsAlongTheLoop) {
  // A flock's points never exist on the CPU. A window of instances lands
  // on one side of the loop; sliding it with two floats streams the
  // whole flock to the other side.
  world::WorldConfig config;
  config.width = 64;
  config.height = 64;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);
  shape::space::Camera camera;
  camera.eye = {0, 0, 500};
  camera.target = {0, 0, 0};
  w->setCamera(camera);

  world::World::FlockDesc desc;
  for (int i = 0; i < 12; ++i) {
    const float a = (float)i / 12.0f * 2.0f * (float)M_PI;
    desc.loop.push_back({160.0f * std::cos(a), 160.0f * std::sin(a), 0});
  }
  desc.count = 3000;
  desc.head = 0.25f;
  desc.span = 0.25f;
  desc.radius = 0;  // points exactly on the arc: a deterministic side
  desc.scale = 1;
  world::Material material;
  material.unlit = true;
  material.baseColor = {0, 1, 0, 1};
  const uint32_t id = w->addFlock(shape::mesh::quad(10, 10), desc, material);
  ASSERT_NE(id, 0u);

  const auto greenHalves = [&](int* left, int* right) {
    sk_sp<SkImage> frame = w->readback();
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(64, 64));
    ASSERT_TRUE(frame && frame->readPixels(nullptr, bm.pixmap(), 0, 0));
    *left = *right = 0;
    for (int y = 0; y < 64; ++y)
      for (int x = 0; x < 64; ++x)
        if (SkColorGetG(bm.getColor(x, y)) > 100)
          (x < 32 ? *left : *right) += 1;
  };

  ASSERT_TRUE(w->render());
  int left = 0, right = 0;
  greenHalves(&left, &right);
  EXPECT_GT(right, 0) << "the window t in [0, 0.25] starts at +x";
  EXPECT_EQ(left, 0);

  w->setFlockWindow(id, 0.75f, 0.25f);
  ASSERT_TRUE(w->render());
  greenHalves(&left, &right);
  EXPECT_GT(left, 0) << "slid flock must stream to the -x side";
  EXPECT_EQ(right, 0);
}

TEST(World, PopChainCooksAndRedescribes) {
  // A chain of operator VALUES cooks on the GPU, and re-describing with
  // edited fields re-cooks. The appended Math op mirrors the window's
  // points across the frame, so seeing them move proves the whole chain
  // ran rather than just its generator.
  world::WorldConfig config;
  config.width = 64;
  config.height = 64;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);
  shape::space::Camera camera;
  camera.eye = {0, 0, 500};
  camera.target = {0, 0, 0};
  w->setCamera(camera);

  using pop = world::World::pop;
  pop::SplineScatter scatter;
  for (int i = 0; i < 12; ++i) {
    const float a = (float)i / 12.0f * 2.0f * (float)M_PI;
    scatter.loop.push_back({160.0f * std::cos(a), 160.0f * std::sin(a), 0});
  }
  scatter.count = 3000;
  scatter.head = 0.125f;  // an arc clear of the screen midline
  scatter.span = 0.2f;
  scatter.radius = 0;
  pop::Chain chain = {scatter, pop::LookAt{{0, 0, 500}},
                      pop::Ramp{pop::Lane::Color, {0, 1, 0, 1}, {0, 1, 0, 1}}};
  world::Material material;
  material.unlit = true;
  material.baseColor = {1, 1, 1, 1};  // tint carries the green
  const uint32_t id = w->addPoints(shape::mesh::quad(10, 10), chain, material);
  ASSERT_NE(id, 0u);

  const auto greenHalves = [&](int* left, int* right) {
    sk_sp<SkImage> frame = w->readback();
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(64, 64));
    ASSERT_TRUE(frame && frame->readPixels(nullptr, bm.pixmap(), 0, 0));
    *left = *right = 0;
    for (int y = 0; y < 64; ++y)
      for (int x = 0; x < 64; ++x)
        if (SkColorGetG(bm.getColor(x, y)) > 100)
          (x < 32 ? *left : *right) += 1;
  };

  ASSERT_TRUE(w->render());
  int left = 0, right = 0;
  greenHalves(&left, &right);
  EXPECT_GT(right, 0) << "the scatter window starts on +x";
  EXPECT_EQ(left, 0);

  // A nondestructive edit: append a Math op mirroring positions across
  // x. The chain's shape changed, so the lanes rebind before the re-cook
  // runs it.
  chain.push_back(pop::Math{pop::Lane::P, {-1, 1, 1, 1}, {0, 0, 0, 0}});
  w->setPoints(id, chain);
  ASSERT_TRUE(w->render());
  greenHalves(&left, &right);
  EXPECT_GT(left, 0) << "Math mul {-1,1,1} must mirror the arc to -x";
  EXPECT_EQ(right, 0);
}

TEST(World, PopCpuAndGpuExecutorsAgree) {
  // Two executors of one description: the GPU chain, cooking compute
  // lanes into an instanced draw, and the CPU reference, cooking a plain
  // mesh, must land the same geometry. The material carries the colour
  // and no tint lane is used, so the comparison is purely about
  // position, direction and scale.
  world::WorldConfig config;
  config.width = 64;
  config.height = 64;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);
  shape::space::Camera camera;
  camera.eye = {0, 0, 500};
  camera.target = {0, 0, 0};
  w->setCamera(camera);

  std::vector<glm::vec3> loop;
  for (int i = 0; i < 12; ++i) {
    const float a = (float)i / 12.0f * 2.0f * (float)M_PI;
    loop.push_back({160.0f * std::cos(a), 160.0f * std::sin(a), 0});
  }
  const shape::pop::Chain chain = shape::pop::on(loop)
                                      .count(200)
                                      .window(0.125f, 0.6f)
                                      .spread(8)
                                      .seed(9)
                                      .vary(0.35f)
                                      .smooth(0.5f, 2)
                                      .lookAt({0, 0, 500});
  const shape::Mesh stamp = shape::mesh::quad(9, 9);
  world::Material material;
  material.unlit = true;
  material.baseColor = {0, 1, 0, 1};

  const auto mask = [&](std::vector<bool>* out) {
    sk_sp<SkImage> frame = w->readback();
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(64, 64));
    ASSERT_TRUE(frame && frame->readPixels(nullptr, bm.pixmap(), 0, 0));
    out->assign(64 * 64, false);
    for (int y = 0; y < 64; ++y)
      for (int x = 0; x < 64; ++x)
        (*out)[(size_t)(y * 64 + x)] = SkColorGetG(bm.getColor(x, y)) > 100;
  };

  const uint32_t gpu = w->addPoints(stamp, chain, material);
  ASSERT_NE(gpu, 0u);
  ASSERT_TRUE(w->render());
  std::vector<bool> gpuMask;
  mask(&gpuMask);
  w->removeSurface(gpu);

  const shape::Mesh cpuModel = shape::popops::cookMesh(chain, stamp);
  ASSERT_NE(w->addSurface(cpuModel, glm::mat4(1.0f), material), 0u);
  ASSERT_TRUE(w->render());
  std::vector<bool> cpuMask;
  mask(&cpuMask);

  int lit = 0, mismatched = 0;
  for (size_t i = 0; i < gpuMask.size(); ++i) {
    if (gpuMask[i] || cpuMask[i]) ++lit;
    if (gpuMask[i] != cpuMask[i]) ++mismatched;
  }
  ASSERT_GT(lit, 100);
  // The two vertex paths — an instanced vertex shader versus a
  // pre-merged mesh — may disagree on edge pixels; the SHAPES must
  // agree.
  EXPECT_LT(mismatched, lit / 50)
      << mismatched << " of " << lit << " lit pixels disagree";
}

TEST(World, ReadPointsQueriesGpuLanesNumerically) {
  // The query door, and the strongest form of the parity claim: the
  // GPU-cooked lanes, read back as a Cloud, must match the CPU reference
  // cook point for point and number for number.
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  std::vector<glm::vec3> loop;
  for (int i = 0; i < 12; ++i) {
    const float a = (float)i / 12.0f * 2.0f * (float)M_PI;
    loop.push_back(
        {160.0f * std::cos(a), 30.0f * std::sin(3 * a), 160.0f * std::sin(a)});
  }
  const shape::pop::Chain chain = shape::pop::on(loop)
                                      .count(500)
                                      .window(0.8f, 0.6f)
                                      .spread(14)
                                      .seed(4)
                                      .vary(0.35f)
                                      .smooth(0.5f, 2)
                                      .atlas(2, 2)
                                      .fade({1, 0, 0, 1}, {0, 0, 1, 1});
  const uint32_t id =
      w->addPoints(shape::mesh::quad(4, 4), chain, world::Material{});
  ASSERT_NE(id, 0u);
  ASSERT_TRUE(w->render());  // the cook

  const shape::Cloud gpu = w->readPoints(id);
  const shape::Cloud cpu = shape::popops::cook(chain);
  ASSERT_EQ(gpu.size(), 500u);
  ASSERT_EQ(cpu.size(), 500u);
  const std::vector<float>* gpuT = gpu.scalarIf("t");
  const std::vector<glm::vec4>* gpuTint = gpu.colorIf("tint");
  const std::vector<float>* cpuT = cpu.scalarIf("t");
  const std::vector<glm::vec4>* cpuTint = cpu.colorIf("tint");
  const std::vector<glm::vec4>* gpuTex = gpu.colorIf("Tex");
  const std::vector<glm::vec4>* cpuTex = cpu.colorIf("Tex");
  ASSERT_TRUE(gpuT && gpuTint && cpuT && cpuTint && gpuTex && cpuTex);
  for (size_t i : {size_t(0), size_t(123), size_t(499)}) {
    EXPECT_NEAR(gpu.positions[i].x, cpu.positions[i].x, 0.02f);
    EXPECT_NEAR(gpu.positions[i].y, cpu.positions[i].y, 0.02f);
    EXPECT_NEAR(gpu.positions[i].z, cpu.positions[i].z, 0.02f);
    EXPECT_NEAR((*gpuT)[i], (*cpuT)[i], 1e-4f);
    EXPECT_NEAR((*gpuTint)[i].r, (*cpuTint)[i].r, 1e-3f);
    EXPECT_NEAR((*gpuTint)[i].b, (*cpuTint)[i].b, 1e-3f);
    EXPECT_NEAR((*gpuTex)[i].r, (*cpuTex)[i].r, 1e-4f);
    EXPECT_NEAR((*gpuTex)[i].g, (*cpuTex)[i].g, 1e-4f);
  }
}

TEST(World, SetPointsWindowSlidesLikeAFullRedescribe) {
  // Sliding the window with two floats must land exactly where a whole
  // re-describe carrying the same window lands: same kernels, same
  // seeds, no drift between the two ways of asking.
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  std::vector<glm::vec3> loop;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    loop.push_back(
        {140.0f * std::cos(a), 40.0f * std::sin(2 * a), 140.0f * std::sin(a)});
  }
  const auto chainAt = [&](float head, float span) {
    return (shape::pop::Chain)shape::pop::on(loop)
        .count(128)
        .window(head, span)
        .spread(9)
        .seed(6)
        .vary(0.3f);
  };

  const uint32_t id = w->addPoints(shape::mesh::quad(3, 3), chainAt(1.0f, 0.5f),
                                   world::Material{});
  ASSERT_NE(id, 0u);
  ASSERT_TRUE(w->render());
  const shape::Cloud before = w->readPoints(id);

  w->setPointsWindow(id, 0.35f, 0.2f);
  ASSERT_TRUE(w->render());
  const shape::Cloud slid = w->readPoints(id);

  w->setPoints(id, chainAt(0.35f, 0.2f));
  ASSERT_TRUE(w->render());
  const shape::Cloud redescribed = w->readPoints(id);

  ASSERT_EQ(slid.size(), 128u);
  ASSERT_EQ(redescribed.size(), 128u);
  float moved = 0;
  for (size_t i : {size_t(0), size_t(64), size_t(127)}) {
    EXPECT_NEAR(slid.positions[i].x, redescribed.positions[i].x, 1e-4f);
    EXPECT_NEAR(slid.positions[i].y, redescribed.positions[i].y, 1e-4f);
    EXPECT_NEAR(slid.positions[i].z, redescribed.positions[i].z, 1e-4f);
    moved += glm::length(slid.positions[i] - before.positions[i]);
  }
  EXPECT_GT(moved, 1.0f) << "the window must actually slide";
}

TEST(World, CustomAttributesCookOnTheGpu) {
  // A custom named attribute gets its own arena slot, cooks through the
  // GPU dispatches, and reads back equal to the CPU reference — so the
  // lane vocabulary is not limited to the builtin names.
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  std::vector<glm::vec3> loop;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    loop.push_back({150.0f * std::cos(a), 0, 150.0f * std::sin(a)});
  }
  const shape::pop::Chain chain =
      shape::pop::on(loop)
          .count(256)
          .set("energy", {0.5f, 0, 0, 0})
          .op(shape::pop::Jitter{"energy", 0.25f, 5})
          .op(shape::pop::Math{"energy", {2, 1, 1, 1}, {0, 0, 0, 0}});
  const uint32_t id =
      w->addPoints(shape::mesh::quad(4, 4), chain, world::Material{});
  ASSERT_NE(id, 0u)
      << "a chain with custom attribute lanes must cook on the GPU";
  ASSERT_TRUE(w->render());

  const shape::Cloud gpu = w->readPoints(id);
  const shape::Cloud cpu = shape::popops::cook(chain);
  const std::vector<glm::vec4>* gpuEnergy = gpu.colorIf("energy");
  const std::vector<glm::vec4>* cpuEnergy = cpu.colorIf("energy");
  ASSERT_TRUE(gpuEnergy && cpuEnergy);
  ASSERT_EQ(gpuEnergy->size(), 256u);
  for (size_t i : {size_t(0), size_t(77), size_t(255)})
    EXPECT_NEAR((*gpuEnergy)[i].r, (*cpuEnergy)[i].r, 1e-4f);
}

TEST(World, PrimitiveClassChainsAreDeclinedNotDropped) {
  // The GPU executor cooks the point class only. A chain carrying an
  // operator that writes the primitive class must be REFUSED outright:
  // dropping the operator would cook silently wrong geometry, and its
  // variant index maps to no compute pipeline at all.
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  std::vector<glm::vec3> loop;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    loop.push_back({150.0f * std::cos(a), 0, 150.0f * std::sin(a)});
  }
  const auto describe = [&](bool promote) {
    shape::pop::Builder b = shape::pop::on(loop);
    b.count(64).fade({1, 0, 0, 1}, {0, 0, 1, 1});
    if (promote) b.promote(shape::pop::Lane::Color);
    return (shape::pop::Chain)b;
  };
  const shape::Mesh stamp = shape::mesh::quad(6, 6);

  // The control: the same chain WITHOUT that operator cooks fine.
  const uint32_t plain =
      w->addPoints(stamp, describe(false), world::Material{});
  ASSERT_NE(plain, 0u);

  EXPECT_EQ(w->addPoints(stamp, describe(true), world::Material{}), 0u);
  const uint32_t upstream =
      w->addPointsOn(plain, stamp,
                     (shape::pop::Chain)shape::pop::on(std::vector<glm::vec3>{})
                         .count(32)
                         .promote(shape::pop::Lane::Color),
                     world::Material{});
  EXPECT_EQ(upstream, 0u) << "the composing entry declines too";

  // setPoints refuses the re-describe rather than half-applying it.
  w->setPoints(plain, describe(true));
  EXPECT_TRUE(w->render());

  // ...and the CPU executor still builds the primitive lanes the GPU
  // cannot, so the capability is not lost, only located.
  const shape::Mesh cpu = shape::popops::cookMesh(describe(true), stamp);
  EXPECT_TRUE(cpu.primIf("Color"));
}

TEST(World, EveryGpuOpMapsToItsOwnKernelAndAgreesWithTheCpu) {
  // A chain holding EVERY operator the GPU executor runs, read back and
  // compared to the CPU reference lane by lane. Operator variant index
  // maps to a compute pipeline through a table, so if any row of it were
  // off — or if any kernel drifted from its CPU twin — at least one lane
  // below diverges. Numbers only, no pixels: this is arithmetic, not
  // rendering.
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  std::vector<glm::vec3> loop;
  for (int i = 0; i < 12; ++i) {
    const float a = (float)i / 12.0f * 2.0f * (float)M_PI;
    loop.push_back(
        {170.0f * std::cos(a), 45.0f * std::sin(3 * a), 170.0f * std::sin(a)});
  }
  // SplineScatter, Jitter, Noise, Relax, Set, Math, Vary, LookAt, Atlas,
  // Ramp and Lookup — every operator with a kernel, each writing a lane
  // the readback below checks.
  const shape::pop::Chain chain =
      shape::pop::on(loop)
          .count(384)
          .window(0.85f, 0.7f)
          .spread(12)
          .seed(9)
          .jitter(6)
          .noise(14, 0.012f)
          .smooth(0.4f, 2)
          .set("energy", {0.5f, 0, 0, 0})
          .op(shape::pop::Math{"energy", {3, 1, 1, 1}, {0.25f, 0, 0, 0}})
          .vary(0.35f)
          .lookAt({0, 220, 0})
          .atlas(3, 2)
          .fade({1, 0.2f, 0, 1}, {0, 0.4f, 1, 1})
          .rampBy(shape::pop::Lane::P, 1,
                  {{0, 0, 0, 0}, {1, 0, 0, 0}, {4, 0, 0, 0}}, -60, 60, "heat");
  const uint32_t id =
      w->addPoints(shape::mesh::quad(4, 4), chain, world::Material{});
  ASSERT_NE(id, 0u) << "every op in this chain is GPU-executable";
  ASSERT_TRUE(w->render());  // the cook

  const shape::Cloud gpu = w->readPoints(id);
  const shape::Cloud cpu = shape::popops::cook(chain);
  ASSERT_EQ(gpu.size(), 384u);
  ASSERT_EQ(cpu.size(), 384u);
  const std::vector<float>* gpuT = gpu.scalarIf("t");
  const std::vector<float>* cpuT = cpu.scalarIf("t");
  const std::vector<glm::vec3>* gpuDir = gpu.vectorIf("dir");
  const std::vector<glm::vec3>* cpuDir = cpu.vectorIf("dir");
  const std::vector<float>* gpuSize = gpu.scalarIf("size");
  const std::vector<float>* cpuSize = cpu.scalarIf("size");
  const std::vector<glm::vec4>* gpuTint = gpu.colorIf("tint");
  const std::vector<glm::vec4>* cpuTint = cpu.colorIf("tint");
  const std::vector<glm::vec4>* gpuTex = gpu.colorIf("Tex");
  const std::vector<glm::vec4>* cpuTex = cpu.colorIf("Tex");
  const std::vector<glm::vec4>* gpuEnergy = gpu.colorIf("energy");
  const std::vector<glm::vec4>* cpuEnergy = cpu.colorIf("energy");
  const std::vector<glm::vec4>* gpuHeat = gpu.colorIf("heat");
  const std::vector<glm::vec4>* cpuHeat = cpu.colorIf("heat");
  ASSERT_TRUE(gpuT && cpuT && gpuDir && cpuDir && gpuSize && cpuSize);
  ASSERT_TRUE(gpuTint && cpuTint && gpuTex && cpuTex);
  ASSERT_TRUE(gpuEnergy && cpuEnergy && gpuHeat && cpuHeat);

  float heatSpan = 0;
  for (size_t i = 0; i < gpu.size(); i += 37) {
    EXPECT_NEAR(gpu.positions[i].x, cpu.positions[i].x, 0.02f) << i;
    EXPECT_NEAR(gpu.positions[i].y, cpu.positions[i].y, 0.02f) << i;
    EXPECT_NEAR(gpu.positions[i].z, cpu.positions[i].z, 0.02f) << i;
    EXPECT_NEAR((*gpuT)[i], (*cpuT)[i], 1e-4f) << i;
    EXPECT_NEAR((*gpuDir)[i].x, (*cpuDir)[i].x, 1e-3f) << i;
    EXPECT_NEAR((*gpuDir)[i].y, (*cpuDir)[i].y, 1e-3f) << i;
    EXPECT_NEAR((*gpuSize)[i], (*cpuSize)[i], 1e-4f) << i;
    EXPECT_NEAR((*gpuTint)[i].r, (*cpuTint)[i].r, 1e-3f) << i;
    EXPECT_NEAR((*gpuTint)[i].b, (*cpuTint)[i].b, 1e-3f) << i;
    EXPECT_NEAR((*gpuTex)[i].r, (*cpuTex)[i].r, 1e-4f) << i;
    EXPECT_NEAR((*gpuTex)[i].g, (*cpuTex)[i].g, 1e-4f) << i;
    EXPECT_NEAR((*gpuEnergy)[i].r, (*cpuEnergy)[i].r, 1e-4f) << i;
    // The lookup lane: agreement AND a real spread, so a kernel that
    // wrote a constant could not pass by matching a constant.
    EXPECT_NEAR((*gpuHeat)[i].r, (*cpuHeat)[i].r, 1e-4f) << i;
    heatSpan = std::max(heatSpan, std::abs((*gpuHeat)[i].r - (*gpuHeat)[0].r));
  }
  EXPECT_GT(heatSpan, 0.5f) << "the lookup must vary across the cloud";

  // A table EDIT is a re-describe: setPoints must notice the stops
  // moved even though every op kind lines up, and re-cook against the
  // new table rather than the buffer it uploaded first.
  shape::pop::Chain edited = chain;
  for (shape::pop::Op& op : edited)
    if (auto* lookup = std::get_if<shape::pop::Lookup>(&op))
      lookup->stops = {{100, 0, 0, 0}, {110, 0, 0, 0}, {140, 0, 0, 0}};
  w->setPoints(id, edited);
  ASSERT_TRUE(w->render());
  const shape::Cloud reheated = w->readPoints(id);
  const shape::Cloud cpuReheated = shape::popops::cook(edited);
  const std::vector<glm::vec4>* gpuHeat2 = reheated.colorIf("heat");
  const std::vector<glm::vec4>* cpuHeat2 = cpuReheated.colorIf("heat");
  ASSERT_TRUE(gpuHeat2 && cpuHeat2);
  for (size_t i = 0; i < reheated.size(); i += 37) {
    EXPECT_GT((*gpuHeat2)[i].r, 50.0f) << "the new table must be live";
    EXPECT_NEAR((*gpuHeat2)[i].r, (*cpuHeat2)[i].r, 1e-3f) << i;
  }
}

TEST(World, SelectorsDeformersAndMasksAgreeAcrossExecutors) {
  // The second parity chain: Group (sphere and box, feathered and
  // combined), a masked Math and a masked Relax, Transform on P and on
  // Dir, Peak, all three Deform kinds and Mix by lane — read back from
  // the GPU and compared to the CPU cook. A kernel that ignored its
  // mask, or a table row pointing at the wrong kernel, shows up as a
  // lane that disagrees.
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  std::vector<glm::vec3> loop;
  for (int i = 0; i < 12; ++i) {
    const float a = (float)i / 12.0f * 2.0f * (float)M_PI;
    loop.push_back(
        {180.0f * std::cos(a), 30.0f * std::sin(2 * a), 180.0f * std::sin(a)});
  }
  const shape::pop::Chain chain =
      shape::pop::on(loop)
          .count(512)
          .seed(4)
          .select("east", {180, 0, 0}, 140, 0.5f)
          .select("east", shape::pop::Group::Shape::Box, {-180, 0, 0},
                  {90, 300, 90}, 0.2f, shape::pop::Group::Combine::Union)
          .move({0, 40, 0})
          .masked("east")
          .jitter(9)
          .smooth(0.5f, 2)
          .masked("east")
          .transform(shape::space::place({10, 0, -5}, 35, 10))
          .orient(shape::space::place({}, 35, 10))
          .peak(12)
          .twist(120, {0, 1, 0}, -40, 40)
          .taper(0.4f, {0, 1, 0}, -40, 40)
          .bend(60, {0, 1, 0}, {1, 0, 0}, -40, 40)
          .set("warm", {1, 0.5f, 0.2f, 1})
          .fade({0, 0, 1, 1}, {0, 1, 0, 1})
          .mixBy("Color", "warm", "Color", "east")
          .copy("east", "sel2");
  const uint32_t id =
      w->addPoints(shape::mesh::quad(4, 4), chain, world::Material{});
  ASSERT_NE(id, 0u) << "every op in this chain is GPU-executable";
  ASSERT_TRUE(w->render());

  const shape::Cloud gpu = w->readPoints(id);
  const shape::Cloud cpu = shape::popops::cook(chain);
  ASSERT_EQ(gpu.size(), 512u);
  ASSERT_EQ(cpu.size(), 512u);
  const std::vector<glm::vec3>* gpuDir = gpu.vectorIf("dir");
  const std::vector<glm::vec3>* cpuDir = cpu.vectorIf("dir");
  const std::vector<glm::vec4>* gpuTint = gpu.colorIf("tint");
  const std::vector<glm::vec4>* cpuTint = cpu.colorIf("tint");
  const std::vector<glm::vec4>* gpuEast = gpu.colorIf("east");
  const std::vector<glm::vec4>* cpuEast = cpu.colorIf("east");
  const std::vector<glm::vec4>* gpuSel2 = gpu.colorIf("sel2");
  ASSERT_TRUE(gpuDir && cpuDir && gpuTint && cpuTint && gpuEast && cpuEast &&
              gpuSel2);
  int selected = 0, partial = 0;
  for (size_t i = 0; i < gpu.size(); ++i) {
    EXPECT_NEAR(gpu.positions[i].x, cpu.positions[i].x, 0.05f) << i;
    EXPECT_NEAR(gpu.positions[i].y, cpu.positions[i].y, 0.05f) << i;
    EXPECT_NEAR(gpu.positions[i].z, cpu.positions[i].z, 0.05f) << i;
    EXPECT_NEAR((*gpuDir)[i].x, (*cpuDir)[i].x, 1e-3f) << i;
    EXPECT_NEAR((*gpuDir)[i].y, (*cpuDir)[i].y, 1e-3f) << i;
    EXPECT_NEAR((*gpuDir)[i].z, (*cpuDir)[i].z, 1e-3f) << i;
    EXPECT_NEAR((*gpuTint)[i].r, (*cpuTint)[i].r, 1e-3f) << i;
    EXPECT_NEAR((*gpuTint)[i].g, (*cpuTint)[i].g, 1e-3f) << i;
    EXPECT_NEAR((*gpuEast)[i].x, (*cpuEast)[i].x, 1e-4f) << i;
    EXPECT_NEAR((*gpuSel2)[i].x, (*cpuEast)[i].x, 1e-4f) << i;
    selected += (*gpuEast)[i].x > 0.99f;
    partial += (*gpuEast)[i].x > 0.05f && (*gpuEast)[i].x < 0.95f;
  }
  EXPECT_GT(selected, 60) << "the selection must pick real points";
  EXPECT_GT(partial, 8) << "the feather must grade";
}

TEST(World, PointSetLedChainsCookOnTheGpuFromTheUploadedLanes) {
  // A chain that starts from an existing cloud — the imported-points
  // door — cooks on the GPU from an arena uploaded with the cloud's
  // lanes, custom ones included, and agrees with the CPU cook. A
  // re-describe with a different cloud re-uploads.
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  shape::Cloud given;
  for (int i = 0; i < 300; ++i)
    given.positions.push_back({(float)(i % 20) * 12 - 120,
                               (float)(i / 20) * 12 - 90,
                               std::sin((float)i * 0.3f) * 30});
  std::vector<float>& size = given.scalar("size", 1);
  std::vector<float>& top = given.scalar("top");
  std::vector<glm::vec3>& normal = given.vector("normal");
  std::vector<glm::vec4>& heat = given.color("heat");
  for (int i = 0; i < 300; ++i) {
    size[(size_t)i] = 1.0f + (float)(i % 4) * 0.5f;
    top[(size_t)i] = given.positions[(size_t)i].y > 0 ? 1.0f : 0.0f;
    normal[(size_t)i] = {0, 0, 1};
    heat[(size_t)i] = {(float)i / 300.0f, 0, 0, 1};
  }
  const shape::pop::Chain chain =
      shape::pop::on(given)
          .move({0, 25, 0})
          .masked("top")
          .peak(8)
          .rampBy("heat", 0, {{0, 0, 1, 1}, {1, 1, 0, 1}})
          .vary(0.3f)
          .masked("top");
  const uint32_t id =
      w->addPoints(shape::mesh::quad(4, 4), chain, world::Material{});
  ASSERT_NE(id, 0u);
  ASSERT_TRUE(w->render());
  shape::Cloud gpu = w->readPoints(id);
  shape::Cloud cpu = shape::popops::cook(chain);
  ASSERT_EQ(gpu.size(), 300u);
  ASSERT_EQ(cpu.size(), 300u);
  const auto compare = [&](const shape::Cloud& a, const shape::Cloud& b) {
    const std::vector<float>* aSize = a.scalarIf("size");
    const std::vector<float>* bSize = b.scalarIf("size");
    const std::vector<glm::vec4>* aTint = a.colorIf("tint");
    const std::vector<glm::vec4>* bTint = b.colorIf("tint");
    const std::vector<glm::vec4>* aTop = a.colorIf("top");
    const std::vector<glm::vec4>* bTop = b.colorIf("top");
    ASSERT_TRUE(aSize && bSize && aTint && bTint && aTop && bTop);
    for (size_t i = 0; i < a.size(); i += 7) {
      EXPECT_NEAR(a.positions[i].x, b.positions[i].x, 1e-3f) << i;
      EXPECT_NEAR(a.positions[i].y, b.positions[i].y, 1e-3f) << i;
      EXPECT_NEAR(a.positions[i].z, b.positions[i].z, 1e-3f) << i;
      EXPECT_NEAR((*aSize)[i], (*bSize)[i], 1e-4f) << i;
      EXPECT_NEAR((*aTint)[i].r, (*bTint)[i].r, 1e-4f) << i;
      EXPECT_NEAR((*aTop)[i].x, (*bTop)[i].x, 1e-5f) << i;
    }
  };
  compare(gpu, cpu);
  // The mask really came from the cloud: the top half moved up 25 and
  // was varied, the bottom half kept y and size.
  int moved = 0;
  for (size_t i = 0; i < 300; ++i)
    moved += gpu.positions[i].y > given.positions[i].y + 20;
  EXPECT_GT(moved, 100);
  EXPECT_LT(moved, 200);

  // Re-describe with a different cloud (fewer points, another custom
  // lane): the arena is rebuilt and re-uploaded.
  shape::Cloud other;
  for (int i = 0; i < 64; ++i) other.positions.push_back({(float)i * 3, 0, 0});
  other.scalar("top", 1);
  other.color("heat", {0.5f, 0, 0, 1});
  other.scalar("extra", 7);
  shape::pop::Chain again = chain;
  again.front() = shape::pop::PointSet{other};
  w->setPoints(id, again);
  ASSERT_TRUE(w->render());
  gpu = w->readPoints(id);
  cpu = shape::popops::cook(again);
  ASSERT_EQ(gpu.size(), 64u);
  compare(gpu, cpu);
  const std::vector<glm::vec4>* extra = gpu.colorIf("extra");
  ASSERT_TRUE(extra);
  EXPECT_FLOAT_EQ((*extra)[3].x, 7.0f);
}

TEST(World, PermutationClassChainsAreDeclinedNotDropped) {
  // The same boundary, for the same structural reason: a sort permutes
  // the whole point set, which is not a per-point map and so has no
  // place in a model of one kernel per operator over fixed lanes.
  // Declining is the contract — a dropped sort would cook the points in
  // the WRONG ORDER, and order is load-bearing for draw order, for swept
  // paths and for neighbourhood operators.
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  std::vector<glm::vec3> loop;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    loop.push_back(
        {150.0f * std::cos(a), 20.0f * std::sin(2 * a), 150.0f * std::sin(a)});
  }
  const auto describe = [&](bool sort) {
    shape::pop::Builder b = shape::pop::on(loop);
    b.count(64).spread(8).fade({1, 0, 0, 1}, {0, 0, 1, 1});
    if (sort) b.order({0, 1, 0}, true);
    return (shape::pop::Chain)b;
  };
  const shape::Mesh stamp = shape::mesh::quad(6, 6);

  // The control: the same chain WITHOUT the sort cooks fine.
  const uint32_t plain =
      w->addPoints(stamp, describe(false), world::Material{});
  ASSERT_NE(plain, 0u);
  ASSERT_TRUE(w->render());
  const shape::Cloud cooked = w->readPoints(plain);
  ASSERT_EQ(cooked.size(), 64u);

  EXPECT_EQ(w->addPoints(stamp, describe(true), world::Material{}), 0u);
  EXPECT_EQ(
      w->addPointsOn(plain, stamp,
                     (shape::pop::Chain)shape::pop::on(std::vector<glm::vec3>{})
                         .count(32)
                         .order({0, 1, 0}),
                     world::Material{}),
      0u)
      << "the composing entry declines too";

  // setPoints refuses the re-describe rather than half-applying it:
  // the surface keeps cooking the chain it had.
  w->setPoints(plain, describe(true));
  ASSERT_TRUE(w->render());
  const shape::Cloud after = w->readPoints(plain);
  ASSERT_EQ(after.size(), 64u);
  for (size_t i = 0; i < after.size(); i += 13)
    EXPECT_NEAR(after.positions[i].y, cooked.positions[i].y, 1e-4f)
        << "a declined re-describe must change nothing";

  // ...and the CPU executor performs the sort the GPU declined, so the
  // capability is not lost, only located.
  const shape::Cloud cpu = shape::popops::cook(describe(true));
  ASSERT_EQ(cpu.size(), 64u);
  for (size_t i = 1; i < cpu.size(); ++i)
    EXPECT_GE(cpu.positions[i - 1].y, cpu.positions[i].y);
}

TEST(World, ChainsComposeOnDevice) {
  // One chain feeds another with NO CPU round trip: chain B's generator
  // reads chain A's cooked position lane straight from its arena, and
  // the result matches composing the same two chains on the CPU.
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  std::vector<glm::vec3> loop;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    loop.push_back({150.0f * std::cos(a), 0, 150.0f * std::sin(a)});
  }
  const shape::pop::Chain chainA =
      shape::pop::on(loop).count(48).noise(24, 0.01f).smooth(0.5f, 2);
  const shape::pop::Chain chainB =
      shape::pop::on(std::vector<glm::vec3>{})  // loop comes from upstream
          .count(300)
          .spread(6)
          .seed(9)
          .vary(0.3f);

  const uint32_t a =
      w->addPoints(shape::mesh::quad(4, 4), chainA, world::Material{});
  ASSERT_NE(a, 0u);
  const uint32_t b =
      w->addPointsOn(a, shape::mesh::quad(4, 4), chainB, world::Material{});
  ASSERT_NE(b, 0u);
  ASSERT_TRUE(w->render());

  const shape::Cloud gpu = w->readPoints(b);
  // The same composition, done on the CPU.
  shape::pop::Chain cpuB = chainB;
  std::get<shape::pop::SplineScatter>(cpuB.front()).loop =
      shape::popops::cook(chainA).positions;
  const shape::Cloud cpu = shape::popops::cook(cpuB);
  ASSERT_EQ(gpu.size(), 300u);
  ASSERT_EQ(cpu.size(), 300u);
  for (size_t i : {size_t(0), size_t(150), size_t(299)}) {
    EXPECT_NEAR(gpu.positions[i].x, cpu.positions[i].x, 0.05f);
    EXPECT_NEAR(gpu.positions[i].y, cpu.positions[i].y, 0.05f);
    EXPECT_NEAR(gpu.positions[i].z, cpu.positions[i].z, 0.05f);
  }
}

TEST(World, SetPointsWithEditedLoopMatchesAFreshDescribe) {
  // The loop is part of the description too: a re-describe whose
  // control points MOVED must cook the new loop rather than the buffer
  // uploaded when the surface was added — including when the chain's
  // shape is otherwise unchanged.
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  const auto chainOn = [](float radius) {
    std::vector<glm::vec3> loop;
    for (int i = 0; i < 10; ++i) {
      const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
      loop.push_back({radius * std::cos(a), 0, radius * std::sin(a)});
    }
    return (shape::pop::Chain)shape::pop::on(loop).count(128).spread(5).seed(3);
  };

  const uint32_t id =
      w->addPoints(shape::mesh::quad(3, 3), chainOn(150), world::Material{});
  ASSERT_NE(id, 0u);
  ASSERT_TRUE(w->render());

  // Same operators, same count; only the loop shrank. The re-cook must
  // land where a fresh describe of the smaller circle lands.
  w->setPoints(id, chainOn(60));
  ASSERT_TRUE(w->render());
  const shape::Cloud gpu = w->readPoints(id);
  const shape::Cloud cpu = shape::popops::cook(chainOn(60));
  ASSERT_EQ(gpu.size(), 128u);
  ASSERT_EQ(cpu.size(), 128u);
  for (size_t i : {size_t(0), size_t(64), size_t(127)}) {
    EXPECT_NEAR(gpu.positions[i].x, cpu.positions[i].x, 0.05f);
    EXPECT_NEAR(gpu.positions[i].y, cpu.positions[i].y, 0.05f);
    EXPECT_NEAR(gpu.positions[i].z, cpu.positions[i].z, 0.05f);
  }
}

TEST(World, RemovingUpstreamLeavesDependentsCookedAndAlive) {
  // Tearing down a chain's upstream must not take its dependents with
  // it. The dependent keeps its last cooked lanes — its bindings hold
  // the upstream arena alive — and the world goes on rendering.
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  std::vector<glm::vec3> loop;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    loop.push_back({150.0f * std::cos(a), 0, 150.0f * std::sin(a)});
  }
  const shape::pop::Chain chainA =
      shape::pop::on(loop).count(48).noise(24, 0.01f).smooth(0.5f, 2);
  const shape::pop::Chain chainB =
      shape::pop::on(std::vector<glm::vec3>{})  // loop comes from upstream
          .count(300)
          .spread(6)
          .seed(9)
          .vary(0.3f);

  const uint32_t a =
      w->addPoints(shape::mesh::quad(4, 4), chainA, world::Material{});
  ASSERT_NE(a, 0u);
  const uint32_t b =
      w->addPointsOn(a, shape::mesh::quad(4, 4), chainB, world::Material{});
  ASSERT_NE(b, 0u);
  ASSERT_TRUE(w->render());
  const shape::Cloud before = w->readPoints(b);
  ASSERT_EQ(before.size(), 300u);

  w->removeSurface(a);
  EXPECT_EQ(w->surfaceCount(), 1u);
  ASSERT_TRUE(w->render());
  const shape::Cloud after = w->readPoints(b);
  ASSERT_EQ(after.size(), 300u);
  // Frozen at the last cook — not recooked over garbage, not zeroed.
  for (size_t i : {size_t(0), size_t(150), size_t(299)}) {
    EXPECT_NEAR(after.positions[i].x, before.positions[i].x, 1e-3f);
    EXPECT_NEAR(after.positions[i].y, before.positions[i].y, 1e-3f);
    EXPECT_NEAR(after.positions[i].z, before.positions[i].z, 1e-3f);
  }
}

TEST(World, UpstreamWindowSlideRecooksDependentsSameFrame) {
  // The dependency edge is LIVE: sliding the upstream's window must
  // re-cook its dependent within the same render, and that dependent
  // must land where composing the slid upstream on the CPU lands.
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  std::vector<glm::vec3> loop;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    loop.push_back({150.0f * std::cos(a), 0, 150.0f * std::sin(a)});
  }
  const auto chainAt = [&](float head, float span) {
    return (shape::pop::Chain)shape::pop::on(loop)
        .count(48)
        .window(head, span)
        .noise(24, 0.01f)
        .smooth(0.5f, 2);
  };
  const shape::pop::Chain chainB =
      shape::pop::on(std::vector<glm::vec3>{})  // loop comes from upstream
          .count(300)
          .spread(6)
          .seed(9)
          .vary(0.3f);

  const uint32_t a = w->addPoints(shape::mesh::quad(4, 4), chainAt(1.0f, 0.4f),
                                  world::Material{});
  ASSERT_NE(a, 0u);
  const uint32_t b =
      w->addPointsOn(a, shape::mesh::quad(4, 4), chainB, world::Material{});
  ASSERT_NE(b, 0u);
  ASSERT_TRUE(w->render());
  const shape::Cloud before = w->readPoints(b);
  ASSERT_EQ(before.size(), 300u);

  w->setPointsWindow(a, 0.5f, 0.4f);
  ASSERT_TRUE(w->render());
  const shape::Cloud slid = w->readPoints(b);
  ASSERT_EQ(slid.size(), 300u);

  // (i) The dependent actually followed the upstream arc.
  float moved = 0;
  for (size_t i : {size_t(0), size_t(150), size_t(299)})
    moved += glm::length(slid.positions[i] - before.positions[i]);
  EXPECT_GT(moved, 1.0f)
      << "geometry riding the window must move when the window slides";

  // (ii) And it landed on the CPU reference over the slid upstream
  // description.
  shape::pop::Chain cpuB = chainB;
  std::get<shape::pop::SplineScatter>(cpuB.front()).loop =
      shape::popops::cook(chainAt(0.5f, 0.4f)).positions;
  const shape::Cloud cpu = shape::popops::cook(cpuB);
  ASSERT_EQ(cpu.size(), 300u);
  for (size_t i : {size_t(0), size_t(150), size_t(299)}) {
    EXPECT_NEAR(slid.positions[i].x, cpu.positions[i].x, 0.05f);
    EXPECT_NEAR(slid.positions[i].y, cpu.positions[i].y, 0.05f);
    EXPECT_NEAR(slid.positions[i].z, cpu.positions[i].z, 0.05f);
  }
}

// --- the demo's tile slicing -----------------------------------------------
// world_demo lays its ribbon artwork out as one tall column, snapshots
// it to a vector picture, and cuts it into GPU tiles through
// sigil::compose::tiles. The band's legibility on the ribbon wall rests
// entirely on that transform, and getting the mirror or the step wrong
// fails silently — the band simply reads backwards or jumps. These two
// tests hold the demo's own tile geometry and orientation in place.

TEST(WorldMarqueeSlice, TileWindowsStepDownAndMirrorAcross) {
  namespace tiles = sigil::compose::tiles;
  // The demo's actual strip geometry: a 506 px wide column cut into ten
  // 4096 px tiles, mirrored across, because the sweep wall's u axis runs
  // backwards.
  const SkISize tile = SkISize::Make(506, 4096);
  const float w = (float)tile.width();
  const float h = (float)tile.height();

  for (int k : {0, 1, 5, 9}) {
    const SkMatrix got =
        tiles::window(tile, k, tiles::Flow::Down, tiles::Facing::Mirrored);

    // (i) Exactly this concatenation:
    //     translate(w, 0) . scale(-1, 1) . translate(0, -k*h).
    SkMatrix byHand = SkMatrix::Translate(w, 0);
    byHand.preScale(-1, 1);
    byHand.preTranslate(0, -(float)k * h);
    EXPECT_EQ(got, byHand) << "tile " << k << " transform moved";

    // (ii) And what that means in points, so a future rewrite that
    //      happens to differ has to differ HERE too: the k-th slice's
    //      top-left corner of the column lands on the tile's top-RIGHT
    //      (the mirror), and its bottom-right on the bottom-left.
    EXPECT_EQ(got.mapPoint({0, (float)k * h}), (SkPoint{w, 0}));
    EXPECT_EQ(got.mapPoint({w, (float)k * h + h}), (SkPoint{0, h}));
    // The slice is a pure step along y: no scaling of the band, and no
    // transpose.
    EXPECT_FLOAT_EQ(std::abs(got.getScaleX()), 1.0f);
    EXPECT_FLOAT_EQ(got.getScaleY(), 1.0f);
    EXPECT_FLOAT_EQ(got.getSkewX(), 0.0f);
    EXPECT_FLOAT_EQ(got.getSkewY(), 0.0f);
  }

  // (iii) Both options the demo chose are load-bearing: an unmirrored
  //       tile, or a slice across instead of down, is a different
  //       picture.
  EXPECT_NE(tiles::window(tile, 3, tiles::Flow::Down, tiles::Facing::Forward),
            tiles::window(tile, 3, tiles::Flow::Down, tiles::Facing::Mirrored));
  EXPECT_NE(
      tiles::window(tile, 3, tiles::Flow::Across, tiles::Facing::Mirrored),
      tiles::window(tile, 3, tiles::Flow::Down, tiles::Facing::Mirrored));
}

TEST(WorldMarqueeSlice, SliceableReplaysTheSamePixels) {
  namespace tiles = sigil::compose::tiles;
  // The demo draws the SLICEABLE re-recording rather than the original
  // snapshot, so its output only stays put while the two replay
  // identically.
  const SkISize tile = SkISize::Make(24, 32);
  const int tileCount = 3;
  SkPictureRecorder recorder;
  {
    SkCanvas* c = recorder.beginRecording(SkRect::MakeWH(
        (float)tile.width(), (float)tile.height() * (float)tileCount));
    SkPaint paint;
    paint.setAntiAlias(true);
    for (int i = 0; i < 40; ++i) {
      paint.setColor(
          SkColorSetARGB(255, (uint8_t)(i * 6), 120, (uint8_t)(255 - i * 5)));
      c->drawRect(
          SkRect::MakeXYWH((float)(i % 7) * 3.0f, (float)i * 2.4f, 9.0f, 5.0f),
          paint);
    }
  }
  const sk_sp<SkPicture> art = recorder.finishRecordingAsPicture();
  const sk_sp<SkPicture> sliced = tiles::sliceable(art);
  ASSERT_TRUE(sliced);

  const auto slice = [&](const sk_sp<SkPicture>& picture, int k) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(tile.width(), tile.height());
    SkCanvas canvas(bitmap);
    canvas.clear(SK_ColorTRANSPARENT);
    canvas.concat(
        tiles::window(tile, k, tiles::Flow::Down, tiles::Facing::Mirrored));
    canvas.drawPicture(picture);
    return bitmap;
  };
  for (int k = 0; k < tileCount; ++k) {
    const SkBitmap raw = slice(art, k);
    const SkBitmap fast = slice(sliced, k);
    ASSERT_EQ(raw.computeByteSize(), fast.computeByteSize());
    EXPECT_EQ(
        std::memcmp(raw.getPixels(), fast.getPixels(), raw.computeByteSize()),
        0)
        << "sliceable() replay differs on tile " << k;
  }
}

// ---------------------------------------------------------------------------
// DECLARED MOTION — Animation.h.
//
// Animatable<float> lanes on registry components, resolved by
// resolveAnimation(), which render() calls itself. These cover that
// surface and the three properties it rests on: float-only lanes, no
// clock inside world, and an animate() form that lands settled.
// ---------------------------------------------------------------------------

TEST(World, AnimatedChainDrivesAnOperatorDialAndRecooksOnlyOnChange) {
  // A twist amount bound to an Output: the resolve writes it into the
  // component's chain and re-describes the surface only when the value
  // moved; a still Output costs no re-cook, and the cooked positions
  // follow the amount.
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);
  std::vector<glm::vec3> loop = {
      {40, -100, 0}, {40, 100, 0}, {40, 100, 1}, {40, -100, 1}};
  const shape::pop::Chain chain = shape::pop::on(loop)
                                      .count(200)
                                      .window(0.5f, 0.5f)
                                      .twist(0, {0, 1, 0}, -100, 100);
  const uint32_t id =
      w->addPoints(shape::mesh::quad(4, 4), chain, world::Material{});
  ASSERT_NE(id, 0u);
  choreograph::Output<float> amount{0.0f};
  world::AnimatedChain animated;
  animated.chain = chain;
  world::AnimatedChain::Lane lane;
  lane.op = 1;  // the Deform
  lane.field = "amount";
  lane.value = world::bind(&amount);
  animated.lanes.push_back(std::move(lane));
  w->registry().emplace<world::AnimatedChain>(world::entity(id),
                                              std::move(animated));

  world::AnimationStats stats = world::resolveAnimation(*w);
  EXPECT_EQ(stats.chains, 1) << "the first resolve applies the lane";
  stats = world::resolveAnimation(*w);
  EXPECT_EQ(stats.chains, 0) << "a still lane costs nothing";
  ASSERT_TRUE(w->render());
  const shape::Cloud flat = w->readPoints(id);

  amount = 180.0f;
  ASSERT_TRUE(w->render());  // render resolves; a moved lane re-cooks
  const shape::Cloud twisted = w->readPoints(id);
  ASSERT_EQ(flat.size(), twisted.size());
  // At the top of the column (u = 1) a 180 twist about +Y sends x = 40
  // to x = -40; at the bottom (u = 0) nothing moves.
  int flipped = 0, still = 0;
  for (size_t i = 0; i < flat.size(); ++i) {
    if (flat.positions[i].y > 90 && twisted.positions[i].x < -35) ++flipped;
    if (flat.positions[i].y < -90 &&
        std::abs(twisted.positions[i].x - flat.positions[i].x) < 3.0f)
      ++still;
  }
  EXPECT_GT(flipped, 0);
  EXPECT_GT(still, 0);
  // ...and it agrees with the CPU cook of the same edited chain.
  shape::pop::Chain edited = chain;
  shape::popops::setField(edited[1], "amount", 180.0f);
  const shape::Cloud cpu = shape::popops::cook(edited);
  for (size_t i = 0; i < cpu.size(); i += 13)
    EXPECT_NEAR(twisted.positions[i].x, cpu.positions[i].x, 1e-2f) << i;
}

TEST(WorldAnimation, BoundLaneDrivesTheTransformWithoutADevice) {
  // The device-free half of the system is a free function over a plain
  // entt::registry, which is what lets the animation semantics be
  // checked on a machine with no GPU, where every test above skips.
  entt::registry registry;
  const entt::entity e = registry.create();
  registry.emplace<world::TransformComponent>(e);
  choreograph::Output<float> slide{0.0f};
  registry.emplace<world::AnimatedTransform>(e).x = &slide;

  slide = 40.0f;
  world::AnimationStats stats = world::resolveAnimation(registry);
  EXPECT_EQ(stats.transforms, 1);
  EXPECT_FLOAT_EQ(registry.get<world::TransformComponent>(e).model[3][0],
                  40.0f);

  slide = -12.5f;
  stats = world::resolveAnimation(registry);
  EXPECT_EQ(stats.transforms, 1);
  EXPECT_FLOAT_EQ(registry.get<world::TransformComponent>(e).model[3][0],
                  -12.5f);
}

TEST(WorldAnimation, ShapedBindingRunsItsChainOnTheLane) {
  // Lanes are float precisely so that bind()'s normalise → curve →
  // affine chain reaches them. If the chain were dropped and the raw
  // Output landed on the lane, this yaw would be 1 degree rather than
  // 22.5.
  entt::registry registry;
  const entt::entity e = registry.create();
  registry.emplace<world::TransformComponent>(e);
  choreograph::Output<float> phase{1.0f};
  registry.emplace<world::AnimatedTransform>(e).yawDeg =
      world::bind(&phase)
          .window(0.0f, 2.0f)             // 1.0 -> 0.5
          .map(&choreograph::easeInQuad)  // 0.5 -> 0.25
          .target(0.0f, 90.0f);           // 0.25 -> 22.5 deg
  world::resolveAnimation(registry);

  const glm::mat4& m = registry.get<world::TransformComponent>(e).model;
  EXPECT_NEAR(m[0][0], std::cos(22.5f * (float)M_PI / 180.0f), 1e-5f);
  // A raw, unshaped binding would have yawed 1 degree, whose cosine is
  // barely under 1.
  EXPECT_LT(m[0][0], 0.99f) << "the bind() chain must run";
}

TEST(WorldAnimation, AnimateFormLandsOnItsSettledValue) {
  // These lanes accept the animate() forms because they are the
  // ordinary motion slot type — but ramp-on-change needs a CHANGE event,
  // and world has no describe-and-diff over components. So a
  // transitioned value resolves straight to its TARGET with no ramp.
  // Landing on the `from`, or on a zero, would be the bug.
  using namespace std::chrono_literals;
  entt::registry registry;
  const entt::entity e = registry.create();
  registry.emplace<world::TransformComponent>(e);
  auto& animated = registry.emplace<world::AnimatedTransform>(e);
  animated.z = world::animate(world::from(0.0f).to(7.0f), {500ms});
  animated.x = world::animate(world::to(3.0f));
  world::resolveAnimation(registry);

  const glm::mat4& m = registry.get<world::TransformComponent>(e).model;
  EXPECT_FLOAT_EQ(m[3][2], 7.0f) << "from().to() settles on the target";
  EXPECT_FLOAT_EQ(m[3][0], 3.0f);
}

TEST(WorldAnimation, MaterialLanesOverrideOnlyWhatTheyDeclare) {
  // The optional lanes are not decoration: a component with plain
  // defaults would slam this pane's authored alpha 0.4 to 1 and its
  // uvScale 2 to 1 the moment uvOffsetX was engaged.
  entt::registry registry;
  const entt::entity e = registry.create();
  world::Material authored;
  authored.baseColor = {0.2f, 0.3f, 0.4f, 0.4f};
  authored.emissiveStrength = 5.0f;
  authored.uvScale = {2, 2};
  registry.emplace<world::MaterialComponent>(e, authored);
  choreograph::Output<float> scroll{0.0f};
  registry.emplace<world::AnimatedMaterial>(e).uvOffsetX =
      world::bind(&scroll).target(0.0f, 1.0f);

  scroll = 0.25f;
  const world::AnimationStats stats = world::resolveAnimation(registry);
  EXPECT_EQ(stats.materials, 1);
  const world::Material& m = registry.get<world::MaterialComponent>(e).material;
  EXPECT_FLOAT_EQ(m.uvOffset.x, 0.25f);
  EXPECT_FLOAT_EQ(m.baseColor.w, 0.4f) << "an undeclared lane is not driven";
  EXPECT_FLOAT_EQ(m.uvScale.x, 2.0f);
  EXPECT_FLOAT_EQ(m.emissiveStrength, 5.0f);
  EXPECT_FLOAT_EQ(m.uvOffset.y, 0.0f);
}

TEST(WorldAnimation, DerivedOutputDrivesAWorldLaneDeviceFree) {
  // Ticker::derive() composing across the library boundary: a derived
  // Output is an ordinary Output, so a world lane consumes it through
  // the same bind() chain as any hand-stepped cell and no world code
  // knows the Ticker owns the write. Device-free — the ticker steps,
  // resolveAnimation() reads, nothing renders.
  entt::registry registry;
  const entt::entity e = registry.create();
  registry.emplace<world::MaterialComponent>(e);

  motion::Ticker ticker;
  choreograph::Output<float> phase{0.0f}, trail{0.0f};
  ticker.timeline().apply(&phase).then<choreograph::RampTo>(1.0f, 1.0f);
  // Registered before the ramp above is ever stepped, and shaped through
  // the ordinary chain: one level of derivation, which is all that is
  // allowed.
  ASSERT_TRUE(
      ticker.derive(&trail, world::bind(&phase).offset(-0.25f).clamp(0, 1)));

  registry.emplace<world::AnimatedMaterial>(e).uvOffsetX =
      world::bind(&trail).target(0.0f, 2.0f);

  ticker.tick(0.5);  // phase 0.5 -> trail 0.25, SAME frame (two-phase step)
  world::AnimationStats stats = world::resolveAnimation(registry);
  EXPECT_EQ(stats.materials, 1);
  const world::Material& m = registry.get<world::MaterialComponent>(e).material;
  EXPECT_FLOAT_EQ(m.uvOffset.x, 0.5f)
      << "the lane must read the derived cell's SAME-frame value: "
         "(0.5 - 0.25) * 2";

  ticker.tick(0.25);
  world::resolveAnimation(registry);
  EXPECT_FLOAT_EQ(registry.get<world::MaterialComponent>(e).material.uvOffset.x,
                  1.0f);
}

TEST(WorldAnimation, ResolveIsIdempotentAndReportsOnlyWhatMoved) {
  // Change detection: the property that keeps a lane sitting in front of
  // a GPU re-cook from dispatching every frame, and the reason
  // AnimationStats exists at all.
  entt::registry registry;
  const entt::entity e = registry.create();
  registry.emplace<world::TransformComponent>(e);
  registry.emplace<world::MaterialComponent>(e);
  registry.emplace<world::LightComponent>(e);
  choreograph::Output<float> lift{0.0f};
  choreograph::Output<float> fade{1.0f};
  choreograph::Output<float> glow{1.0f};
  registry.emplace<world::AnimatedTransform>(e).y = &lift;
  registry.emplace<world::AnimatedMaterial>(e).opacity = &fade;
  registry.emplace<world::AnimatedLight>(e).intensity = &glow;

  // Every lane resolves to what the component already held, so the
  // FIRST resolve is a no-op too — change detection compares against
  // the destination, not against a "have I run yet" flag.
  world::AnimationStats stats = world::resolveAnimation(registry);
  EXPECT_EQ(stats.transforms, 0) << "the composed matrix was already identity";
  EXPECT_EQ(stats.materials, 0) << "alpha was already 1";
  EXPECT_EQ(stats.lights, 0) << "intensity was already 1";

  lift = 20.0f;
  fade = 0.5f;
  glow = 3.0f;
  stats = world::resolveAnimation(registry);
  EXPECT_EQ(stats.transforms, 1);
  EXPECT_EQ(stats.materials, 1);
  EXPECT_EQ(stats.lights, 1);

  // Nothing moved: a second resolve must write nothing at all.
  stats = world::resolveAnimation(registry);
  EXPECT_EQ(stats.transforms, 0);
  EXPECT_EQ(stats.materials, 0);
  EXPECT_EQ(stats.lights, 0);
  EXPECT_FLOAT_EQ(registry.get<world::TransformComponent>(e).model[3][1],
                  20.0f);
}

TEST(WorldAnimation, SameTimeYieldsTheSameNumber) {
  // DETERMINISM, in numbers: world owns no clock, so the caller steps a
  // motion::Ticker with the delta it chooses and the same frame index
  // resolves to the same number, bit for bit — which is what makes a
  // headless render reproducible.
  const auto run = [](double dt, int frames) {
    motion::Ticker ticker;
    choreograph::Output<float> phase{0.0f};
    ticker.timeline().apply(&phase).then<choreograph::RampTo>(120.0f, 1.0f);
    entt::registry registry;
    const entt::entity e = registry.create();
    registry.emplace<world::TransformComponent>(e);
    registry.emplace<world::AnimatedTransform>(e).x =
        world::bind(&phase).map(&choreograph::easeInOutQuad).scale(1.0f);
    std::vector<float> trace;
    for (int i = 0; i < frames; ++i) {
      ticker.tick(dt);
      world::resolveAnimation(registry);
      trace.push_back(registry.get<world::TransformComponent>(e).model[3][0]);
    }
    return trace;
  };

  const std::vector<float> a = run(1.0 / 60.0, 60);
  const std::vector<float> b = run(1.0 / 60.0, 60);
  ASSERT_EQ(a.size(), b.size());
  for (size_t i = 0; i < a.size(); ++i)
    EXPECT_EQ(a[i], b[i]) << "frame " << i << " must be bit-identical";

  // Not vacuous: the trace has to actually move, and a DIFFERENT dt
  // sequence has to land somewhere else at the same frame index —
  // otherwise "identical" would be a claim about a constant.
  EXPECT_NE(a.front(), a.back());
  const std::vector<float> fast = run(1.0 / 30.0, 60);
  EXPECT_NE(a[10], fast[10]);
}

TEST(WorldAnimation, WiggledLanesShakeDeterministicallyAndPerSeed) {
  // The wiggle() stage lives in the motion library, so every world lane
  // gets it. It stays compatible with world's no-clock property because
  // the noise is a pure function of the NORMALISED input and never of a
  // clock — so this test can hold the same bar as
  // SameTimeYieldsTheSameNumber above.
  const auto run = [](double dt, int frames, uint32_t seedX, uint32_t seedY) {
    motion::Ticker ticker;
    choreograph::Output<float> seconds{0.0f};
    ticker.timeline().apply(&seconds).then<choreograph::RampTo>(2.0f, 2.0f);
    entt::registry registry;
    const entt::entity e = registry.create();
    registry.emplace<world::TransformComponent>(e);
    world::AnimatedTransform& lanes =
        registry.emplace<world::AnimatedTransform>(e);
    lanes.x = world::wiggle(&seconds, 0.6f, 7.0f, seedX);  // ±0.6 world units
    lanes.y = world::wiggle(&seconds, 0.6f, 7.0f, seedY);
    std::vector<std::pair<float, float>> trace;
    for (int i = 0; i < frames; ++i) {
      ticker.tick(dt);
      world::resolveAnimation(registry);
      const glm::mat4& m = registry.get<world::TransformComponent>(e).model;
      trace.emplace_back(m[3][0], m[3][1]);
    }
    return trace;
  };

  const auto a = run(1.0 / 60.0, 90, 1, 2);
  const auto b = run(1.0 / 60.0, 90, 1, 2);
  ASSERT_EQ(a.size(), b.size());
  for (size_t i = 0; i < a.size(); ++i)
    EXPECT_EQ(a[i], b[i]) << "frame " << i << " must be bit-identical";

  // Not vacuous, to the same three-part standard: the shake MOVES, it
  // stays inside its declared amplitude, and a different dt sequence
  // lands elsewhere at the same frame index.
  float span = 0.0f;
  for (const auto& [x, y] : a) {
    EXPECT_LE(std::fabs(x), 0.6f + 1e-4f);
    EXPECT_LE(std::fabs(y), 0.6f + 1e-4f);
    span = std::max(span, std::fabs(x - a.front().first));
  }
  EXPECT_GT(span, 0.3f) << "the shake never moved";
  const auto fast = run(1.0 / 30.0, 90, 1, 2);
  EXPECT_NE(a[10], fast[10]);

  // SEEDING: two lanes with the same seed are the SAME shake (a diagonal
  // slide), two with different seeds are independent. Both halves, or
  // neither claim means anything.
  const auto shared = run(1.0 / 60.0, 90, 5, 5);
  for (const auto& [x, y] : shared) EXPECT_FLOAT_EQ(x, y);
  int apart = 0;
  for (const auto& [x, y] : a)
    if (std::fabs(x - y) > 0.2f) ++apart;
  EXPECT_GT(apart, 10) << "seeds 1 and 2 shook together";
}

TEST(WorldAnimation, RenderResolvesTheLanesItself) {
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  const uint32_t id = w->addSurface(shape::mesh::quad(40, 40), glm::mat4(1.0f),
                                    world::Material{});
  ASSERT_NE(id, 0u);
  choreograph::Output<float> fade{0.25f};
  w->registry().emplace<world::AnimatedMaterial>(world::entity(id)).opacity =
      &fade;

  // No explicit resolveAnimation() call: render() owns that step.
  ASSERT_TRUE(w->render());
  EXPECT_FLOAT_EQ(w->registry()
                      .get<world::MaterialComponent>(world::entity(id))
                      .material.baseColor.w,
                  0.25f);
}

TEST(WorldAnimation, AnimatedFrameRendersIdenticallyAcrossRuns) {
  // DETERMINISM, in pixels: the same frame INDEX must produce the same
  // image. If declared motion ever made a render a function of wall
  // time, this is the test that catches it.
  world::WorldConfig config;
  config.width = 96;
  config.height = 72;
  config.clearColor = {0, 0, 0, 1};

  const auto runTo = [&](int frames, SkBitmap* out) {
    MAKE_WORLD_OR_SKIP(w, config);
    shape::space::Camera camera;
    camera.eye = {0, 0, 260};
    camera.target = {0, 0, 0};
    w->setCamera(camera);
    const uint32_t id = w->addSurface(shape::mesh::quad(90, 90),
                                      glm::mat4(1.0f), world::Material{});
    ASSERT_NE(id, 0u);

    motion::Ticker ticker;
    choreograph::Output<float> spin{0.0f};
    ticker.timeline().apply(&spin).then<choreograph::RampTo>(70.0f, 1.0f);
    w->registry().emplace<world::AnimatedTransform>(world::entity(id)).yawDeg =
        &spin;

    for (int i = 0; i < frames; ++i) {
      ticker.tick(1.0 / 60.0);
      ASSERT_TRUE(w->render());
    }
    sk_sp<SkImage> frame = w->readback();
    ASSERT_TRUE(frame);
    out->allocPixels(SkImageInfo::MakeN32Premul(config.width, config.height));
    ASSERT_TRUE(frame->readPixels(nullptr, out->pixmap(), 0, 0));
  };

  SkBitmap first, second, later;
  runTo(30, &first);
  if (first.isNull()) GTEST_SKIP() << "no 3D backend";
  runTo(30, &second);
  runTo(31, &later);

  ASSERT_EQ(first.computeByteSize(), second.computeByteSize());
  EXPECT_EQ(std::memcmp(first.getPixels(), second.getPixels(),
                        first.computeByteSize()),
            0)
      << "frame 30 must render identically across runs";
  // Not vacuous: frame 31 is a different frame, so it must differ.
  EXPECT_NE(std::memcmp(first.getPixels(), later.getPixels(),
                        first.computeByteSize()),
            0)
      << "the animation must actually be moving";
}

TEST(WorldAnimation, WindowLaneReachesTheGpuAndRecooksOnlyWhenItMoves) {
  // The one lane sitting in front of a GPU RE-COOK, and two claims about
  // it: it lands exactly where the imperative setter lands, and a lane
  // that is not moving costs zero dispatches. Without the second, an
  // animated flock would re-scatter every frame behind a parked Output.
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  std::vector<glm::vec3> loop;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    loop.push_back(
        {140.0f * std::cos(a), 40.0f * std::sin(2 * a), 140.0f * std::sin(a)});
  }
  const auto chain = [&] {
    return (shape::pop::Chain)shape::pop::on(loop)
        .count(128)
        .window(1.0f, 0.5f)
        .spread(9)
        .seed(6)
        .vary(0.3f);
  };
  const uint32_t animatedId =
      w->addPoints(shape::mesh::quad(3, 3), chain(), world::Material{});
  const uint32_t manualId =
      w->addPoints(shape::mesh::quad(3, 3), chain(), world::Material{});
  ASSERT_NE(animatedId, 0u);
  ASSERT_NE(manualId, 0u);

  choreograph::Output<float> head{0.35f};
  auto& window =
      w->registry().emplace<world::AnimatedWindow>(world::entity(animatedId));
  window.head = &head;
  window.span = 0.2f;

  world::AnimationStats stats = world::resolveAnimation(*w);
  EXPECT_EQ(stats.windows, 1);
  // Parked Output: the second resolve must not touch the generator.
  stats = world::resolveAnimation(*w);
  EXPECT_EQ(stats.windows, 0) << "an unmoved lane must not re-cook";

  w->setPointsWindow(manualId, 0.35f, 0.2f);
  ASSERT_TRUE(w->render());
  const shape::Cloud animatedCloud = w->readPoints(animatedId);
  const shape::Cloud manualCloud = w->readPoints(manualId);
  ASSERT_EQ(animatedCloud.size(), 128u);
  ASSERT_EQ(manualCloud.size(), 128u);
  for (size_t i : {size_t(0), size_t(64), size_t(127)}) {
    EXPECT_NEAR(animatedCloud.positions[i].x, manualCloud.positions[i].x,
                1e-4f);
    EXPECT_NEAR(animatedCloud.positions[i].y, manualCloud.positions[i].y,
                1e-4f);
    EXPECT_NEAR(animatedCloud.positions[i].z, manualCloud.positions[i].z,
                1e-4f);
  }

  // And it slides: move the Output and the cooked points move with it.
  head = 0.8f;
  stats = world::resolveAnimation(*w);
  EXPECT_EQ(stats.windows, 1);
  ASSERT_TRUE(w->render());
  const shape::Cloud slid = w->readPoints(animatedId);
  float moved = 0;
  for (size_t i : {size_t(0), size_t(64), size_t(127)})
    moved += glm::length(slid.positions[i] - animatedCloud.positions[i]);
  EXPECT_GT(moved, 1.0f) << "the animated window must actually slide";
}

// ---------------------------------------------------------------------------
// THE CAMERA LANES — AnimatedCamera.
//
// A camera is already a registry entity, so its lanes hang off an entity
// like every other lane and resolve in the DEVICE-FREE half. Precedence
// is the existing rule — an active CameraComponent beats setCamera() —
// and the tests below fix it at the pixel level.
// ---------------------------------------------------------------------------

TEST(WorldAnimation, CameraLanesDriveACameraEntityWithoutADevice) {
  entt::registry registry;
  const entt::entity e = registry.create();
  registry.emplace<world::CameraComponent>(e);
  choreograph::Output<float> dolly{0.0f};
  auto& animated = registry.emplace<world::AnimatedCamera>(e);
  animated.eyeZ = world::bind(&dolly).target(600.0f, 200.0f);
  animated.fovYDeg = world::bind(&dolly).target(40.0f, 18.0f);

  dolly = 0.5f;
  world::AnimationStats stats = world::resolveAnimation(registry);
  EXPECT_EQ(stats.cameras, 1);
  const shape::space::Camera& cam =
      registry.get<world::CameraComponent>(e).camera;
  EXPECT_FLOAT_EQ(cam.eye.z, 400.0f);
  EXPECT_FLOAT_EQ(cam.fovYDeg, 29.0f);

  // Change detection is one rule everywhere: a parked Output writes
  // nothing.
  stats = world::resolveAnimation(registry);
  EXPECT_EQ(stats.cameras, 0) << "an unmoved camera lane must not write";
}

TEST(WorldAnimation, CameraLanesOverrideOnlyWhatTheyDeclare) {
  // The camera is a component the caller also authors, so its lanes are
  // optional. With plain defaults instead, engaging eyeX alone would
  // also slam this camera's authored 12-degree lens back to 40 and its
  // target to the origin.
  entt::registry registry;
  const entt::entity e = registry.create();
  shape::space::Camera authored;
  authored.eye = {10, 20, 30};
  authored.target = {0, 90, 0};
  authored.fovYDeg = 12;
  authored.zNear = 2;
  authored.zFar = 9000;
  registry.emplace<world::CameraComponent>(e, authored);
  choreograph::Output<float> pan{0.0f};
  registry.emplace<world::AnimatedCamera>(e).eyeX =
      world::bind(&pan).target(0.0f, 100.0f);

  pan = 0.25f;
  EXPECT_EQ(world::resolveAnimation(registry).cameras, 1);
  const shape::space::Camera& cam =
      registry.get<world::CameraComponent>(e).camera;
  EXPECT_FLOAT_EQ(cam.eye.x, 25.0f);
  EXPECT_FLOAT_EQ(cam.eye.y, 20.0f) << "an undeclared lane is not driven";
  EXPECT_FLOAT_EQ(cam.eye.z, 30.0f);
  EXPECT_FLOAT_EQ(cam.target.y, 90.0f);
  EXPECT_FLOAT_EQ(cam.fovYDeg, 12.0f);
  EXPECT_FLOAT_EQ(cam.zNear, 2.0f) << "clip planes have no lanes at all";
  EXPECT_FLOAT_EQ(cam.zFar, 9000.0f);
}

TEST(WorldAnimation, CameraRollTurnsUpAboutTheViewAxisAndStaysUnit) {
  // `up` gets no lanes because three free floats cannot promise a unit
  // vector; rollDeg is the safe single-float parameterisation. The
  // derived up stays exactly unit length, and it is recomputed from
  // rollReference on every resolve, so repeated resolves cannot drift.
  entt::registry registry;
  const entt::entity e = registry.create();
  shape::space::Camera authored;
  authored.eye = {0, 0, 10};
  authored.target = {0, 0, 0};
  registry.emplace<world::CameraComponent>(e, authored);
  choreograph::Output<float> tilt{90.0f};
  registry.emplace<world::AnimatedCamera>(e).rollDeg = &tilt;

  EXPECT_EQ(world::resolveAnimation(registry).cameras, 1);
  const shape::space::Camera& cam =
      registry.get<world::CameraComponent>(e).camera;
  // Right-handed about the eye->target axis (0,0,-1): +y goes to +x.
  EXPECT_NEAR(cam.up.x, 1.0f, 1e-6f);
  EXPECT_NEAR(cam.up.y, 0.0f, 1e-6f);
  EXPECT_NEAR(cam.up.z, 0.0f, 1e-6f);
  EXPECT_NEAR(glm::length(cam.up), 1.0f, 1e-6f);

  // Idempotent: resolving twice at the same angle is not two rolls.
  EXPECT_EQ(world::resolveAnimation(registry).cameras, 0);
  EXPECT_NEAR(cam.up.x, 1.0f, 1e-6f);
  EXPECT_NEAR(glm::length(cam.up), 1.0f, 1e-6f);

  // And it is a real lane, not a constant: a new angle turns further.
  tilt = 180.0f;
  EXPECT_EQ(world::resolveAnimation(registry).cameras, 1);
  EXPECT_NEAR(cam.up.y, -1.0f, 1e-6f);
  EXPECT_NEAR(glm::length(cam.up), 1.0f, 1e-6f);
}

TEST(WorldAnimation, CameraTraceIsTheSameAtTheSameFrameIndex) {
  // DETERMINISM for the camera lanes, to the same three-part standard:
  // same frame index gives the same numbers bit for bit; the trace must
  // MOVE; and a different dt sequence must land elsewhere at the same
  // index, so "identical" is not a claim about a constant.
  const auto run = [](double dt, int frames) {
    motion::Ticker ticker;
    choreograph::Output<float> phase{0.0f};
    ticker.timeline().apply(&phase).then<choreograph::RampTo>(1.0f, 1.0f);
    entt::registry registry;
    const entt::entity e = registry.create();
    registry.emplace<world::CameraComponent>(e);
    auto& animated = registry.emplace<world::AnimatedCamera>(e);
    animated.eyeZ = world::bind(&phase)
                        .map(&choreograph::easeInOutQuad)
                        .target(700.0f, 180.0f);
    animated.fovYDeg = world::bind(&phase).target(45.0f, 22.0f);
    animated.rollDeg = world::bind(&phase).target(0.0f, 30.0f);
    std::vector<float> trace;
    for (int i = 0; i < frames; ++i) {
      ticker.tick(dt);
      world::resolveAnimation(registry);
      const shape::space::Camera& cam =
          registry.get<world::CameraComponent>(e).camera;
      trace.push_back(cam.eye.z);
      trace.push_back(cam.fovYDeg);
      trace.push_back(cam.up.x);
    }
    return trace;
  };

  const std::vector<float> a = run(1.0 / 60.0, 60);
  const std::vector<float> b = run(1.0 / 60.0, 60);
  ASSERT_EQ(a.size(), b.size());
  for (size_t i = 0; i < a.size(); ++i)
    EXPECT_EQ(a[i], b[i]) << "sample " << i << " must be bit-identical";

  EXPECT_NE(a.front(), a[a.size() - 3]) << "the dolly must actually move";
  const std::vector<float> fast = run(1.0 / 30.0, 60);
  EXPECT_NE(a[30], fast[30]);
}

TEST(WorldAnimation, AnimatedCameraOutranksALaterSetCamera) {
  // PRECEDENCE, at the pixel: an animated camera IS a camera entity, and
  // an active CameraComponent beats setCamera() — including a
  // setCamera() called after the lanes were engaged. Last-writer-wins
  // would make the frame depend on call order, which a declared lane
  // does not have.
  world::WorldConfig config;
  config.width = 96;
  config.height = 72;
  config.clearColor = {0, 0, 0, 1};

  const auto shoot = [&](float eyeZ, bool animate, SkBitmap* out) {
    MAKE_WORLD_OR_SKIP(w, config);
    shape::space::Camera far;
    far.eye = {0, 0, 900};
    far.target = {0, 0, 0};
    w->setCamera(far);
    ASSERT_NE(w->addSurface(shape::mesh::quad(120, 120), glm::mat4(1.0f),
                            world::Material{}),
              0u);
    if (animate) {
      entt::registry& registry = w->registry();
      const entt::entity cam = registry.create();
      registry.emplace<world::CameraComponent>(cam);
      auto& animated = registry.emplace<world::AnimatedCamera>(cam);
      animated.eyeZ = eyeZ;
      animated.targetZ = 0.0f;
      // The imperative door, used AFTER the lanes exist: it must lose.
      w->setCamera(far);
    } else {
      shape::space::Camera plain;
      plain.eye = {0, 0, eyeZ};
      plain.target = {0, 0, 0};
      w->setCamera(plain);
    }
    ASSERT_TRUE(w->render());
    sk_sp<SkImage> frame = w->readback();
    ASSERT_TRUE(frame);
    out->allocPixels(SkImageInfo::MakeN32Premul(config.width, config.height));
    ASSERT_TRUE(frame->readPixels(nullptr, out->pixmap(), 0, 0));
  };

  SkBitmap plainNear, plainFar, animatedNear;
  shoot(240.0f, false, &plainNear);
  if (plainNear.isNull()) GTEST_SKIP() << "no 3D backend";
  shoot(900.0f, false, &plainFar);
  shoot(240.0f, true, &animatedNear);

  ASSERT_EQ(plainNear.computeByteSize(), animatedNear.computeByteSize());
  // Not vacuous: the two setCamera() positions genuinely differ.
  EXPECT_NE(std::memcmp(plainNear.getPixels(), plainFar.getPixels(),
                        plainNear.computeByteSize()),
            0);
  EXPECT_EQ(std::memcmp(plainNear.getPixels(), animatedNear.getPixels(),
                        plainNear.computeByteSize()),
            0)
      << "the lane's camera must render, not the later setCamera()";
}

TEST(WorldAnimation, AnimatedCameraFrameRendersIdenticallyAcrossRuns) {
  // Determinism in pixels for the camera, mirroring
  // AnimatedFrameRendersIdenticallyAcrossRuns: the same frame index
  // gives the same pixels, and the next frame differs, so the dolly is
  // really moving.
  world::WorldConfig config;
  config.width = 96;
  config.height = 72;
  config.clearColor = {0, 0, 0, 1};

  const auto runTo = [&](int frames, SkBitmap* out) {
    MAKE_WORLD_OR_SKIP(w, config);
    ASSERT_NE(w->addSurface(shape::mesh::quad(120, 120), glm::mat4(1.0f),
                            world::Material{}),
              0u);
    entt::registry& registry = w->registry();
    const entt::entity cam = registry.create();
    registry.emplace<world::CameraComponent>(cam);

    motion::Ticker ticker;
    choreograph::Output<float> push{0.0f};
    ticker.timeline().apply(&push).then<choreograph::RampTo>(1.0f, 1.0f);
    auto& animated = registry.emplace<world::AnimatedCamera>(cam);
    animated.eyeZ = world::bind(&push).target(900.0f, 260.0f);
    animated.targetZ = 0.0f;
    animated.rollDeg = world::bind(&push).target(0.0f, 25.0f);

    for (int i = 0; i < frames; ++i) {
      ticker.tick(1.0 / 60.0);
      ASSERT_TRUE(w->render());
    }
    sk_sp<SkImage> frame = w->readback();
    ASSERT_TRUE(frame);
    out->allocPixels(SkImageInfo::MakeN32Premul(config.width, config.height));
    ASSERT_TRUE(frame->readPixels(nullptr, out->pixmap(), 0, 0));
  };

  SkBitmap first, second, later;
  runTo(30, &first);
  if (first.isNull()) GTEST_SKIP() << "no 3D backend";
  runTo(30, &second);
  runTo(31, &later);

  ASSERT_EQ(first.computeByteSize(), second.computeByteSize());
  EXPECT_EQ(std::memcmp(first.getPixels(), second.getPixels(),
                        first.computeByteSize()),
            0)
      << "frame 30 must render identically across runs";
  EXPECT_NE(std::memcmp(first.getPixels(), later.getPixels(),
                        first.computeByteSize()),
            0)
      << "the camera must actually be moving";
}

// ---------------------------------------------------------------------------
// THE CAMERA PATH — one float lane walking a curve.
//
// Independent float lanes can describe a POINT but not a TRAJECTORY.
// CameraPath adds the curve while keeping lanes float-only, by making
// the lane the curve's PARAMETER. The tests below fix its four rules —
// precedence over the eye lanes, the wrap, arc-length by default, and
// roll composing with the flight axis — plus the cache that keeps the
// arc-length table off the per-frame bill.
// ---------------------------------------------------------------------------

namespace {

/** An OPEN, deliberately uneven straight run: the first half of the
 *  parameter covers 10 units and the second covers 90, so parameter-
 *  uniform and arc-length walks answer differently at every sample and
 *  the difference is arithmetic anyone can check by hand. */
shape::Spline3 unevenRun() {
  shape::Spline3 s;
  s.points = {{0, 0, 0}, {10, 0, 0}, {100, 0, 0}};
  s.type = shape::Spline3::Type::Linear;
  s.closed = false;
  return s;
}

/** A CLOSED loop through four points, for the wrap and roll tests. */
shape::Spline3 diamondLoop() {
  shape::Spline3 s;
  s.points = {{100, 0, 0}, {0, 0, 100}, {-100, 0, 0}, {0, 0, -100}};
  s.type = shape::Spline3::Type::CatmullRom;
  s.closed = true;
  return s;
}

/** The camera of the registry's sole camera entity. */
const shape::space::Camera& cameraOf(entt::registry& registry, entt::entity e) {
  return registry.get<world::CameraComponent>(e).camera;
}

}  // namespace

TEST(WorldAnimation, CameraPathFliesTheEyeAlongTheSpline) {
  entt::registry registry;
  const entt::entity e = registry.create();
  registry.emplace<world::CameraComponent>(e);
  choreograph::Output<float> along{0.0f};
  auto& animated = registry.emplace<world::AnimatedCamera>(e);
  world::CameraPath& flight = animated.path.emplace();
  flight.path = unevenRun();
  flight.t = &along;
  flight.lookAhead = 0;  // aiming is tested separately; this is placement

  along = 0.5f;
  EXPECT_EQ(world::resolveAnimation(registry).cameras, 1);
  // Half the LENGTH, not half the parameter: 50 units along a 100-unit
  // run, where the parameter midpoint is x = 10.
  EXPECT_NEAR(cameraOf(registry, e).eye.x, 50.0f, 0.05f);
  EXPECT_FLOAT_EQ(cameraOf(registry, e).eye.y, 0.0f);

  // One rule everywhere: a parked lane writes nothing.
  EXPECT_EQ(world::resolveAnimation(registry).cameras, 0)
      << "an unmoved path parameter must not write";

  along = 1.0f;
  EXPECT_EQ(world::resolveAnimation(registry).cameras, 1);
  EXPECT_NEAR(cameraOf(registry, e).eye.x, 100.0f, 0.05f);
}

TEST(WorldAnimation, CameraPathArcLengthIsConstantSpeedByDefault) {
  // A camera move wants constant SPEED, so arc-length parameterisation
  // is the default and the flag is the opt-out.
  entt::registry registry;
  const entt::entity e = registry.create();
  registry.emplace<world::CameraComponent>(e);
  choreograph::Output<float> along{0.0f};
  auto& animated = registry.emplace<world::AnimatedCamera>(e);
  world::CameraPath& flight = animated.path.emplace();
  flight.path = unevenRun();
  flight.t = &along;
  flight.lookAhead = 0;

  const auto walk = [&] {
    std::vector<float> steps;
    float previous = 0;
    for (int i = 0; i <= 4; ++i) {
      along = (float)i / 4.0f;
      world::resolveAnimation(registry);
      const float x = cameraOf(registry, e).eye.x;
      if (i > 0) steps.push_back(x - previous);
      previous = x;
    }
    return steps;
  };

  const std::vector<float> even = walk();
  ASSERT_EQ(even.size(), 4u);
  for (const float step : even)
    EXPECT_NEAR(step, 25.0f, 0.2f) << "arc-length walks at one speed";

  flight.arcLength = false;
  const std::vector<float> raw = walk();
  ASSERT_EQ(raw.size(), 4u);
  // Not vacuous: the same curve, parameter-uniform, crawls then sprints.
  EXPECT_NEAR(raw[0], 5.0f, 0.05f);
  EXPECT_NEAR(raw[3], 45.0f, 0.05f);
}

TEST(WorldAnimation, CameraPathAimsAheadUnlessLookAheadIsZero) {
  // PRECEDENCE, target half: the path owns the target if and only if it
  // was ASKED to aim. lookAhead = 0 is the spelling of "I aim it
  // myself", and then the target lanes stand untouched.
  entt::registry registry;
  const entt::entity e = registry.create();
  shape::space::Camera authored;
  authored.target = {0, 90, 0};
  registry.emplace<world::CameraComponent>(e, authored);
  choreograph::Output<float> along{0.25f};
  auto& animated = registry.emplace<world::AnimatedCamera>(e);
  animated.targetY = 42.0f;
  world::CameraPath& flight = animated.path.emplace();
  flight.path = unevenRun();
  flight.t = &along;
  flight.lookAhead = 0.05f;

  EXPECT_EQ(world::resolveAnimation(registry).cameras, 1);
  {
    const shape::space::Camera& cam = cameraOf(registry, e);
    EXPECT_NEAR(cam.eye.x, 25.0f, 0.05f);
    // 5% of a 100-unit run ahead of the eye, and the aiming path owns
    // the target outright, so the 42 lane does NOT get a say.
    EXPECT_NEAR(cam.target.x, 30.0f, 0.1f);
    EXPECT_NEAR(cam.target.y, 0.0f, 1e-4f)
        << "an aiming path drives the whole target";
  }

  flight.lookAhead = 0;
  EXPECT_EQ(world::resolveAnimation(registry).cameras, 1);
  {
    const shape::space::Camera& cam = cameraOf(registry, e);
    EXPECT_NEAR(cam.eye.x, 25.0f, 0.05f) << "the eye is still flown";
    EXPECT_FLOAT_EQ(cam.target.y, 42.0f)
        << "lookAhead = 0 hands the target back to the lanes";
  }
}

TEST(WorldAnimation, CameraPathOutranksTheEyeLanes) {
  // PRECEDENCE, eye half: whatever the path drives, it drives outright.
  // A lane that half-contradicts a curve could only place the camera off
  // it, so the eye lanes are ignored while a path is engaged.
  entt::registry registry;
  const entt::entity e = registry.create();
  registry.emplace<world::CameraComponent>(e);
  choreograph::Output<float> along{0.5f};
  auto& animated = registry.emplace<world::AnimatedCamera>(e);
  animated.eyeX = 999.0f;
  animated.eyeY = 777.0f;
  animated.eyeZ = 555.0f;
  animated.targetZ = 7.0f;
  world::CameraPath& flight = animated.path.emplace();
  flight.path = unevenRun();
  flight.t = &along;
  flight.lookAhead = 0;

  EXPECT_EQ(world::resolveAnimation(registry).cameras, 1);
  const shape::space::Camera& cam = cameraOf(registry, e);
  EXPECT_NEAR(cam.eye.x, 50.0f, 0.05f) << "the curve places the eye";
  EXPECT_FLOAT_EQ(cam.eye.y, 0.0f);
  EXPECT_FLOAT_EQ(cam.eye.z, 0.0f);
  EXPECT_FLOAT_EQ(cam.target.z, 7.0f)
      << "a non-aiming path leaves the target lanes alone";

  // And it is the PATH that outranks, not merely a later write: drop it
  // and the very same lanes take over.
  animated.path.reset();
  EXPECT_EQ(world::resolveAnimation(registry).cameras, 1);
  EXPECT_FLOAT_EQ(cam.eye.x, 999.0f);
  EXPECT_FLOAT_EQ(cam.eye.y, 777.0f);
}

TEST(WorldAnimation, CameraPathWrapsOnALoopAndClampsOnAnOpenCurve) {
  // THE WRAP RULE. A closed curve comes round (0 and 1 are the same
  // point, so a hard stop mid-loop is never what "closed" meant); an
  // open one parks at its ends.
  entt::registry registry;
  const entt::entity e = registry.create();
  registry.emplace<world::CameraComponent>(e);
  choreograph::Output<float> along{0.25f};
  auto& animated = registry.emplace<world::AnimatedCamera>(e);
  world::CameraPath& flight = animated.path.emplace();
  flight.path = diamondLoop();
  flight.t = &along;
  flight.lookAhead = 0.05f;

  world::resolveAnimation(registry);
  const glm::vec3 quarter = cameraOf(registry, e).eye;
  along = 1.25f;
  world::resolveAnimation(registry);
  EXPECT_LT(glm::length(cameraOf(registry, e).eye - quarter), 1e-3f)
      << "a lap and a quarter is a quarter";
  along = -0.75f;
  world::resolveAnimation(registry);
  EXPECT_LT(glm::length(cameraOf(registry, e).eye - quarter), 1e-3f)
      << "negative parameters run backwards round the loop";

  // The look-ahead wraps too, so the aim reads ACROSS the seam instead
  // of staring at the end of the loop while flying past it: on a smooth
  // arc-length loop the aim chord is the same length everywhere.
  along = 0.48f;
  world::resolveAnimation(registry);
  const float midChord =
      glm::length(cameraOf(registry, e).target - cameraOf(registry, e).eye);
  along = 0.98f;
  world::resolveAnimation(registry);
  const float seamChord =
      glm::length(cameraOf(registry, e).target - cameraOf(registry, e).eye);
  EXPECT_GT(midChord, 1.0f) << "the fixture must actually aim somewhere";
  EXPECT_NEAR(seamChord, midChord, midChord * 0.05f)
      << "the aim must read across the seam of a closed loop";

  // Open: both ends park.
  flight.path = unevenRun();
  along = 1.5f;
  world::resolveAnimation(registry);
  EXPECT_NEAR(cameraOf(registry, e).eye.x, 100.0f, 0.05f);
  along = -0.5f;
  world::resolveAnimation(registry);
  EXPECT_NEAR(cameraOf(registry, e).eye.x, 0.0f, 0.05f);
}

TEST(WorldAnimation, CameraPathTableRebuildsOnlyWhenTheSplineChanges) {
  // The arc-length table costs `samples` spline evaluations to build and
  // must not be rebuilt per frame. There is no dirty flag; the cache is
  // compared against the SPLINE that determines it. So this test
  // POISONS the table and shows that a resolve keeps the poison — proof
  // it did not rebuild — then edits a control point and shows that the
  // very same resolve does rebuild.
  entt::registry registry;
  const entt::entity e = registry.create();
  registry.emplace<world::CameraComponent>(e);
  choreograph::Output<float> along{0.5f};
  auto& animated = registry.emplace<world::AnimatedCamera>(e);
  world::CameraPath& flight = animated.path.emplace();
  flight.path = unevenRun();
  flight.t = &along;
  flight.lookAhead = 0;

  world::resolveAnimation(registry);
  EXPECT_NEAR(cameraOf(registry, e).eye.x, 50.0f, 0.05f);
  EXPECT_EQ(flight.tableSamples, 256);
  EXPECT_NEAR(flight.arcTable.back(), 100.0f, 0.05f);

  // Poison: a flat table has no extent, so the resolver falls back to
  // the raw parameter — x = 10 at t = 0.5 on this uneven run.
  std::fill(flight.arcTable.begin(), flight.arcTable.end(), 0.0f);
  world::resolveAnimation(registry);
  EXPECT_NEAR(cameraOf(registry, e).eye.x, 10.0f, 0.05f)
      << "an unchanged spline must not rebuild its table";

  // Edit a control point in place: the cache no longer matches the
  // spline, so it rebuilds. There is no flag anyone could forget.
  flight.path.points[2] = {200, 0, 0};
  world::resolveAnimation(registry);
  EXPECT_NEAR(flight.arcTable.back(), 200.0f, 0.05f);
  EXPECT_NEAR(cameraOf(registry, e).eye.x, 100.0f, 0.1f)
      << "half of a 200-unit run";
}

TEST(WorldAnimation, CameraPathRollTurnsAboutTheFlightAxis) {
  // Roll composes with the path rather than fighting it: rollDeg
  // resolves AFTER the path, about the eye->target axis the path just
  // produced, so a dutch tilt follows the curve round.
  entt::registry registry;
  const entt::entity e = registry.create();
  shape::space::Camera authored;
  authored.eye = {0, 0, 10};
  authored.target = {0, 0, 0};  // forward -z, if roll ran before the path
  registry.emplace<world::CameraComponent>(e, authored);
  choreograph::Output<float> along{0.0f};
  auto& animated = registry.emplace<world::AnimatedCamera>(e);
  animated.rollDeg = 90.0f;
  world::CameraPath& flight = animated.path.emplace();
  flight.path = diamondLoop();
  flight.t = &along;
  flight.lookAhead = 0.005f;

  EXPECT_EQ(world::resolveAnimation(registry).cameras, 1);
  const shape::space::Camera& cam = cameraOf(registry, e);
  // At s = 0 the loop passes (100,0,0) along its Catmull-Rom tangent,
  // the centred difference of the neighbouring knots: +z. Rolling
  // world-up 90 degrees right-handed about +z lands up on -x.
  EXPECT_NEAR(cam.up.x, -1.0f, 5e-2f);
  EXPECT_NEAR(cam.up.y, 0.0f, 5e-2f);
  EXPECT_NEAR(glm::length(cam.up), 1.0f, 1e-5f);
  // Rolling about the AUTHORED axis instead — the -z from eye (0,0,10)
  // to the origin — would have put up on +x. That whole sign is the
  // difference this test exists to see.
  EXPECT_LT(cam.up.x, 0.0f);

  // Still idempotent with a path in front of it.
  EXPECT_EQ(world::resolveAnimation(registry).cameras, 0);
  EXPECT_NEAR(glm::length(cam.up), 1.0f, 1e-5f);
}

TEST(WorldAnimation, CameraPathTraceIsTheSameAtTheSameFrameIndex) {
  // DETERMINISM for the path, to the same three-part standard: same
  // frame index gives the same numbers bit for bit; the trace must MOVE;
  // and a different dt sequence must land elsewhere at the same index,
  // so "identical" is not a claim about a constant.
  const auto run = [](double dt, int frames) {
    motion::Ticker ticker;
    choreograph::Output<float> phase{0.0f};
    ticker.timeline().apply(&phase).then<choreograph::RampTo>(1.0f, 1.0f);
    entt::registry registry;
    const entt::entity e = registry.create();
    registry.emplace<world::CameraComponent>(e);
    auto& animated = registry.emplace<world::AnimatedCamera>(e);
    animated.rollDeg = world::bind(&phase).target(0.0f, 20.0f);
    world::CameraPath& flight = animated.path.emplace();
    flight.path = diamondLoop();
    flight.t = world::bind(&phase).map(&choreograph::easeInOutQuad);
    flight.lookAhead = 0.05f;
    std::vector<float> trace;
    for (int i = 0; i < frames; ++i) {
      ticker.tick(dt);
      world::resolveAnimation(registry);
      const shape::space::Camera& cam = cameraOf(registry, e);
      trace.push_back(cam.eye.x);
      trace.push_back(cam.eye.z);
      trace.push_back(cam.target.x);
      trace.push_back(cam.up.x);
    }
    return trace;
  };

  const std::vector<float> a = run(1.0 / 60.0, 60);
  const std::vector<float> b = run(1.0 / 60.0, 60);
  ASSERT_EQ(a.size(), b.size());
  for (size_t i = 0; i < a.size(); ++i)
    EXPECT_EQ(a[i], b[i]) << "sample " << i << " must be bit-identical";

  EXPECT_NE(a.front(), a[a.size() - 4]) << "the flight must actually move";
  const std::vector<float> fast = run(1.0 / 30.0, 60);
  EXPECT_NE(a[40], fast[40]);
}

TEST(WorldAnimation, CameraPathFrameRendersIdenticallyAcrossRuns) {
  // Determinism in pixels: the path reaches the renderer, the same frame
  // index gives the same pixels, and the next frame does not — so the
  // flight is genuinely under way rather than parked.
  world::WorldConfig config;
  config.width = 96;
  config.height = 72;
  config.clearColor = {0, 0, 0, 1};

  const auto runTo = [&](int frames, SkBitmap* out) {
    MAKE_WORLD_OR_SKIP(w, config);
    ASSERT_NE(w->addSurface(shape::mesh::quad(120, 120), glm::mat4(1.0f),
                            world::Material{}),
              0u);
    entt::registry& registry = w->registry();
    const entt::entity cam = registry.create();
    registry.emplace<world::CameraComponent>(cam);

    motion::Ticker ticker;
    choreograph::Output<float> phase{0.0f};
    ticker.timeline().apply(&phase).then<choreograph::RampTo>(1.0f, 1.0f);
    auto& animated = registry.emplace<world::AnimatedCamera>(cam);
    world::CameraPath& flight = animated.path.emplace();
    flight.path.points = {{0, 40, 520}, {60, 80, 400}, {0, 20, 300}};
    flight.path.type = shape::Spline3::Type::CatmullRom;
    flight.t = world::bind(&phase).target(0.0f, 0.8f);
    flight.lookAhead = 0.05f;

    for (int i = 0; i < frames; ++i) {
      ticker.tick(1.0 / 60.0);
      ASSERT_TRUE(w->render());
    }
    sk_sp<SkImage> frame = w->readback();
    ASSERT_TRUE(frame);
    out->allocPixels(SkImageInfo::MakeN32Premul(config.width, config.height));
    ASSERT_TRUE(frame->readPixels(nullptr, out->pixmap(), 0, 0));
  };

  SkBitmap first, second, later;
  runTo(24, &first);
  if (first.isNull()) GTEST_SKIP() << "no 3D backend";
  runTo(24, &second);
  runTo(25, &later);

  ASSERT_EQ(first.computeByteSize(), second.computeByteSize());
  EXPECT_EQ(std::memcmp(first.getPixels(), second.getPixels(),
                        first.computeByteSize()),
            0)
      << "frame 24 of a flown camera must render identically across runs";
  EXPECT_NE(std::memcmp(first.getPixels(), later.getPixels(),
                        first.computeByteSize()),
            0)
      << "the camera must actually be flying";
}

// ---------------------------------------------------------------------------
// SCENE x ANIMATION — what happens to an Animated* component on an
// entity the reconciler manages.
//
// Three answers. A kept leaf rides its entity and keeps its lanes, but
// those lanes then OUTRANK the node's declared placement. A leaf whose
// material or mesh changed is remove-and-add, so its lanes are destroyed
// along with the entity. And the camera composes freely, because a
// camera is not a scene node.
//
// These three reach for their entity by iterating the registry rather
// than through Scene::find(), deliberately: they exercise the underlying
// mechanics independently of the lookup, which has its own tests below.
// ---------------------------------------------------------------------------

namespace {
/** The registry's one and only surface entity, found by iteration so
 *  these tests do not depend on Scene::find() being correct. */
entt::entity soleSurfaceEntity(world::World& world) {
  entt::entity found = entt::null;
  int count = 0;
  for (const entt::entity e :
       world.registry().view<world::TransformComponent>()) {
    found = e;
    ++count;
  }
  EXPECT_EQ(count, 1) << "the fixture expects exactly one surface";
  return found;
}
}  // namespace

TEST(WorldSceneAnimation, KeptLeavesRideTheirEntityAndTheirLanesOutrankIt) {
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  auto mesh = std::make_shared<const shape::Mesh>(shape::mesh::quad(40, 40));
  const auto describe = [&] {
    return world::scene::group().key("set").child(
        world::scene::surface(mesh, world::Material{})
            .key("card")
            .at({100, 0, 0}));
  };
  world::scene::Scene scene(*w);
  ASSERT_EQ(scene.render(describe()).added, 1);

  const entt::entity e = soleSurfaceEntity(*w);
  ASSERT_TRUE(e != entt::null);
  choreograph::Output<float> lift{0.0f};
  w->registry().emplace<world::AnimatedTransform>(e).y = &lift;

  // The entity is kept, so the component rides along.
  const world::scene::Scene::Stats stats = scene.render(describe());
  EXPECT_EQ(stats.kept, 1);
  EXPECT_EQ(stats.added, 0);
  EXPECT_EQ(stats.removed, 0);
  ASSERT_TRUE(w->registry().valid(e));
  ASSERT_TRUE(w->registry().all_of<world::AnimatedTransform>(e));

  // ...and then WINS. AnimatedTransform owns the whole placement, so the
  // node's declared x = 100 is gone: the surface sits at the lane's own
  // origin plus lift, and the reconciler still reports `kept` while the
  // surface is nowhere the tree says it is. This is the rule that a
  // component owning a TransformComponent must not also be driven by
  // hand, met by a reconciler that drives it by hand.
  lift = 55.0f;
  ASSERT_TRUE(w->render());
  const glm::mat4& model =
      w->registry().get<world::TransformComponent>(e).model;
  EXPECT_FLOAT_EQ(model[3][1], 55.0f);
  EXPECT_FLOAT_EQ(model[3][0], 0.0f)
      << "the lane overrides the node's declared placement";
}

TEST(WorldSceneAnimation, AChangedLeafRecreatesTheEntityAndDropsItsLanes) {
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  auto mesh = std::make_shared<const shape::Mesh>(shape::mesh::quad(40, 40));
  const auto describe = [&](const world::Material& material) {
    return world::scene::group().key("set").child(
        world::scene::surface(mesh, material).key("card"));
  };
  world::scene::Scene scene(*w);
  ASSERT_EQ(scene.render(describe({})).added, 1);

  const entt::entity e = soleSurfaceEntity(*w);
  ASSERT_TRUE(e != entt::null);
  choreograph::Output<float> lift{20.0f};
  w->registry().emplace<world::AnimatedTransform>(e).y = &lift;
  EXPECT_EQ(world::resolveAnimation(w->registry()).transforms, 1);

  // Change the material: reuse needs the mesh pointer AND the material
  // to compare equal, so this leaf is remove-and-add — a NEW entity.
  world::Material recoloured;
  recoloured.baseColor = {1, 0, 0, 1};
  const world::scene::Scene::Stats stats = scene.render(describe(recoloured));
  EXPECT_EQ(stats.removed, 1);
  EXPECT_EQ(stats.added, 1);

  EXPECT_FALSE(w->registry().valid(e))
      << "the old entity is destroyed, not reused in place";
  EXPECT_EQ(w->registry().view<world::AnimatedTransform>().size(), 0u)
      << "the lane went with it — silently";
  // The whole system now resolves nothing at all: the animation simply
  // stops, with no error anywhere. Re-attach after a recreate.
  EXPECT_EQ(world::resolveAnimation(*w).transforms, 0);
  const entt::entity replacement = soleSurfaceEntity(*w);
  EXPECT_NE(replacement, e);
  EXPECT_FALSE(w->registry().all_of<world::AnimatedTransform>(replacement));
}

TEST(WorldSceneAnimation, CameraLanesAreUntouchedByReconciliation) {
  // The one place the two systems compose freely, and by construction: a
  // camera is not a scene node, so no reconcile can add, remove or
  // recreate it. A declared dolly over a declared scene is safe.
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  entt::registry& registry = w->registry();
  const entt::entity cam = registry.create();
  registry.emplace<world::CameraComponent>(cam);
  choreograph::Output<float> dolly{0.0f};
  auto& animated = registry.emplace<world::AnimatedCamera>(cam);
  animated.eyeZ = world::bind(&dolly).target(800.0f, 200.0f);
  animated.targetZ = 0.0f;

  auto mesh = std::make_shared<const shape::Mesh>(shape::mesh::quad(40, 40));
  world::scene::Scene scene(*w);
  const auto describe = [&](const world::Material& material) {
    return world::scene::group().key("set").child(
        world::scene::surface(mesh, material).key("card"));
  };
  ASSERT_EQ(scene.render(describe({})).added, 1);

  dolly = 0.5f;
  ASSERT_TRUE(w->render());
  EXPECT_FLOAT_EQ(registry.get<world::CameraComponent>(cam).camera.eye.z,
                  500.0f);

  // Churn the scene hard: recreate the leaf, then drop it entirely.
  world::Material recoloured;
  recoloured.baseColor = {0, 1, 0, 1};
  ASSERT_EQ(scene.render(describe(recoloured)).added, 1);
  scene.clear();
  dolly = 1.0f;
  ASSERT_TRUE(w->render());
  ASSERT_TRUE(registry.valid(cam));
  ASSERT_TRUE(registry.all_of<world::AnimatedCamera>(cam));
  EXPECT_FLOAT_EQ(registry.get<world::CameraComponent>(cam).camera.eye.z,
                  200.0f);
}

// ---------------------------------------------------------------------------
// Scene::find() — publishing identity, and warning on the silent
// outrank. Four claims: (a) find() resolves the reconciler's own key
// path to the kept entity, (b) a recreate's component drop is
// observable, because find() then answers the NEW entity, (c) a lane
// attached through find() animates, and (d) a kept-and-outranked leaf
// warns once while a clean keep stays silent.
//
// These need a device — a Scene requires a World, and identity is only
// observable through Scene::render() — so they skip without one.
// ---------------------------------------------------------------------------

TEST(WorldSceneAnimation, FindResolvesANestedPathAcrossAKeep) {
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  auto mesh = std::make_shared<const shape::Mesh>(shape::mesh::quad(40, 40));
  const auto describe = [&] {
    return world::scene::group().key("comp").child(
        world::scene::group()
            .key("hud")
            .at({0, 60, 0})
            .child(world::scene::surface(mesh, world::Material{})
                       .key("card")
                       .at({-420, 0, 0})));
  };
  world::scene::Scene scene(*w);
  ASSERT_EQ(scene.render(describe()).added, 1);

  const std::optional<entt::entity> found = scene.find("comp/hud/card");
  ASSERT_TRUE(found.has_value());
  EXPECT_TRUE(w->registry().valid(*found));
  EXPECT_TRUE(w->registry().all_of<world::TransformComponent>(*found))
      << "find() hands out the entity that carries the node's components";
  EXPECT_EQ(*found, soleSurfaceEntity(*w));
  // The internal spelling (leading '/') is the same path.
  EXPECT_EQ(scene.find("/comp/hud/card"), found);

  // A KEEP: an identical re-render keeps the entity, and find() agrees.
  const world::scene::Scene::Stats stats = scene.render(describe());
  EXPECT_EQ(stats.kept, 1);
  EXPECT_EQ(stats.added, 0);
  EXPECT_EQ(scene.find("comp/hud/card"), found);

  // CONTROL: scrambled or partial keys resolve nothing — and groups
  // have no entity, so a group path is empty too. Never a throw.
  EXPECT_FALSE(scene.find("hud/comp/card").has_value());
  EXPECT_FALSE(scene.find("card").has_value());
  EXPECT_FALSE(scene.find("comp/hud").has_value());
  EXPECT_FALSE(scene.find("").has_value());
}

TEST(WorldSceneAnimation, FindReturnsTheNewEntityAfterARecreate) {
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  auto meshA = std::make_shared<const shape::Mesh>(shape::mesh::quad(40, 40));
  auto meshB = std::make_shared<const shape::Mesh>(shape::mesh::quad(60, 20));
  const auto describe = [&](std::shared_ptr<const shape::Mesh> mesh) {
    return world::scene::group().key("set").child(
        world::scene::surface(std::move(mesh), world::Material{}).key("card"));
  };
  world::scene::Scene scene(*w);
  ASSERT_EQ(scene.render(describe(meshA)).added, 1);

  const std::optional<entt::entity> before = scene.find("set/card");
  ASSERT_TRUE(before.has_value());
  choreograph::Output<float> lift{10.0f};
  w->registry().emplace<world::AnimatedTransform>(*before).y = &lift;

  // Changed geometry: a different mesh pointer is remove + add.
  const world::scene::Scene::Stats stats = scene.render(describe(meshB));
  EXPECT_EQ(stats.removed, 1);
  EXPECT_EQ(stats.added, 1);

  // The drop is OBSERVABLE: the old entity is gone from the registry and
  // find() answers the new one, so a caller can re-attach its lanes.
  EXPECT_FALSE(w->registry().valid(*before));
  const std::optional<entt::entity> after = scene.find("set/card");
  ASSERT_TRUE(after.has_value());
  ASSERT_TRUE(w->registry().valid(*after));
  EXPECT_NE(*after, *before);
  // CONTROL against id aliasing: the registry may reuse the destroyed
  // slot's INDEX, but its version bumps, so the full id — the uint32
  // World hands out — still differs and a stale id cannot pass for the
  // new entity.
  EXPECT_NE((uint32_t)*after, (uint32_t)*before);
  EXPECT_FALSE(w->registry().all_of<world::AnimatedTransform>(*after))
      << "the lane died with the old entity; the replacement is bare";
  EXPECT_EQ(w->registry().view<world::AnimatedTransform>().size(), 0u);
}

TEST(WorldSceneAnimation, ALaneAttachedThroughFindAnimates) {
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  auto mesh = std::make_shared<const shape::Mesh>(shape::mesh::quad(40, 40));
  world::scene::Scene scene(*w);
  ASSERT_EQ(scene
                .render(world::scene::group().key("set").child(
                    world::scene::surface(mesh, world::Material{}).key("card")))
                .added,
            1);

  // The supported way to reach a declared leaf: ask the Scene, attach,
  // resolve.
  const std::optional<entt::entity> e = scene.find("set/card");
  ASSERT_TRUE(e.has_value());
  choreograph::Output<float> lift{0.0f};
  w->registry().emplace<world::AnimatedTransform>(*e).y = &lift;

  lift = 33.0f;
  EXPECT_EQ(world::resolveAnimation(w->registry()).transforms, 1);
  const glm::mat4& model =
      w->registry().get<world::TransformComponent>(*e).model;
  EXPECT_FLOAT_EQ(model[3][1], 33.0f);
  // Idempotent, like every resolve: a second pass writes nothing at all.
  EXPECT_EQ(world::resolveAnimation(w->registry()).transforms, 0);
}

TEST(WorldSceneAnimation, AKeptOutrankedLeafWarnsOnceAndACleanKeepIsSilent) {
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  auto mesh = std::make_shared<const shape::Mesh>(shape::mesh::quad(40, 40));
  const auto describe = [&] {
    return world::scene::group()
        .key("set")
        .child(world::scene::surface(mesh, world::Material{})
                   .key("card")
                   .at({100, 0, 0}))
        .child(world::scene::surface(mesh, world::Material{})
                   .key("clean")
                   .at({-100, 0, 0}));
  };
  world::scene::Scene scene(*w);
  ASSERT_EQ(scene.render(describe()).added, 2);

  const std::optional<entt::entity> card = scene.find("set/card");
  ASSERT_TRUE(card.has_value());
  choreograph::Output<float> lift{0.0f};
  w->registry().emplace<world::AnimatedTransform>(*card).y = &lift;

  const auto count = [](const std::string& hay, const char* needle) {
    size_t n = 0;
    const std::string want(needle);
    for (size_t pos = hay.find(want); pos != std::string::npos;
         pos = hay.find(want, pos + want.size()))
      ++n;
    return n;
  };

  // Two kept renders with a live outranking lane. The warning names the
  // node and the component ONCE, not once per frame, and never names the
  // clean sibling kept right beside it.
  ::testing::internal::CaptureStderr();
  EXPECT_EQ(scene.render(describe()).kept, 2);
  EXPECT_EQ(scene.render(describe()).kept, 2);
  const std::string err = ::testing::internal::GetCapturedStderr();
  EXPECT_EQ(count(err, "/set/card"), 1u) << err;
  EXPECT_EQ(count(err, "OUTRANKS"), 1u) << err;
  EXPECT_NE(err.find("AnimatedTransform"), std::string::npos) << err;
  EXPECT_NE(err.find("Scene::find()"), std::string::npos) << err;
  EXPECT_EQ(count(err, "/set/clean"), 0u)
      << "a clean keep must stay silent: " << err;

  // CONTROL: with the lane gone the same keep says nothing new — the
  // node was already warned — and a fresh clean scene never speaks.
  w->registry().remove<world::AnimatedTransform>(*card);
  world::scene::Scene fresh(*w);
  ::testing::internal::CaptureStderr();
  scene.render(describe());
  ASSERT_EQ(fresh
                .render(world::scene::group().key("solo").child(
                    world::scene::surface(mesh, world::Material{}).key("pane")))
                .added,
            1);
  fresh.render(world::scene::group().key("solo").child(
      world::scene::surface(mesh, world::Material{}).key("pane")));
  const std::string silent = ::testing::internal::GetCapturedStderr();
  EXPECT_EQ(count(silent, "OUTRANKS"), 0u) << silent;
}

// ---------------------------------------------------------------------------
// LAYERS AT DEPTH — a stack of flat panels at different depths, which is
// how a compositing-style layer stack is expressed here.
//
// The parallax and Stack-arithmetic tests are DEVICE-FREE on purpose,
// like the resolveAnimation() tests above: they run on a machine with no
// GPU, where every device test here skips. The reconciler test at the
// end does need a device, because identity is only observable through
// Scene::render().
// ---------------------------------------------------------------------------

TEST(WorldLayers, NearLayersShiftMoreThanFarOnesInTheDepthRatio) {
  // Parallax is ordinary perspective: for one lateral eye move d, a
  // layer at view depth z shifts f*d/z on screen — so the near layer
  // outruns the far one in the ratio z_far/z_near. No device, no
  // renderer: two projections through space::Camera::viewProjection().
  const SkSize viewport = SkSize::Make(1920, 1080);
  const auto projectX = [&](glm::vec3 eye, glm::vec3 point) {
    shape::space::Camera cam;
    cam.eye = eye;
    cam.target = eye + glm::vec3{0, 0, -100};  // a truck: view axis fixed
    const glm::vec4 out = cam.viewProjection(viewport) * glm::vec4(point, 1);
    return out.x / out.w;
  };
  const glm::vec3 eyeA = {0, 0, 300}, eyeB = {50, 0, 300};
  const glm::vec3 nearLayer = {0, 0, -300};  // view depth 600
  const glm::vec3 farLayer = {0, 0, -900};   // view depth 1200
  const float nearShift = projectX(eyeB, nearLayer) - projectX(eyeA, nearLayer);
  const float farShift = projectX(eyeB, farLayer) - projectX(eyeA, farLayer);
  // The truck must actually displace both layers on screen.
  EXPECT_GT(std::abs(nearShift), 1.0f);
  EXPECT_GT(std::abs(farShift), 1.0f);
  // The tolerance absorbs float round-off through the view and
  // projection matrices while still separating this ratio of 2 from the
  // control's ratio of 1 by a wide margin.
  EXPECT_NEAR(nearShift / farShift, 1200.0f / 600.0f, 0.02f);

  // POSITIVE CONTROL: a stack at ONE Z displaces uniformly — ratio 1 —
  // so the depth ratio above is measuring depth, not the projection.
  const glm::vec3 left = {-200, 0, -300}, right = {200, 0, -300};
  const float leftShift = projectX(eyeB, left) - projectX(eyeA, left);
  const float rightShift = projectX(eyeB, right) - projectX(eyeA, right);
  EXPECT_GT(std::abs(leftShift), 1.0f);
  EXPECT_NEAR(leftShift / rightShift, 1.0f, 0.01f);
}

namespace {

sk_sp<SkImage> solidImage(int width, int height) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
  surface->getCanvas()->clear(SK_ColorWHITE);
  return surface->makeImageSnapshot();
}

}  // namespace

TEST(WorldLayers, StackSizesLayersInTheirPixelRatio) {
  // ONE density for the stack: two images of different pixel sizes come
  // out at world sizes in the exact ratio of their pixel sizes, and
  // each layer's aspect is its image's aspect by construction.
  const sk_sp<SkImage> wide = solidImage(640, 360);
  const sk_sp<SkImage> square = solidImage(320, 320);
  const world::scene::Stack stack{32.0f};
  const SkSize a = stack.size(*wide);
  const SkSize b = stack.size(*square);
  EXPECT_FLOAT_EQ(a.width() / b.width(), 640.0f / 320.0f);
  EXPECT_FLOAT_EQ(a.height() / b.height(), 360.0f / 320.0f);
  EXPECT_FLOAT_EQ(a.width() / a.height(), 640.0f / 360.0f);
  EXPECT_FLOAT_EQ(a.width(), 20.0f);  // 640 px / 32 px-per-wu

  // POSITIVE CONTROL: hand-picking panel dimensions instead — here a
  // square panel for a widescreen image — fails the aspect equality, so
  // the guarantee above is not vacuous.
  const float handTunedAspect = 300.0f / 300.0f;
  EXPECT_NE(handTunedAspect, 640.0f / 360.0f);
}

TEST(WorldLayers, StackPanelsReconcileAsKeepsAgainstExplicitPanels) {
  // The Stack is arithmetic, not a new node kind: a Stack panel and an
  // explicit panel(w, h) with the same numbers are the IDENTICAL
  // surface, so re-describing one as the other is a keep.
  world::WorldConfig config;
  config.width = 96;
  config.height = 72;
  MAKE_WORLD_OR_SKIP(w, config);
  const sk_sp<SkImage> image = solidImage(384, 256);
  world::scene::Scene scene(*w);

  const world::scene::Stack stack{2.0f};  // 192 x 128 wu
  world::scene::Scene::Stats stats =
      scene.render(world::scene::group().key("comp").child(
          stack.panel(image).key("card").at({0, 0, -50})));
  EXPECT_EQ(stats.added, 1);

  stats = scene.render(world::scene::group().key("comp").child(
      world::scene::panel(image, 384 / 2.0f, 256 / 2.0f)
          .key("card")
          .at({0, 0, -50})));
  EXPECT_EQ(stats.kept, 1) << "same arithmetic, same surface";
  EXPECT_EQ(stats.added, 0);
  EXPECT_EQ(stats.removed, 0);
  EXPECT_EQ(stats.moved, 0);

  // POSITIVE CONTROL: a different density is a different quad, so the
  // reconciler recreates — which means the keep above really was
  // measuring identity.
  const world::scene::Stack denser{4.0f};
  stats = scene.render(world::scene::group().key("comp").child(
      denser.panel(image).key("card").at({0, 0, -50})));
  EXPECT_EQ(stats.removed, 1);
  EXPECT_EQ(stats.added, 1);
  EXPECT_EQ(stats.kept, 0);
}
