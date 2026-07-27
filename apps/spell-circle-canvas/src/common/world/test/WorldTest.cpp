#include "sigilworld/Components.h"
#include "sigilworld/Easel.h"
#include "sigilworld/Scene.h"
#include "sigilworld/World.h"

#include <sigilshape/Mesh.h>
#include <sigilshape/Points.h>

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
  ASSERT_NE(w->addSurface(shape::mesh::quad(600, 300), SkM44(), material),
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
  entt::registry &registry = w->registry();
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
  ASSERT_NE(w->addSurface(shape::mesh::quad(200, 200), SkM44(), material),
            0u);

  // An active camera entity faces it; that one must drive the frame.
  shape::space::Camera facing;
  facing.eye = {0, 0, 400};
  facing.target = {0, 0, 0};
  entt::registry &registry = w->registry();
  const entt::entity camEntity = registry.create();
  registry.emplace<world::CameraComponent>(camEntity,
                                           world::CameraComponent{facing});

  auto greenPixels = [&]() {
    sk_sp<SkImage> frame = w->readback();
    if (!frame)
      return -1;
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(100, 100));
    if (!frame->readPixels(nullptr, bm.pixmap(), 0, 0))
      return -1;
    int count = 0;
    for (int y = 0; y < 100; ++y)
      for (int x = 0; x < 100; ++x)
        if (SkColorGetG(bm.getColor(x, y)) > 100)
          ++count;
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

  // An 10x8 lattice of small quads spanning the middle of the frame,
  // tinted red through the tint lane over a white unlit material.
  shape::Cloud field = shape::points::grid({-150, -100, 0}, {300, 0, 0},
                                           {0, 200, 0}, 10, 8);
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

  auto redPixels = [&](int *quadrants) {
    sk_sp<SkImage> frame = w->readback();
    if (!frame)
      return -1;
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(200, 150));
    if (!frame->readPixels(nullptr, bm.pixmap(), 0, 0))
      return -1;
    int count = 0;
    for (int i = 0; i < 4; ++i)
      quadrants[i] = 0;
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
  shape::Cloud fewer = shape::points::grid({-150, -100, 0}, {300, 0, 0},
                                           {0, 200, 0}, 2, 2);
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
      w->addSurface(shape::mesh::quad(100, 100), SkM44(), material);
  ASSERT_NE(id, 0u);

  // The id is an entity carrying the public components.
  entt::registry &registry = w->registry();
  const entt::entity e = world::entity(id);
  ASSERT_TRUE(registry.valid(e));
  ASSERT_TRUE((registry.all_of<world::TransformComponent,
                               world::MaterialComponent>(e)));

  // Systems attach their own components alongside rendering state.
  struct Velocity {
    float y;
  };
  registry.emplace<Velocity>(e, Velocity{150.0f});
  for (auto [entity, velocity, transform] :
       registry.view<Velocity, world::TransformComponent>().each())
    transform.model = SkM44::Translate(0, velocity.y, 0);

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
      if (SkColorGetG(bm.getColor(x, y)) > 100)
        (y < 50 ? top : bottom)++;
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
