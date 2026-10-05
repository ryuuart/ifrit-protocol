/** @file
 * The retained side: identity across a reorder, the three lifetimes
 * pulling apart under a geometry-slot change, the store sharing one
 * cooked artefact, lanes moving a placement, the bake taken once and
 * lost to a driven lane below it, and what a pass is handed being a
 * function of the description alone.
 *
 * NOTHING HERE DRAWS. What a pass is handed is read back through an
 * executor that keeps it as values, so the retained side is asked its
 * questions without a device; what a device does with what it is handed
 * is asked beside the device's executor.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Tween.h>
#include <sigilworld/advanced/Skia.h>
#include <sigilworld/scene/Scene.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "TestMaterial.h"

using namespace sigil;
using namespace sigil::world;
using namespace sigil::world::test;
using namespace std::chrono_literals;

namespace {

/** A flat body @p size across each way, facing the camera. */
geometry::mesh::Mesh card(float size) {
  return geometry::mesh::quad(size * 2.0f, size * 2.0f);
}

/** A closed loop and a chain of stamps riding a window of it — the same
 *  VALUE wherever it is built, which is what makes it shareable. */
geometry::mesh::pop::Chain cometChain() {
  std::vector<glm::vec3> loop = {
      {-80, 0, 0}, {0, 60, 0}, {80, 0, 0}, {0, -60, 0}};
  return geometry::mesh::pop::on(loop)
      .count(64)
      .window(0.9f, 0.4f)
      .spread(6.0f);
}

/** WHAT A PASS WAS HANDED, as values: every body's key, placement,
 *  colour and backface in the order the view lists them, and every
 *  emitter's colour and strength. Two of these compare equal exactly when
 *  two frames handed their passes the same scene. */
struct Seen {
  std::vector<std::string> keys;
  std::vector<glm::mat4> worlds;
  std::vector<glm::vec4> colours;
  std::vector<material::Backface> backfaces;
  std::vector<material::Color> lightColours;
  std::vector<float> lightStrengths;
  std::vector<Selection> realisations;
  bool operator==(const Seen&) const = default;
};

/** AN EXECUTOR THAT DRAWS NOTHING AND KEEPS WHAT IT WAS HANDED. A
 *  declared body is run, as every executor runs one, and every image it
 *  is asked for is one transparent pixel, so a readback has something to
 *  carry. */
struct Recording : Executor {
  std::shared_ptr<Seen> seen = std::make_shared<Seen>();
  void beginFrame(Targets& targets) const override {
    *seen = Seen{};
    targets.source([](std::string_view) {
      SkBitmap pixel;
      pixel.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
      pixel.eraseColor(SK_ColorTRANSPARENT);
      pixel.setImmutable();
      return pixel.asImage();
    });
  }
  void execute(const PassWork& work, const View& view,
               Targets& targets) const override {
    if (!work.pass) return;
    if (work.pass->body()) {
      work.pass->body()->run(view, targets);
      return;
    }
    seen->realisations.push_back(work.realisation);
    for (const Draw& draw : view.draws) {
      seen->keys.emplace_back(draw.key);
      seen->worlds.push_back(draw.world);
      seen->colours.push_back(draw.baseColor);
      seen->backfaces.push_back(draw.backface);
    }
    for (const material::Light& light : view.lights) {
      seen->lightColours.push_back(light.color.value());
      seen->lightStrengths.push_back(light.intensity.value());
    }
  }
  bool operator==(const Recording& other) const { return seen == other.seen; }
};

/** @p tree as a frame with one geometry pass, performed on @p recorder. */
Frame recorded(
    Element tree, const Recording& recorder,
    const geometry::mesh::camera::Camera& camera = frontCamera(320.0f)) {
  Frame frame(std::move(tree));
  frame.extent({96, 96})
      .camera(camera)
      .runtime(Runtime{recorder})
      .pass(geometryPass("colour").writes("colour"));
  return frame;
}

/** A CLOCK AND A SCENE READING IT — what nearly every case below opens
 *  with, and the only state any of them shares. */
class WorldScene : public testing::Test {
 protected:
  motion::Engine engine;
  Scene scene{engine};
};

}  // namespace

