#include "sigilworld/Components.h"
#include "sigilworld/Easel.h"
#include "sigilworld/Scene.h"
#include "sigilworld/World.h"

#include <sigilshape/Mesh.h>
#include <sigilshape/Points.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>

#include <glm/gtc/matrix_transform.hpp>

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
  ASSERT_NE(w->addSurface(shape::mesh::quad(220, 160), glm::mat4(1.0f), material),
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

TEST(World, BakedVertexColorsTintBothPipelines) {
  world::WorldConfig config;
  config.width = 200;
  config.height = 150;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);

  shape::space::Camera camera;
  camera.eye = {0, 0, 400};
  w->setCamera(camera);

  auto grab = [&](SkBitmap *bm) {
    if (!w->render())
      return false;
    sk_sp<SkImage> frame = w->readback();
    if (!frame)
      return false;
    bm->allocPixels(SkImageInfo::MakeN32Premul(200, 150));
    return frame->readPixels(nullptr, bm->pixmap(), 0, 0);
  };

  // Plain pipeline: the mesh color lane — what pop's cookMesh fades and
  // import's merged() bake — reads back directly. Left red, right blue.
  shape::Mesh mesh = shape::mesh::quad(220, 160);
  mesh.colors.resize(mesh.positions.size());
  for (size_t i = 0; i < mesh.positions.size(); ++i)
    mesh.colors[i] = mesh.positions[i].x < 0 ? glm::vec4{1, 0, 0, 1}
                                             : glm::vec4{0, 0, 1, 1};
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

  // Instanced pipeline: baked stamp color MULTIPLIES the per-instance
  // tint. Yellow stamp x cyan tint = green — a color neither input
  // shows alone, so pass-through of either would fail both counts.
  shape::Mesh stamp = shape::mesh::quad(14, 14);
  stamp.colors.assign(stamp.positions.size(), glm::vec4{1, 1, 0, 1});
  shape::Cloud field = shape::points::grid({-150, -100, 0}, {300, 0, 0},
                                           {0, 200, 0}, 10, 8);
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
        ++passthrough; // yellow (color only) or cyan (tint only)
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
  ASSERT_NE(w->addSurface(shape::mesh::quad(100, 100),
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
  ASSERT_NE(w->addSurface(shape::mesh::quad(600, 300), glm::mat4(1.0f), material),
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
  ASSERT_NE(w->addSurface(shape::mesh::quad(200, 200), glm::mat4(1.0f), material),
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
      w->addSurface(shape::mesh::quad(100, 100), glm::mat4(1.0f), material);
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

TEST(Easel, SwarmCloudEditsRefreshInstancesInPlace) {
  // The fingerprint arm SwarmsAreKeyStable leaves untested: the SAME
  // key with CHANGED cloud content must refresh instances in place
  // (setInstances), never tear the surface down and re-add it.
  world::WorldConfig config;
  config.width = 96;
  config.height = 64;
  MAKE_WORLD_OR_SKIP(w, config);

  world::easel::Stage stage = world::easel::stage(*w);
  world::Material glow;
  glow.unlit = true;

  auto describe = [&](float originX) {
    shape::Cloud cloud = shape::points::grid(
        {originX - 40, -20, 0}, {80, 0, 0}, {0, 40, 0}, 6, 4);
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

TEST(World, UvScaleOffsetSelectsTexelLiveAcrossFrames) {
  // uvScale {0,0} collapses sampling to the single texel uvOffset
  // names — filtering- and orientation-proof, and mutating the LIVE
  // MaterialComponent between frames must move it (the marquee scroll
  // contract: animation with zero texture uploads).
  world::WorldConfig config;
  config.width = 64;
  config.height = 64;
  config.clearColor = {0, 0, 0, 1};
  MAKE_WORLD_OR_SKIP(w, config);
  shape::space::Camera camera;
  camera.eye = {0, 0, 500};
  camera.target = {0, 0, 0};
  w->setCamera(camera);

  SkBitmap texels; // green | blue, texel centers u = 0.25 / 0.75
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

  auto &material =
      w->registry().get<world::MaterialComponent>(world::entity(id));
  material.material.uvOffset = {0.75f, 0.5f};
  ASSERT_TRUE(w->render());
  const SkColor second = centerColor();
  EXPECT_GT(SkColorGetB(second), 128u);
  EXPECT_LT(SkColorGetG(second), 100u);
}

TEST(World, SetSurfaceMeshMovesGeometryInPlace) {
  // The towed-flag contract: same topology updates the GPU buffers in
  // place; a different shape recreates them; material and entity
  // survive both.
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

  const auto greenRows = [&](int *top, int *bottom) {
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
  EXPECT_GT(bottom, 0); // centered quad spans both halves

  // Same topology, vertices moved up: the UpdateBuffer path.
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
  // The compute generator path: a circle loop lives on the GPU, the
  // sweep writes the ribbon's vertices in place, and sliding the
  // window (two floats) moves the arc to the other side — no CPU
  // mesh ever exists.
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
  desc.head = 0.25f; // a quarter arc ending at the loop's top-right
  desc.span = 0.25f;
  world::Material material;
  material.unlit = true;
  material.baseColor = {0, 1, 0, 1};
  const uint32_t id = w->addSweep(desc, material);
  ASSERT_NE(id, 0u);

  const auto greenHalves = [&](int *left, int *right) {
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

  w->setSweepWindow(id, 0.75f, 0.25f); // the opposite quarter
  ASSERT_TRUE(w->render());
  greenHalves(&left, &right);
  EXPECT_GT(left, 0) << "slid window must land on the -x side";
  EXPECT_EQ(right, 0);
}

TEST(World, GpuFlockStreamsAlongTheLoop) {
  // POP phase 2: the points never exist on the CPU. A window of
  // instances lands on one side of the loop; sliding it (two floats)
  // streams the whole flock to the other side.
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
  desc.radius = 0; // points exactly on the arc: a deterministic side
  desc.scale = 1;
  world::Material material;
  material.unlit = true;
  material.baseColor = {0, 1, 0, 1};
  const uint32_t id =
      w->addFlock(shape::mesh::quad(10, 10), desc, material);
  ASSERT_NE(id, 0u);

  const auto greenHalves = [&](int *left, int *right) {
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
  // The combinator path: a chain of operator VALUES cooks on the GPU.
  // Re-describing with edited fields re-cooks; a Math op pushing the
  // window's points across the frame proves the whole chain ran.
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
    scatter.loop.push_back(
        {160.0f * std::cos(a), 160.0f * std::sin(a), 0});
  }
  scatter.count = 3000;
  scatter.head = 0.125f; // an arc clear of the screen midline
  scatter.span = 0.2f;
  scatter.radius = 0;
  pop::Chain chain = {scatter, pop::LookAt{{0, 0, 500}},
                      pop::Ramp{pop::Lane::Color,
                                {0, 1, 0, 1},
                                {0, 1, 0, 1}}};
  world::Material material;
  material.unlit = true;
  material.baseColor = {1, 1, 1, 1}; // tint carries the green
  const uint32_t id =
      w->addPoints(shape::mesh::quad(10, 10), chain, material);
  ASSERT_NE(id, 0u);

  const auto greenHalves = [&](int *left, int *right) {
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

  // Nondestructive edit: append a Math op mirroring P across x. The
  // chain SHAPE changed, so lanes rebind, then the re-cook runs it.
  chain.push_back(
      pop::Math{pop::Lane::P, {-1, 1, 1, 1}, {0, 0, 0, 0}});
  w->setPoints(id, chain);
  ASSERT_TRUE(w->render());
  greenHalves(&left, &right);
  EXPECT_GT(left, 0) << "Math mul {-1,1,1} must mirror the arc to -x";
  EXPECT_EQ(right, 0);
}

TEST(World, PopCpuAndGpuExecutorsAgree) {
  // The two executors of one description: the GPU chain (compute
  // lanes -> instanced draw) and the CPU reference (popops::cookMesh
  // -> plain surface) must land the same geometry. Tint stays white
  // (the plain pipeline ignores baked vertex colors); the material
  // carries the green, so the comparison is pure P/Dir/Scale.
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

  const auto mask = [&](std::vector<bool> *out) {
    sk_sp<SkImage> frame = w->readback();
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(64, 64));
    ASSERT_TRUE(frame && frame->readPixels(nullptr, bm.pixmap(), 0, 0));
    out->assign(64 * 64, false);
    for (int y = 0; y < 64; ++y)
      for (int x = 0; x < 64; ++x)
        (*out)[(size_t)(y * 64 + x)] =
            SkColorGetG(bm.getColor(x, y)) > 100;
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
    if (gpuMask[i] || cpuMask[i])
      ++lit;
    if (gpuMask[i] != cpuMask[i])
      ++mismatched;
  }
  ASSERT_GT(lit, 100);
  // Different vertex paths (instanced VS vs pre-merged mesh) may
  // wiggle edge pixels; the SHAPES must agree.
  EXPECT_LT(mismatched, lit / 50)
      << mismatched << " of " << lit << " lit pixels disagree";
}


TEST(World, ReadPointsQueriesGpuLanesNumerically) {
  // The query door, and with it the strongest parity statement: the
  // GPU-cooked lanes read back as a Cloud must match the CPU
  // reference cook point for point, number for number.
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  std::vector<glm::vec3> loop;
  for (int i = 0; i < 12; ++i) {
    const float a = (float)i / 12.0f * 2.0f * (float)M_PI;
    loop.push_back({160.0f * std::cos(a), 30.0f * std::sin(3 * a),
                    160.0f * std::sin(a)});
  }
  const shape::pop::Chain chain =
      shape::pop::on(loop)
          .count(500)
          .window(0.8f, 0.6f)
          .spread(14)
          .seed(4)
          .vary(0.35f)
          .smooth(0.5f, 2)
          .atlas(2, 2)
          .fade({1, 0, 0, 1}, {0, 0, 1, 1});
  const uint32_t id = w->addPoints(shape::mesh::quad(4, 4), chain,
                                   world::Material{});
  ASSERT_NE(id, 0u);
  ASSERT_TRUE(w->render()); // the cook

  const shape::Cloud gpu = w->readPoints(id);
  const shape::Cloud cpu = shape::popops::cook(chain);
  ASSERT_EQ(gpu.size(), 500u);
  ASSERT_EQ(cpu.size(), 500u);
  const std::vector<float> *gpuT = gpu.scalarIf("t");
  const std::vector<glm::vec4> *gpuTint = gpu.colorIf("tint");
  const std::vector<float> *cpuT = cpu.scalarIf("t");
  const std::vector<glm::vec4> *cpuTint = cpu.colorIf("tint");
  const std::vector<glm::vec4> *gpuTex = gpu.colorIf("Tex");
  const std::vector<glm::vec4> *cpuTex = cpu.colorIf("Tex");
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
  // The animation verb: two floats must land exactly where a whole
  // setPoints re-describe with the same window lands — same kernels,
  // same seeds, zero drift between the doors.
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  std::vector<glm::vec3> loop;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    loop.push_back(
        {140.0f * std::cos(a), 40.0f * std::sin(2 * a),
         140.0f * std::sin(a)});
  }
  const auto chainAt = [&](float head, float span) {
    return (shape::pop::Chain)shape::pop::on(loop)
        .count(128)
        .window(head, span)
        .spread(9)
        .seed(6)
        .vary(0.3f);
  };

  const uint32_t id = w->addPoints(shape::mesh::quad(3, 3),
                                   chainAt(1.0f, 0.5f),
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
  // The last vocabulary gap closed: a custom named attribute gets an
  // arena slot, cooks through GPU dispatches, and reads back equal to
  // the CPU reference — the two executors now speak the whole
  // language (mesh seeding aside).
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
  const uint32_t id = w->addPoints(shape::mesh::quad(4, 4), chain,
                                   world::Material{});
  ASSERT_NE(id, 0u) << "custom-attr chains must be GPU-cookable now";
  ASSERT_TRUE(w->render());

  const shape::Cloud gpu = w->readPoints(id);
  const shape::Cloud cpu = shape::popops::cook(chain);
  const std::vector<glm::vec4> *gpuEnergy = gpu.colorIf("energy");
  const std::vector<glm::vec4> *cpuEnergy = cpu.colorIf("energy");
  ASSERT_TRUE(gpuEnergy && cpuEnergy);
  ASSERT_EQ(gpuEnergy->size(), 256u);
  for (size_t i : {size_t(0), size_t(77), size_t(255)})
    EXPECT_NEAR((*gpuEnergy)[i].r, (*cpuEnergy)[i].r, 1e-4f);
}

TEST(World, ChainsComposeOnDevice) {
  // Pops feed pops with NO CPU round trip: chain B's generator reads
  // chain A's cooked P lane straight from its arena — and the result
  // matches the CPU composing entry numerically.
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
      shape::pop::on(std::vector<glm::vec3>{}) // loop comes from upstream
          .count(300)
          .spread(6)
          .seed(9)
          .vary(0.3f);

  const uint32_t a =
      w->addPoints(shape::mesh::quad(4, 4), chainA, world::Material{});
  ASSERT_NE(a, 0u);
  const uint32_t b = w->addPointsOn(a, shape::mesh::quad(4, 4), chainB,
                                    world::Material{});
  ASSERT_NE(b, 0u);
  ASSERT_TRUE(w->render());

  const shape::Cloud gpu = w->readPoints(b);
  // The CPU composing entry over the same descriptions.
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
  // control points MOVED must cook the new loop, not the buffer the
  // original addPoints uploaded — same-shape chains included.
  world::WorldConfig config;
  config.width = 32;
  config.height = 32;
  MAKE_WORLD_OR_SKIP(w, config);

  const auto chainOn = [](float radius) {
    std::vector<glm::vec3> loop;
    for (int i = 0; i < 10; ++i) {
      const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
      loop.push_back(
          {radius * std::cos(a), 0, radius * std::sin(a)});
    }
    return (shape::pop::Chain)shape::pop::on(loop)
        .count(128)
        .spread(5)
        .seed(3);
  };

  const uint32_t id = w->addPoints(shape::mesh::quad(3, 3),
                                   chainOn(150), world::Material{});
  ASSERT_NE(id, 0u);
  ASSERT_TRUE(w->render());

  // Same ops, same count — only the loop shrank. The re-cook must
  // land where a fresh describe of the small circle lands.
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
  // Tearing down a chain's upstream must not take the dependent with
  // it: the rider keeps its last cooked lanes (its SRBs hold the
  // upstream arena alive) and the world keeps rendering.
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
      shape::pop::on(std::vector<glm::vec3>{}) // loop comes from upstream
          .count(300)
          .spread(6)
          .seed(9)
          .vary(0.3f);

  const uint32_t a =
      w->addPoints(shape::mesh::quad(4, 4), chainA, world::Material{});
  ASSERT_NE(a, 0u);
  const uint32_t b = w->addPointsOn(a, shape::mesh::quad(4, 4), chainB,
                                    world::Material{});
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
  // The dependency edge is LIVE: sliding the upstream's window (two
  // floats) must re-cook the rider in the same render, and the rider
  // must land where the CPU composing entry over the slid upstream
  // lands.
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
      shape::pop::on(std::vector<glm::vec3>{}) // loop comes from upstream
          .count(300)
          .spread(6)
          .seed(9)
          .vary(0.3f);

  const uint32_t a = w->addPoints(shape::mesh::quad(4, 4),
                                  chainAt(1.0f, 0.4f), world::Material{});
  ASSERT_NE(a, 0u);
  const uint32_t b = w->addPointsOn(a, shape::mesh::quad(4, 4), chainB,
                                    world::Material{});
  ASSERT_NE(b, 0u);
  ASSERT_TRUE(w->render());
  const shape::Cloud before = w->readPoints(b);
  ASSERT_EQ(before.size(), 300u);

  w->setPointsWindow(a, 0.5f, 0.4f);
  ASSERT_TRUE(w->render());
  const shape::Cloud slid = w->readPoints(b);
  ASSERT_EQ(slid.size(), 300u);

  // (i) The rider actually followed the upstream arc.
  float moved = 0;
  for (size_t i : {size_t(0), size_t(150), size_t(299)})
    moved += glm::length(slid.positions[i] - before.positions[i]);
  EXPECT_GT(moved, 1.0f) << "the rider must follow the slid window";

  // (ii) And it landed on the CPU composing reference over the slid
  // upstream description.
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
