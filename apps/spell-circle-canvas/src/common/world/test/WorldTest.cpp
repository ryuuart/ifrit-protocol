#include "sigilworld/Scene.h"
#include "sigilworld/World.h"

#include <sigilshape/Mesh.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>

using namespace sigil;

namespace {

/** Device bring-up needs a Vulkan runtime (MoltenVK); tests skip — not
 *  fail — when the machine has none, so CI without a GPU stays green. */
#define MAKE_WORLD_OR_SKIP(w, config)                                       \
  std::unique_ptr<world::World> w;                                          \
  {                                                                         \
    std::string worldError;                                                 \
    w = world::World::create(config, &worldError);                          \
    if (!w)                                                                 \
      GTEST_SKIP() << "no 3D backend: " << worldError;                      \
  }

} // namespace

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
      w->addSurface(shape::mesh::quad(200, 200), SkM44(), material);
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
  ASSERT_NE(w->addSurface(shape::mesh::quad(220, 160), SkM44(), material),
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
  ASSERT_NE(w->addSurface(shape::mesh::quad(100, 100),
                          SkM44::Translate(0, 150, 0), material),
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
      if (SkColorGetG(bm.getColor(x, y)) > 100)
        (y < 50 ? top : bottom)++;
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

  auto mesh = std::make_shared<const shape::Mesh>(
      shape::mesh::quad(50, 50));
  world::Material red;
  red.baseColor = {1, 0, 0, 1};

  auto describe = [&](float x) {
    return world::scene::group().key("root").child(
        world::scene::group()
            .key("rig")
            .at({x, 0, 0})
            .child(world::scene::surface(mesh, red).key("card")));
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
  world::scene::Node swapped =
      world::scene::group().key("root").child(
          world::scene::group().key("rig").at({40, 0, 0}).child(
              world::scene::surface(mesh, blue).key("card")));
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