TEST_F(WorldScene, AKeyedReorderKeepsEveryNodesHandle) {
  const auto describe = [](const std::vector<std::string>& order) {
    Element root;
    root.key("root");
    for (const std::string& key : order)
      root.children({Element().key(key).mesh(card(10))});
    return root;
  };

  scene.render(describe({"a", "b", "c"}));
  const uint64_t a = scene.handleOf("a");
  const uint64_t b = scene.handleOf("b");
  const uint64_t c = scene.handleOf("c");
  EXPECT_NE(a, 0u);
  EXPECT_NE(b, 0u);
  EXPECT_NE(c, 0u);

  scene.render(describe({"c", "a", "b"}));
  EXPECT_EQ(scene.handleOf("a"), a);
  EXPECT_EQ(scene.handleOf("b"), b);
  EXPECT_EQ(scene.handleOf("c"), c);
  EXPECT_EQ(scene.stats().reconcile.mounted, 0);
  EXPECT_EQ(scene.stats().reconcile.retired, 0);
}

TEST_F(WorldScene, ABodysBackfaceReachesThePassThatDrawsIt) {
  const Recording recorder;
  const auto describe = [](material::Backface backface) {
    return Element().key("root").children(
        {Element().key("card").mesh(card(24)).backface(backface)});
  };
  scene.render(recorded(describe(material::Backface::Hidden), recorder));
  ASSERT_EQ(recorder.seen->backfaces.size(), 1u);
  EXPECT_EQ(recorder.seen->backfaces.front(), material::Backface::Hidden);

  // The description changed and the node did not: the same body is handed
  // over with the face it now declares.
  const uint64_t handle = scene.handleOf("card");
  scene.render(recorded(describe(material::Backface::Visible), recorder));
  EXPECT_EQ(scene.handleOf("card"), handle);
  ASSERT_EQ(recorder.seen->backfaces.size(), 1u);
  EXPECT_EQ(recorder.seen->backfaces.front(), material::Backface::Visible);
}

TEST_F(WorldScene, AGeometrySlotChangeKeepsTheNodeAndItsLanes) {
  motion::Animatable<float> lift = motion::animatable(0.0f);

  const auto describe = [&lift](bool asCloud) {
    Element body = Element().key("body").translateY(lift);
    if (asCloud) {
      geometry::mesh::Cloud points;
      points.positions = {{-20, 0, 0}, {20, 0, 0}};
      body.cloud(points).stamp(card(4));
    } else {
      body.mesh(card(20));
    }
    return Element().key("root").children({std::move(body)});
  };

  scene.render(describe(false));
  const uint64_t handle = scene.handleOf("body");
  ASSERT_NE(handle, 0u);
  EXPECT_EQ(scene.stats().cooked, 1);

  lift = 12.0f;
  scene.render(describe(true));
  // The entity survives the kind change — no remount, ever …
  EXPECT_EQ(scene.handleOf("body"), handle);
  EXPECT_EQ(scene.stats().reconcile.mounted, 0);
  EXPECT_EQ(scene.stats().reconcile.retired, 0);
  // … the lane on it goes on driving the placement …
  const std::optional<glm::mat4> world = scene.transformOf("body");
  ASSERT_TRUE(world.has_value());
  EXPECT_FLOAT_EQ((*world)[3].y, 12.0f);
  // … and the artefact is a new one, cooked from the new slot.
  EXPECT_EQ(scene.stats().cooked, 1);
  EXPECT_EQ(scene.stats().resources, 1);
}

TEST_F(WorldScene, TwoNodesDescribingOneChainShareOneCook) {
  const geometry::mesh::pop::Chain chain = cometChain();
  Element root;
  root.key("root").children(
      {Element().key("left").chain(chain).stamp(card(3)).at({-40, 0, 0}),
       Element().key("right").chain(chain).stamp(card(3)).at({40, 0, 0})});

  scene.render(root);
  EXPECT_EQ(scene.stats().cooked, 1);
  EXPECT_EQ(scene.stats().resources, 1);
  EXPECT_EQ(scene.referencesOf("left"), 2);
  EXPECT_EQ(scene.referencesOf("right"), 2);

  // One of them describes a different body: the shared entry stays, and
  // the other node keeps drawing from it.
  Element split;
  split.key("root").children(
      {Element().key("left").chain(chain).stamp(card(3)).at({-40, 0, 0}),
       Element().key("right").mesh(card(9)).at({40, 0, 0})});
  scene.render(split);
  EXPECT_EQ(scene.stats().resources, 2);
  EXPECT_EQ(scene.referencesOf("left"), 1);
  EXPECT_EQ(scene.referencesOf("right"), 1);
}

