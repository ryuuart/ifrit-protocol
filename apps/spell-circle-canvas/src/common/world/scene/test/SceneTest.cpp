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
#include <sigilgeometry/kit/Solids.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Tween.h>
#include <sigilworld/World.h>
#include <sigilworld/advanced/Skia.h>
#include <sigilworld/scene/Scene.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
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

/** A closed loop and stamps riding a window of it, as instanced geometry
 *  — the same VALUE wherever it is built, which is what makes it
 *  shareable. */
geometry::mesh::Instances comet() {
  std::vector<glm::vec3> loop = {
      {-80, 0, 0}, {0, 60, 0}, {80, 0, 0}, {0, -60, 0}};
  return geometry::mesh::pop::on(loop)
      .count(64)
      .window(0.9f, 0.4f)
      .spread(6.0f)
      .instances(card(3));
}

/** WHAT A PASS WAS HANDED, as values: every body's key, placement,
 *  colour and backface in the order the view lists them, and every
 *  light's colour and strength. Two of these compare equal exactly when
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
    for (const PlacedLight& light : view.lights) {
      seen->lightColours.push_back(light.light.color.value());
      seen->lightStrengths.push_back(light.light.intensity.value());
    }
  }
  bool operator==(const Recording& other) const { return seen == other.seen; }
};

/** @p tree as a frame with one geometry pass, performed on @p recorder. */
Frame recorded(
    Node tree, const Recording& recorder,
    const geometry::mesh::camera::Camera& camera = frontCamera(320.0f)) {
  Frame frame(std::move(tree));
  frame.extent({96, 96})
      .defaultCamera(camera)
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
    return group(each(order,
                      [](const std::string& key) {
                        return mesh(card(10)).key(key);
                      }))
        .key("root");
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
    return group({mesh(card(24)).key("card").backface(backface)}).key("root");
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

  // Triangles and instanced geometry are two values of one geometry slot,
  // so the same key describing the one and then the other is a change of
  // what the node holds, not a new node.
  const auto describe = [&lift](bool asCloud) -> Group {
    if (asCloud) {
      geometry::mesh::Cloud cloud;
      cloud.positions = {{-20, 0, 0}, {20, 0, 0}};
      return group({mesh(geometry::mesh::points::instances(cloud, card(4)))
                        .key("body")
                        .translateY(lift)})
          .key("root");
    }
    return group({mesh(card(20)).key("body").translateY(lift)}).key("root");
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

TEST_F(WorldScene, ASolidSpelledAsAValueIsFormedOnceWhileItStands) {
  // Two nodes name one solid by its numbers, frame after frame: the
  // geometry slot holds the value, so the store forms it once, shares it
  // between the two, and a second frame naming it again forms nothing.
  const auto describe = [] {
    const geometry::solids::Superellipsoid slab{.radii = {22, 30, 22},
                                                .exponent = 3,
                                                .longitudeSteps = 18,
                                                .latitudeSteps = 12};
    return group({mesh(slab).key("left").at({-40, 0, 0}),
                  mesh(slab).key("right").at({40, 0, 0})})
        .key("root");
  };
  scene.render(describe());
  EXPECT_EQ(scene.stats().cooked, 1);
  EXPECT_EQ(scene.stats().resources, 1);
  scene.render(describe());
  EXPECT_EQ(scene.stats().cooked, 0)
      << "an unchanged solid is not formed again";
  EXPECT_EQ(scene.stats().resources, 1);
}

TEST_F(WorldScene, TwoNodesDescribingOneInstancingShareOneArtefact) {
  const geometry::mesh::Instances instances = comet();
  const Group root = group({mesh(instances).key("left").at({-40, 0, 0}),
                            mesh(instances).key("right").at({40, 0, 0})})
                         .key("root");

  scene.render(root);
  EXPECT_EQ(scene.stats().cooked, 1);
  EXPECT_EQ(scene.stats().resources, 1);
  EXPECT_EQ(scene.referencesOf("left"), 2);
  EXPECT_EQ(scene.referencesOf("right"), 2);

  // One of them describes a different body: the shared entry stays, and
  // the other node keeps drawing from it.
  const Group split = group({mesh(instances).key("left").at({-40, 0, 0}),
                             mesh(card(9)).key("right").at({40, 0, 0})})
                          .key("root");
  scene.render(split);
  EXPECT_EQ(scene.stats().resources, 2);
  EXPECT_EQ(scene.referencesOf("left"), 1);
  EXPECT_EQ(scene.referencesOf("right"), 1);
}

TEST_F(WorldScene, ALaneRampsAPlacement) {
  const auto describe = [](float target) {
    return group({mesh(card(10)).key("body").translateX(
                     motion::animate({.to = target, .duration = 200ms}))})
        .key("root");
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
    return group({group({mesh(card(10)).key("body").rotateY(spin)}).key("rig")})
        .key("root");
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
    return group({group({mesh(card(20)).key("body")})
                      .key("rig")
                      .translateX(lane)})
        .key("root");
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
    return group({directionalLight({-0.4f, -0.7f, -0.6f}).key("sun"),
                  mesh(card(60)).key("body").rotateY(20.0f)})
        .key("root");
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

TEST_F(WorldScene, LightsAndCamerasRideTheirNodesPlacement) {
  geometry::mesh::camera::Camera declared;
  declared.eye = {0, 0, 100};
  scene.render(group({group({camera(declared).key("eye"),
                             pointLight().key("lamp").at({0, 20, 0})})
                          .key("rig")
                          .at({50, 0, 0})})
                   .key("root"));

  const std::optional<geometry::mesh::camera::Camera> lens = scene.camera();
  ASSERT_TRUE(lens.has_value());
  EXPECT_FLOAT_EQ(lens->eye.x, 50.0f);
  EXPECT_FLOAT_EQ(lens->eye.z, 100.0f);

  const std::vector<PlacedLight> lights = scene.lights();
  ASSERT_EQ(lights.size(), 1u);
  EXPECT_FLOAT_EQ(lights.front().position.x, 50.0f);
  EXPECT_FLOAT_EQ(lights.front().position.y, 20.0f);
  EXPECT_EQ(lights.front().key, "lamp");
}

TEST_F(WorldScene, ALightsDirectionTurnsWithItsNode) {
  scene.render(group({group({directionalLight({0, 0, -1}).key("sun")})
                          .key("rig")
                          .rotateY(90.0f)})
                   .key("root"));
  const std::vector<PlacedLight> lights = scene.lights();
  ASSERT_EQ(lights.size(), 1u);
  // A quarter turn about y carries -z onto -x.
  EXPECT_NEAR(lights.front().light.direction.x, -1.0f, 1e-6f);
  EXPECT_NEAR(lights.front().light.direction.z, 0.0f, 1e-6f);
}

namespace {

geometry::mesh::camera::Camera lensAt(float z) {
  geometry::mesh::camera::Camera lens;
  lens.eye = {0, 0, z};
  return lens;
}

}  // namespace

TEST_F(WorldScene, OneCameraInTheTreeIsTheOneLookedThrough) {
  scene.render(group({camera(lensAt(300)).key("only")}).key("root"));
  ASSERT_TRUE(scene.camera().has_value());
  EXPECT_FLOAT_EQ(scene.camera()->eye.z, 300.0f);
  EXPECT_TRUE(scene.error().empty());
}

TEST_F(WorldScene, SeveralCamerasAreChosenBetweenByKey) {
  const Group root =
      group({camera(lensAt(300)).key("wide"), camera(lensAt(900)).key("far")})
          .key("root");
  Frame named(root);
  named.camera("far");
  scene.render(named);
  ASSERT_TRUE(scene.camera().has_value());
  EXPECT_FLOAT_EQ(scene.camera()->eye.z, 900.0f);
  EXPECT_TRUE(scene.error().empty());
}

TEST_F(WorldScene, ACameraLookingAtANullLooksWhereTheNullStands) {
  scene.render(group({group({null().key("pivot")}).at({40, 10, -5}),
                      camera(lensAt(300)).key("eye").lookAt("pivot")})
                   .key("root"));
  ASSERT_TRUE(scene.camera().has_value());
  EXPECT_TRUE(scene.error().empty());
  EXPECT_FLOAT_EQ(scene.camera()->target.x, 40.0f);
  EXPECT_FLOAT_EQ(scene.camera()->target.y, 10.0f);
  EXPECT_FLOAT_EQ(scene.camera()->target.z, -5.0f);
  EXPECT_FLOAT_EQ(scene.camera()->eye.z, 300.0f);
}

TEST_F(WorldScene, AVisibleLightRidingARailFacesWhatItLooksAtAtEveryStation) {
  // A panel riding a straight rail above a subject, aimed at it: at two
  // stations the light shines at the subject and the panel's face, its
  // +z, points there too, wherever the rail has carried it.
  const geometry::mesh::curve::Spline3 rail{
      .points = {{-200, 100, 0}, {200, 100, 0}},
      .type = geometry::mesh::curve::Spline3::Type::Linear};
  const glm::vec3 subject{0, -50, 30};
  const Recording recorder;
  for (const float distance : {50.0f, 300.0f}) {
    scene.render(
        recorded(group({null().key("subject").at(subject),
                        kit::visibleLight(
                            rectLight({0, -1, 0}, {.width = 40, .height = 20})
                                .key("panel"))
                            .key("panel rig")
                            .along(rail, distance)
                            .lookAt("subject")})
                     .key("root"),
                 recorder));
    ASSERT_TRUE(scene.error().empty()) << scene.error();
    const glm::vec3 station{-200.0f + distance, 100.0f, 0.0f};
    const glm::vec3 toward = glm::normalize(subject - station);

    const auto emitter = std::find(recorder.seen->keys.begin(),
                                   recorder.seen->keys.end(), "emitter");
    ASSERT_NE(emitter, recorder.seen->keys.end());
    const glm::mat4& placed =
        recorder.seen->worlds[size_t(emitter - recorder.seen->keys.begin())];
    EXPECT_NEAR(glm::distance(glm::vec3(placed[3]), station), 0.0f, 1e-3f);
    const glm::vec3 normal =
        glm::normalize(glm::mat3(placed) * glm::vec3(0, 0, 1));
    EXPECT_NEAR(glm::dot(normal, toward), 1.0f, 1e-5f)
        << "the panel faces its subject at " << distance;

    const std::vector<PlacedLight> lights = scene.lights();
    ASSERT_EQ(lights.size(), 1u);
    EXPECT_NEAR(glm::dot(lights.front().light.direction, toward), 1.0f, 1e-5f)
        << "the light shines at its subject at " << distance;
  }
}

TEST_F(WorldScene, AnAimedGroupTurnsItsForwardAxisOntoWhatItLooksAt) {
  // A plain group points +z at what it looks at, and carries its children
  // round with it; a body standing ahead of it on +z is carried onto the
  // line toward the target.
  const Recording recorder;
  scene.render(
      recorded(group({null().key("target").at({100, 0, 0}),
                      group({mesh(card(2)).key("ahead").at({0, 0, 10})})
                          .key("aimed")
                          .rotateY(30.0f)
                          .lookAt("target")})
                   .key("root"),
               recorder));
  ASSERT_TRUE(scene.error().empty()) << scene.error();
  ASSERT_EQ(recorder.seen->worlds.size(), 1u);
  const glm::vec3 ahead(recorder.seen->worlds.front()[3]);
  EXPECT_NEAR(ahead.x, 10.0f, 1e-4f);
  EXPECT_NEAR(ahead.y, 0.0f, 1e-4f);
  EXPECT_NEAR(ahead.z, 0.0f, 1e-4f);

  scene.render(group({group().key("aimed").lookAt("nowhere")}).key("root"));
  EXPECT_NE(scene.error().find("\"aimed\""), std::string::npos);
  EXPECT_NE(scene.error().find("\"nowhere\""), std::string::npos);
}

TEST_F(WorldScene, LookingAtAKeyNoNodeAnswersToIsAnErrorNamingIt) {
  scene.render(group({directionalLight({0, -1, 0}).key("sun").lookAt("pivot"),
                      null().key("elsewhere")})
                   .key("root"));
  EXPECT_NE(scene.error().find("\"sun\""), std::string::npos);
  EXPECT_NE(scene.error().find("\"pivot\""), std::string::npos);
}

TEST_F(WorldScene, SeveralCamerasAndNoneNamedIsAnErrorThatSaysSo) {
  scene.render(
      group({camera(lensAt(300)).key("wide"), camera(lensAt(900)).key("far")})
          .key("root"));
  EXPECT_FALSE(scene.camera().has_value());
  EXPECT_NE(scene.error().find("\"wide\""), std::string::npos);
  EXPECT_NE(scene.error().find("\"far\""), std::string::npos);

  Frame missing(group({camera(lensAt(300)).key("wide")}).key("root"));
  missing.camera("close");
  scene.render(missing);
  EXPECT_NE(scene.error().find("\"close\""), std::string::npos);
}

TEST_F(WorldScene, AnOverrideMovesTheLensAndKeepsTheCamerasExposure) {
  // A host that has taken hold of the viewpoint says where the picture is
  // taken from; the camera the tree chose still says how it is read.
  float exposure = 0;
  geometry::mesh::camera::Camera seen;
  Frame frame(
      group({camera(lensAt(300), {.exposure = 1.5f}).key("eye")}).key("root"));
  frame.extent({8, 8})
      .cameraOverride(lensAt(700))
      .runtime(Runtime{Recording{}})
      .pass(geometryPass("hand").writes("colour").body([&](const View& view) {
        exposure = view.exposure;
        seen = view.camera;
      }));
  scene.render(frame);
  ASSERT_TRUE(scene.error().empty()) << scene.error();
  EXPECT_FLOAT_EQ(exposure, 1.5f);
  EXPECT_FLOAT_EQ(seen.eye.z, 700.0f);
}

TEST_F(WorldScene, AHiddenNodeTakesItsSubtreeOutOfTheScene) {
  const auto describe = [](bool shown) {
    return group({group({mesh(card(10)).key("body"), pointLight().key("lamp"),
                         camera(lensAt(200)).key("eye")})
                      .key("rig")
                      .visible(shown)})
        .key("root");
  };
  const Recording recorder;
  scene.render(recorded(describe(true), recorder));
  EXPECT_EQ(recorder.seen->keys.size(), 1u);
  EXPECT_EQ(scene.lights().size(), 1u);
  EXPECT_TRUE(scene.camera().has_value());

  scene.render(recorded(describe(false), recorder));
  EXPECT_TRUE(recorder.seen->keys.empty());
  EXPECT_TRUE(scene.lights().empty());
  EXPECT_FALSE(scene.camera().has_value());
}

TEST_F(WorldScene, ALightWithNoBindingShinesAsItWasDeclared) {
  scene.render(
      group({pointLight({.color = material::Color{0.3f, 0.6f, 0.9f, 1.0f},
                         .intensity = 0.4f})
                 .key("lamp")})
          .key("root"));
  const std::vector<PlacedLight> lights = scene.lights();
  ASSERT_EQ(lights.size(), 1u);
  EXPECT_FLOAT_EQ(lights.front().light.intensity.value(), 0.4f);
  EXPECT_FLOAT_EQ(lights.front().light.color.value().b, 0.9f);
}

TEST_F(WorldScene, ALiveLightReachesRetainedBodiesWithoutRecooking) {
  motion::Animatable<material::Color> color =
      motion::animatable(material::Color{2, 0.125f, 0.0625f, 0.5f});
  motion::Animatable<float> strength = motion::animatable(0.25f);
  const auto describe = [](motion::Animatable<material::Color> tint,
                           motion::Animatable<float> power) {
    return group(
               {directionalLight({0, 0, -1}, {.color = std::move(tint),
                                              .intensity = std::move(power)})
                    .key("lamp"),
                mesh(card(30)).key("body").fill(paint({0.8f, 0.8f, 0.8f, 1}))})
        .key("root");
  };
  const Recording recorder;
  const Frame tree = recorded(describe(color, strength), recorder);
  for (int frame = 0; frame < 8; ++frame) scene.render(tree);
  ASSERT_GT(scene.stats().replayed, 0);
  const uint64_t handle = scene.handleOf("body");
  const Seen before = *recorder.seen;
  ASSERT_EQ(before.keys.size(), 1u);

  const material::Color changed{0.0625f, 0.125f, 2, 0.75f};
  color = changed;
  strength = 0.9f;
  scene.render(tree);
  const std::vector<PlacedLight> lights = scene.lights();
  ASSERT_EQ(lights.size(), 1u);
  EXPECT_EQ(lights.front().light.color.value(), changed);
  EXPECT_FLOAT_EQ(lights.front().light.intensity.value(), 0.9f);
  // What a frame keeps of a light is held still.
  EXPECT_FALSE(lights.front().light.color.isRunning());
  EXPECT_FALSE(lights.front().light.intensity.isRunning());
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
  const Recording referenceRecorder;
  reference.render(recorded(describe(changed, 0.9f), referenceRecorder));
  EXPECT_EQ(after, *referenceRecorder.seen);
}

TEST_F(WorldScene, ReadingALightDoesNotWriteIntoItsLiveSources) {
  const material::Color initial{2, 3, 4, 0.5f};
  motion::Animatable<material::Color> color = motion::animatable(initial);
  motion::Animatable<float> strength = motion::animatable(0.75f);
  const Group tree = group({spotLight({0, 0, -1}, {.color = color,
                                                   .intensity = strength,
                                                   .range = 987,
                                                   .innerAngle = 13,
                                                   .outerAngle = 47})
                                .key("lamp")
                                .at({2, 3, 4})})
                         .key("root");
  scene.render(tree);
  EXPECT_EQ(color.value(), initial);
  EXPECT_FLOAT_EQ(strength.value(), 0.75f);
  std::vector<PlacedLight> lights = scene.lights();
  ASSERT_EQ(lights.size(), 1u);
  EXPECT_EQ(lights.front().light.color.value(), initial);
  EXPECT_EQ(lights.front().light.kind, LightKind::Spot);
  EXPECT_EQ(lights.front().position, glm::vec3(2, 3, 4));
  EXPECT_FLOAT_EQ(lights.front().light.range, 987);
  EXPECT_FLOAT_EQ(lights.front().light.innerAngle, 13);
  EXPECT_FLOAT_EQ(lights.front().light.outerAngle, 47);

  const material::Color changed{5, 6, 7, 0.8f};
  color = changed;
  strength = 1.5f;
  scene.render(tree);
  EXPECT_EQ(color.value(), changed);
  EXPECT_FLOAT_EQ(strength.value(), 1.5f);
  lights = scene.lights();
  ASSERT_EQ(lights.size(), 1u);
  EXPECT_EQ(lights.front().light.color.value(), changed);
  EXPECT_FLOAT_EQ(lights.front().light.intensity.value(), 1.5f);
}

TEST_F(WorldScene, RetiringANodeHandsBackItsEntityAndItsArtefact) {
  scene.render(group({mesh(card(10)).key("body")}).key("root"));
  EXPECT_EQ(scene.stats().resources, 1);
  EXPECT_NE(scene.handleOf("body"), 0u);

  scene.render(group().key("root"));
  EXPECT_EQ(scene.handleOf("body"), 0u);
  EXPECT_EQ(scene.stats().resources, 0);
  EXPECT_EQ(scene.stats().reconcile.retired, 1);
}

// ---- frames: passes, ordering, readbacks -----------------------------------

namespace {

constexpr glm::ivec2 kFrameExtent{96, 96};

/** The two bodies of a scene: one plain on the left and one in the "glow"
 *  collection on the right. */
std::vector<Node> bodies() {
  return {mesh(card(34)).key("left").at({-46, 0, 0}).collection("plain"),
          mesh(card(34)).key("right").at({46, 0, 0}).collection("glow")};
}

/** …as a set of their own. */
Group pair() { return group(bodies()).key("root"); }

Frame framed(Node scene) {
  Frame frame(std::move(scene));
  frame.extent(kFrameExtent).defaultCamera(frontCamera(320.0f));
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
      group({bodies(), directionalLight({0, 0, -1}).key("sun")}).key("root")));
  EXPECT_TRUE(scene.error().empty());
  EXPECT_EQ(scene.stats().passes, 0);
  // The scene is there — its lights and its bodies are extracted — and
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
  std::vector<std::string> collections;
  Frame frame = framed(pair());
  frame.runtime(Runtime{Recording{}})
      .pass(geometryPass("hand").writes("colour").body([&](const View& view) {
        for (const Draw& draw : view.draws) {
          keys.emplace_back(draw.key);
          for (const std::string& name : draw.collections)
            collections.push_back(name);
          for (const std::string& up : draw.ancestors) ancestors.push_back(up);
        }
      }));
  scene.render(frame);

  ASSERT_EQ(keys.size(), 2u);
  EXPECT_EQ(scene.stats().passes, 1);
  EXPECT_EQ(collections.size(), 2u);
  // Every body stands under the root, and under nothing else.
  ASSERT_EQ(ancestors.size(), 2u);
  EXPECT_EQ(ancestors.front(), "root");
}

TEST_F(WorldScene, ASelectingGeometryPassIsHandedItsRealisation) {
  const Recording recorder;
  Frame frame = framed(pair());
  frame.runtime(Runtime{recorder})
      .pass(geometryPass("glow").only("glow").writes("colour"));
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

// ---- texture passes ---------------------------------------------------------

namespace {

/** A producer standing for a page: it paints when told something changed,
 *  and says which textures are its own by their source. */
struct Page : TextureProducerOperations {
  explicit Page(media::PixelSource source) : pixels(std::move(source)) {}
  std::shared_ptr<int> painted = std::make_shared<int>(0);
  std::shared_ptr<bool> changed = std::make_shared<bool>(true);
  media::PixelSource pixels;

  bool produce(std::chrono::duration<double>) const override {
    if (!*changed) return false;
    ++*painted;
    *changed = false;
    return true;
  }
  bool paints(const material::Texture& texture) const override {
    const auto* marked = texture.source().as<ProducedPixels>();
    return marked && marked->pixels == pixels;
  }
  bool operator==(const Page& other) const { return painted == other.painted; }
};

/** A picture standing for what a page paints. */
media::PixelSource pagePixels() {
  SkBitmap pixel;
  pixel.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  pixel.eraseColor(SK_ColorWHITE);
  pixel.setImmutable();
  return media::PixelSource(pixel.asImage());
}

}  // namespace

TEST_F(WorldScene, ABodyWearingATextureWhosePassIsMissingIsANamedError) {
  const Page page(pagePixels());
  material::Material wearing = material::surface::program();
  wearing.slot(material::surface::kBaseColorSlot,
               produced(material::Texture(page.pixels), "page"));
  const Group set =
      group({mesh(card(20)).key("plate").fill(wearing)}).key("root");

  Frame without = framed(set);
  without.runtime(Runtime{Recording{}})
      .pass(geometryPass("main").writes("colour"));
  scene.render(without);
  EXPECT_NE(scene.error().find("\"plate\""), std::string::npos);
  EXPECT_NE(scene.error().find("\"page\""), std::string::npos);
  EXPECT_EQ(scene.stats().passes, 0);
  EXPECT_EQ(*page.painted, 0);

  Frame with = framed(set);
  with.runtime(Runtime{Recording{}})
      .pass(geometryPass("main").writes("colour"))
      .pass(texturePass("page", TextureProducer(page)));
  scene.render(with);
  EXPECT_TRUE(scene.error().empty()) << scene.error();
  EXPECT_EQ(*page.painted, 1);
  EXPECT_EQ(scene.stats().texturesPainted, 1);
  EXPECT_EQ(scene.stats().texturesSkipped, 0);

  // Nothing the page paints has changed, and the work skipped is counted.
  scene.render(with);
  EXPECT_EQ(*page.painted, 1);
  EXPECT_EQ(scene.stats().texturesPainted, 0);
  EXPECT_EQ(scene.stats().texturesSkipped, 1);
}

TEST_F(WorldScene, ABodyIsRepaintedInAFrameWhoseTexturePassPaintedItsPage) {
  const Page page(pagePixels());
  material::Material wearing = material::surface::program();
  wearing.slot(material::surface::kBaseColorSlot,
               produced(material::Texture(page.pixels), "page"));
  std::vector<bool> repainted;
  Frame frame = framed(group({mesh(card(20)).key("plate").fill(wearing),
                              mesh(card(20)).key("bare").at({40, 0, 0})})
                           .key("root"));
  frame.runtime(Runtime{Recording{}})
      .pass(texturePass("page", TextureProducer(page)))
      .pass(geometryPass("hand").writes("colour").body([&](const View& view) {
        repainted.clear();
        for (const Draw& body : view.draws) repainted.push_back(body.repainted);
      }));
  const auto wearer = [&](const std::vector<bool>& seen) {
    return std::count(seen.begin(), seen.end(), true);
  };
  scene.render(frame);
  ASSERT_TRUE(scene.error().empty()) << scene.error();
  ASSERT_EQ(repainted.size(), 2u);
  // The page was painted, and only the body wearing it says so…
  EXPECT_EQ(wearer(repainted), 1);
  // …nothing it paints changed, so nobody is repainted…
  scene.render(frame);
  EXPECT_EQ(wearer(repainted), 0);
  // …until it paints again.
  *page.changed = true;
  scene.render(frame);
  EXPECT_EQ(wearer(repainted), 1);
}

TEST_F(WorldScene, TheEnvironmentIsReadOnceAFrameFromTheFrame) {
  motion::Animatable<float> strength = motion::animatable(0.5f);
  Environment seen;
  const auto describe = [&] {
    Frame frame = framed(pair());
    frame
        .environment(EnvironmentMap::baked(
                         8, [](float, float) { return glm::vec3(1.0f); }),
                     {.intensity = strength, .rotation = 90.0f, .shown = 1.0f})
        .runtime(Runtime{Recording{}})
        .pass(geometryPass("hand").writes("colour").body(
            [&](const View& view) { seen = view.environment; }));
    return frame;
  };
  scene.render(describe());
  ASSERT_TRUE(scene.error().empty()) << scene.error();
  EXPECT_TRUE(seen.valid());
  EXPECT_FLOAT_EQ(seen.intensity, 0.5f);
  EXPECT_FLOAT_EQ(seen.shown, 1.0f);
  // The turn carries a world direction into the panorama: under a quarter
  // turn about y, toward -z in the world is toward +x in the panorama.
  const glm::vec3 into = seen.orientation * glm::vec3(0, 0, -1);
  EXPECT_NEAR(into.x, 1.0f, 1e-6f);
  strength = 2.0f;
  scene.render(describe());
  EXPECT_FLOAT_EQ(seen.intensity, 2.0f);
}

TEST_F(WorldScene, ARenderThatCannotRunItsPassesAnswersWhy) {
  Frame frame = framed(pair());
  frame.pass(geometryPass("main").writes("colour"));
  const auto refused = scene.render(frame);
  ASSERT_FALSE(refused);
  EXPECT_FALSE(refused.error().message.empty());
  EXPECT_EQ(refused.error().message, scene.error());
}