TEST_F(WorldScene, ALaneRampsAPlacement) {
  const auto describe = [](float target) {
    return Element().key("root").children(
        {Element().key("body").mesh(card(10)).translateX(
            motion::animate({.to = target, .duration = 200ms}))});
  };

  const auto reach = [this] {
    const std::optional<glm::mat4> world = scene.transformOf("body");
    return world ? (*world)[3].x : std::numeric_limits<float>::quiet_NaN();
  };

  scene.render(describe(0.0f));
  EXPECT_FLOAT_EQ(reach(), 0.0f);

  scene.render(describe(100.0f));
  engine.advance(engine.elapsed() + motion::Duration(0.1));
  scene.render(describe(100.0f));
  const float midway = reach();
  EXPECT_GT(midway, 0.0f);
  EXPECT_LT(midway, 100.0f);

  engine.advance(engine.elapsed() + motion::Duration(0.2));
  scene.render(describe(100.0f));
  EXPECT_NEAR(reach(), 100.0f, 1e-3f);
}

TEST_F(WorldScene, ASettledSubtreeBakesOnceAndADrivenLaneBelowUnsettlesIt) {
  motion::Animatable<float> spin = motion::animatable(0.0f);
  const auto describe = [&spin] {
    return Element().key("root").children({Element().key("rig").children(
        {Element().key("body").mesh(card(10)).rotateY(spin)})});
  };

  // Still frames: the placement resolves identically, the settle
  // releases the volatility the binding declares, and the draw order is
  // taken once and replayed after.
  int64_t takes = 0;
  int64_t replays = 0;
  for (int frame = 0; frame < 8; ++frame) {
    scene.render(describe());
    takes += scene.stats().baked;
    replays += scene.stats().replayed;
  }
  EXPECT_EQ(takes, 1);
  EXPECT_GT(replays, 0);
  EXPECT_EQ(scene.stats().extracted, 0);  // the last frame walked nothing

  // …and the frame the lane moves, the bake above it is gone.
  spin = 45.0f;
  scene.render(describe());
  EXPECT_EQ(scene.stats().replayed, 0);
  EXPECT_GT(scene.stats().extracted, 0);
  EXPECT_FALSE(scene.stats().drawn == 0);
}

TEST_F(WorldScene, AStillChildInsideAMovingRigIsDrawnWhereItNowStands) {
  motion::Animatable<float> pan = motion::animatable(0.0f);
  const auto describe = [](motion::Animatable<float>& lane) {
    return Element().key("root").children(
        {Element().key("rig").translateX(lane).children(
            {Element().key("body").mesh(card(20))})});
  };

  const Recording held;
  // `body` declares no motion of its own, so its draw order is recorded on
  // the first frame — before any hold has warmed up, and while `rig`, whose
  // lane is live, is still painting live around it.
  scene.render(recorded(describe(pan), held));

  // The lane is then assigned from OUTSIDE. Nothing about `body`'s own
  // description changed — only the matrix above it — and the order it
  // recorded carries the placement it was recorded with, so a replay would
  // hand it over where it used to stand.
  pan = 30.0f;
  scene.render(recorded(describe(pan), held));
  EXPECT_EQ(scene.stats().replayed, 0);

  // …and it is handed over where a scene that has held nothing puts it.
  Scene fresh(engine);
  const Recording nothingHeld;
  motion::Animatable<float> panned = motion::animatable(30.0f);
  fresh.render(recorded(describe(panned), nothingHeld));
  ASSERT_FALSE(held.seen->worlds.empty());
  EXPECT_EQ(*held.seen, *nothingHeld.seen);
}

TEST_F(WorldScene, WhatAPassIsHandedIsAFunctionOfTheDescriptionAlone) {
  Scene second(engine);
  const auto describe = [] {
    return Element().key("root").children(
        {Element().key("sun").light(light::sun({-0.4f, -0.7f, -0.6f})),
         Element().key("body").mesh(card(60)).rotateY(20.0f)});
  };
  const Recording first;
  const Recording other;
  scene.render(recorded(describe(), first));
  second.render(recorded(describe(), other));

  EXPECT_EQ(*first.seen, *other.seen);
  // …and something was handed over, so the comparison is not two empty
  // readings.
  EXPECT_EQ(first.seen->keys.size(), 1u);
  EXPECT_EQ(first.seen->lightColours.size(), 1u);
}

TEST_F(WorldScene, EmittersAndViewpointsRideTheirNodesPlacement) {
  geometry::mesh::camera::Camera declared;
  declared.eye = {0, 0, 100};
  scene.render(Element().key("root").children(
      {Element()
           .key("rig")
           .at({50, 0, 0})
           .children({Element().key("eye").camera(declared)})
           .children(
               {Element().key("lamp").light(light::point({0, 20, 0}))})}));

  const std::optional<geometry::mesh::camera::Camera> camera = scene.camera();
  ASSERT_TRUE(camera.has_value());
  EXPECT_FLOAT_EQ(camera->eye.x, 50.0f);
  EXPECT_FLOAT_EQ(camera->eye.z, 100.0f);

  const std::vector<material::Light> lights = scene.lights();
  ASSERT_EQ(lights.size(), 1u);
  EXPECT_FLOAT_EQ(lights.front().position.x, 50.0f);
  EXPECT_FLOAT_EQ(lights.front().position.y, 20.0f);
}

TEST_F(WorldScene, TheFirstViewpointInTreeOrderIsTheOneSeenFrom) {
  // A host that takes hold of a set's viewpoint hangs the set under a node
  // carrying its own camera; that camera stands before every camera the
  // set declared beneath it, so it is the one the frame is seen from.
  geometry::mesh::camera::Camera host;
  host.eye = {0, 0, 700};
  geometry::mesh::camera::Camera declared;
  declared.eye = {0, 0, 100};
  scene.render(Element().camera(host).children({Element().key("set").children(
      {Element().key("lens").camera(declared)})}));
  const std::optional<geometry::mesh::camera::Camera> camera = scene.camera();
  ASSERT_TRUE(camera.has_value());
  EXPECT_FLOAT_EQ(camera->eye.z, 700.0f);
}

TEST_F(WorldScene, AnEmitterDialReachesTheLightItScales) {
  motion::Animatable<float> strength = motion::animatable(0.25f);
  motion::Animatable<float> red = motion::animatable(1.0f);

  const auto describe = [&] {
    return Element().key("root").children(
        {Element()
             .key("lamp")
             .light(light::point({0, 0, 0}, {0.1f, 0.2f, 0.3f, 1.0f}, 0.6f))
             .intensity(strength)
             .emission(red, 0.5f, 0.5f)});
  };

  scene.render(describe());
  std::vector<material::Light> lights = scene.lights();
  ASSERT_EQ(lights.size(), 1u);
  EXPECT_FLOAT_EQ(lights.front().intensity.value(), 0.25f);
  EXPECT_FLOAT_EQ(lights.front().color.value().r, 1.0f);
  EXPECT_FLOAT_EQ(lights.front().color.value().g, 0.5f);

  // A LANE, not a description: the value moves and the tree is unchanged.
  strength = 0.9f;
  red = 0.2f;
  scene.render(describe());
  lights = scene.lights();
  ASSERT_EQ(lights.size(), 1u);
  EXPECT_FLOAT_EQ(lights.front().intensity.value(), 0.9f);
  EXPECT_FLOAT_EQ(lights.front().color.value().r, 0.2f);
}

TEST_F(WorldScene, AnEmitterWithNoDialsShinesAsItWasDeclared) {
  scene.render(Element().key("root").children({Element().key("lamp").light(
      light::point({0, 0, 0}, {0.3f, 0.6f, 0.9f, 1.0f}, 0.4f))}));
  const std::vector<material::Light> lights = scene.lights();
  ASSERT_EQ(lights.size(), 1u);
  EXPECT_FLOAT_EQ(lights.front().intensity.value(), 0.4f);
  EXPECT_FLOAT_EQ(lights.front().color.value().b, 0.9f);
}

TEST_F(WorldScene, ALiveLightColorReachesRetainedBodiesWithoutRecooking) {
  motion::Animatable<material::Color> color =
      motion::animatable(material::Color{2, 0.125f, 0.0625f, 0.5f});
  material::Light source = light::sun({0, 0, -1});
  source.color = color;
  const auto describe = [](const material::Light& lamp) {
    return Element().key("root").children(
        {Element().key("lamp").light(lamp),
         Element().key("body").mesh(card(30)).fill(
             paint({0.8f, 0.8f, 0.8f, 1}))});
  };
  const Recording recorder;
  const Frame tree = recorded(describe(source), recorder);
  for (int frame = 0; frame < 8; ++frame) scene.render(tree);
  ASSERT_GT(scene.stats().replayed, 0);
  const uint64_t handle = scene.handleOf("body");
  const Seen before = *recorder.seen;
  ASSERT_EQ(before.keys.size(), 1u);

  const material::Color changed{0.0625f, 0.125f, 2, 0.75f};
  color = changed;
  scene.render(tree);
  const std::vector<material::Light> lights = scene.lights();
  ASSERT_EQ(lights.size(), 1u);
  EXPECT_EQ(lights.front().color.value(), changed);
  EXPECT_FALSE(lights.front().color.isRunning());
  EXPECT_EQ(scene.handleOf("body"), handle);
  EXPECT_EQ(scene.stats().cooked, 0);
  EXPECT_EQ(scene.stats().resources, 1);
  EXPECT_EQ(scene.referencesOf("body"), 1);
  EXPECT_GT(scene.stats().replayed, 0);
  const Seen after = *recorder.seen;
  EXPECT_NE(before, after);
  ASSERT_EQ(after.lightColours.size(), 1u);
  EXPECT_EQ(after.lightColours.front(), changed);

  motion::Engine referenceEngine;
  Scene reference(referenceEngine);
  material::Light fixed = light::sun({0, 0, -1});
  fixed.color = motion::Animatable<material::Color>(changed);
  const Recording referenceRecorder;
  reference.render(recorded(describe(fixed), referenceRecorder));
  EXPECT_EQ(after, *referenceRecorder.seen);
}

TEST_F(WorldScene, EmitterDialsDoNotWriteIntoTheirLiveSources) {
  const material::Color initial{2, 3, 4, 0.5f};
  motion::Animatable<material::Color> color = motion::animatable(initial);
  motion::Animatable<float> strength = motion::animatable(0.75f);
  material::Light source =
      light::spot({2, 3, 4}, {0, 0, -1}, 47, 13, {}, 1, 987);
  source.ambient = 0.125f;
  source.color = color;
  source.intensity = strength;
  const Element tree =
      Element().key("root").children({Element()
                                          .key("lamp")
                                          .light(source)
                                          .emission(0.2f, 0.4f, 0.6f)
                                          .intensity(0.25f)});
  scene.render(tree);
  EXPECT_EQ(color.value(), initial);
  EXPECT_FLOAT_EQ(strength.value(), 0.75f);
  std::vector<material::Light> lights = scene.lights();
  ASSERT_EQ(lights.size(), 1u);
  EXPECT_EQ(lights.front().color.value(),
            (material::Color{0.2f, 0.4f, 0.6f, 0.5f}));
  EXPECT_FLOAT_EQ(lights.front().intensity.value(), 0.25f);
  EXPECT_FALSE(lights.front().color.isRunning());
  EXPECT_FALSE(lights.front().intensity.isRunning());
  EXPECT_EQ(lights.front().kind, material::LightKind::Spot);
  EXPECT_EQ(lights.front().position, source.position);
  EXPECT_FLOAT_EQ(lights.front().range, 987);
  EXPECT_FLOAT_EQ(lights.front().innerAngle, 13);
  EXPECT_FLOAT_EQ(lights.front().outerAngle, 47);
  EXPECT_FLOAT_EQ(lights.front().ambient, 0.125f);

  const material::Color changed{5, 6, 7, 0.8f};
  color = changed;
  strength = 1.5f;
  scene.render(tree);
  EXPECT_EQ(color.value(), changed);
  EXPECT_FLOAT_EQ(strength.value(), 1.5f);
  lights = scene.lights();
  ASSERT_EQ(lights.size(), 1u);
  EXPECT_EQ(lights.front().color.value(),
            (material::Color{0.2f, 0.4f, 0.6f, 0.8f}));
  EXPECT_FLOAT_EQ(lights.front().intensity.value(), 0.25f);
  EXPECT_EQ(lights.front().kind, source.kind);
  EXPECT_EQ(lights.front().position, source.position);
  EXPECT_FLOAT_EQ(lights.front().range, source.range);
  EXPECT_FLOAT_EQ(lights.front().innerAngle, source.innerAngle);
  EXPECT_FLOAT_EQ(lights.front().outerAngle, source.outerAngle);
  EXPECT_FLOAT_EQ(lights.front().ambient, source.ambient);
}

TEST_F(WorldScene, PlacingALightDoesNotWriteIntoItsLiveAngles) {
  motion::Animatable<float> direction = motion::animatable(30.0f);
  motion::Animatable<float> elevation = motion::animatable(45.0f);
  material::Light source;
  source.direction = direction;
  source.elevation = elevation;
  const auto describe = [](const material::Light& lamp) {
    return Element().key("root").children(
        {Element().key("rig").rotateY(60).children(
            {Element().key("lamp").light(lamp)})});
  };
  const Element tree = describe(source);
  motion::Engine referenceEngine;
  Scene reference(referenceEngine);
  for (int frame = 0; frame < 8; ++frame) {
    const float bearing = frame < 4 ? 30.0f : 90.0f;
    const float height = frame < 4 ? 45.0f : 25.0f;
    if (frame == 4) {
      direction = bearing;
      elevation = height;
    }
    material::Light fixed;
    fixed.direction = bearing;
    fixed.elevation = height;
    reference.render(describe(fixed));
    scene.render(tree);
    EXPECT_FLOAT_EQ(direction.value(), bearing);
    EXPECT_FLOAT_EQ(elevation.value(), height);
    const std::vector<material::Light> actual = scene.lights();
    const std::vector<material::Light> expected = reference.lights();
    ASSERT_EQ(actual.size(), 1u);
    ASSERT_EQ(expected.size(), 1u);
    EXPECT_FALSE(actual.front().direction.isRunning());
    EXPECT_FALSE(actual.front().elevation.isRunning());
    const glm::vec3 ray = light::travel(actual.front());
    const glm::vec3 target = light::travel(expected.front());
    EXPECT_NEAR(ray.x, target.x, 1e-6f);
    EXPECT_NEAR(ray.y, target.y, 1e-6f);
    EXPECT_NEAR(ray.z, target.z, 1e-6f);
  }
}

TEST_F(WorldScene, RetiringANodeHandsBackItsEntityAndItsArtefact) {
  scene.render(
      Element().key("root").children({Element().key("body").mesh(card(10))}));
  EXPECT_EQ(scene.stats().resources, 1);
  EXPECT_NE(scene.handleOf("body"), 0u);

  scene.render(Element().key("root"));
  EXPECT_EQ(scene.handleOf("body"), 0u);
  EXPECT_EQ(scene.stats().resources, 0);
  EXPECT_EQ(scene.stats().reconcile.retired, 1);
}

// ---- frames: passes, ordering, readbacks -----------------------------------

namespace {

constexpr glm::ivec2 kFrameExtent{96, 96};

/** A set with one plain body on the left and one tagged "glow" on the
 *  right. */
Element pair() {
  return Element().key("root").children(
      {Element().key("left").at({-46, 0, 0}).mesh(card(34)).tag("plain"),
       Element().key("right").at({46, 0, 0}).mesh(card(34)).tag("glow")});
}

Frame framed(Element scene) {
  Frame frame(std::move(scene));
  frame.extent(kFrameExtent).camera(frontCamera(320.0f));
  return frame;
}

/** WHAT A FRAME PRESENTED, as pixels. */
SkBitmap present(Scene& scene) {
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(kFrameExtent.x, kFrameExtent.y));
  bitmap.eraseColor(SK_ColorTRANSPARENT);
  SkCanvas canvas(bitmap);
  draw(scene, canvas);
  return bitmap;
}

}  // namespace

TEST_F(WorldScene, AFrameWithNoPassesIsExtractedAndFormsNoPicture) {
  scene.render(framed(
      pair().children({Element().key("sun").light(light::sun({0, 0, -1}))})));
  EXPECT_TRUE(scene.error().empty());
  EXPECT_EQ(scene.stats().passes, 0);
  // The scene is there — its emitters and its bodies are extracted — and
  // nothing drew it, because a picture is what a pass writes.
  EXPECT_EQ(scene.lights().size(), 1u);
  EXPECT_NE(scene.handleOf("left"), 0u);
  const SkBitmap picture = present(scene);
  EXPECT_EQ(test::paintedIn(picture, true) + test::paintedIn(picture, false),
            0);
}

TEST_F(WorldScene, AFrameThatDeclaresPassesAndNoRuntimeSaysSo) {
  Frame frame = framed(pair());
  frame.pass(geometryPass("main").writes("colour"));
  scene.render(frame);
  EXPECT_FALSE(scene.error().empty());
  EXPECT_EQ(scene.stats().passes, 0);
}

TEST_F(WorldScene, APassSeesWhatExtractWroteAndNotTheTree) {
  std::vector<std::string> keys;
  std::vector<std::string> ancestors;
  std::vector<std::string> tags;
  Frame frame = framed(pair());
  frame.runtime(Runtime{Recording{}})
      .pass(geometryPass("hand").writes("colour").body([&](const View& view) {
        for (const Draw& draw : view.draws) {
          keys.emplace_back(draw.key);
          for (const std::string& word : draw.tags) tags.push_back(word);
          for (const std::string& up : draw.ancestors) ancestors.push_back(up);
        }
      }));
  scene.render(frame);

  ASSERT_EQ(keys.size(), 2u);
  EXPECT_EQ(scene.stats().passes, 1);
  EXPECT_EQ(tags.size(), 2u);
  // Every body stands under the root, and under nothing else.
  ASSERT_EQ(ancestors.size(), 2u);
  EXPECT_EQ(ancestors.front(), "root");
}

TEST_F(WorldScene, ASelectingGeometryPassIsHandedItsRealisation) {
  const Recording recorder;
  Frame frame = framed(pair());
  frame.runtime(Runtime{recorder})
      .pass(geometryPass("glow").only(selectors::tag("glow")).writes("colour"));
  scene.render(frame);

  ASSERT_TRUE(scene.error().empty());
  ASSERT_EQ(recorder.seen->realisations.size(), 1u);
  EXPECT_EQ(recorder.seen->realisations.front(), Selection::Cull);
}

TEST_F(WorldScene, AReadbackIsHandedOverTheFrameAfter) {
  int calls = 0;
  uint64_t at = 0;
  bool hadImage = false;
  const auto describe = [&] {
    Frame frame = framed(pair());
    frame.runtime(Runtime{Recording{}})
        .pass(geometryPass("main").writes("colour"))
        .readback(readback("colour").then([&](const Readback::Result& result) {
          ++calls;
          at = result.frame;
          hadImage = (bool)result.image;
        }));
    return frame;
  };

  scene.render(describe());
  EXPECT_EQ(calls, 0);  // taken, not yet handed over
  scene.render(describe());
  EXPECT_EQ(calls, 1);
  EXPECT_EQ(at, 0u);  // what the FIRST frame wrote
  EXPECT_TRUE(hadImage);
  scene.render(describe());
  EXPECT_EQ(calls, 2);
  EXPECT_EQ(at, 1u);
}

TEST_F(WorldScene, ACycleInThePassesIsAnErrorAndNothingRuns) {
  Frame frame = framed(pair());
  frame.runtime(Runtime{Recording{}})
      .pass(postPass("a").reads("second").writes("first"))
      .pass(postPass("b").reads("first").writes("second"));
  scene.render(frame);

  EXPECT_FALSE(scene.error().empty());
  EXPECT_EQ(scene.stats().passes, 0);
}

TEST_F(WorldScene, AFrameThatDeclaresPassesAndNoExtentSaysSo) {
  Frame frame(pair());
  frame.runtime(Runtime{Recording{}})
      .pass(geometryPass("main").writes("colour"));
  scene.render(frame);
  EXPECT_FALSE(scene.error().empty());
  EXPECT_EQ(scene.stats().passes, 0);
}

TEST_F(WorldScene, TheOrderingsCountsAreOnTheFramesTally) {
  Frame frame = framed(pair());
  frame.runtime(Runtime{Recording{}})
      .pass(geometryPass("main").writes("colour"))
      .pass(postPass("half").reads("colour").writes("half"))
      .pass(postPass("quarter").reads("half").writes("quarter"))
      .pass(postPass("eighth").reads("quarter").writes("eighth"));
  scene.render(frame);

  ASSERT_TRUE(scene.error().empty());
  EXPECT_EQ(scene.stats().passes, 4);
  EXPECT_EQ(scene.stats().surfaces, scene.plan().surfaces());
  ASSERT_GT(scene.plan().aliased(), 0)
      << "this frame reuses no surface, so the tally says nothing";
  EXPECT_EQ(scene.stats().aliased, scene.plan().aliased());
  EXPECT_GT(scene.stats().barriers, 0);
}
