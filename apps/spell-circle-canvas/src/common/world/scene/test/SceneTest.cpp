/** @file
 * The retained side: identity across a reorder, the three lifetimes
 * pulling apart under a geometry-slot change, the store sharing one
 * cooked artefact, lanes moving a placement, the bake taken once and
 * lost to a driven lane below it, and a draw that is a function of the
 * description alone.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImageInfo.h>
#include <sigilmotion/Animation.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilworld/scene/Scene.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
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

/** The bytes a scene draws into, at a fixed size, on the CPU. */
std::vector<uint8_t> plate(Scene& scene,
                           const geometry::mesh::camera::Camera& camera) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(96, 96));
  bitmap.eraseColor(SK_ColorTRANSPARENT);
  SkCanvas canvas(bitmap);
  scene.draw(canvas, camera);
  const uint8_t* pixels = (const uint8_t*)bitmap.getPixels();
  return {pixels, pixels + bitmap.computeByteSize()};
}

std::vector<uint8_t> plate(Scene& scene) {
  return plate(scene, frontCamera(320.0f));
}

bool hasInk(const std::vector<uint8_t>& pixels) {
  return std::any_of(pixels.begin(), pixels.end(),
                     [](uint8_t value) { return value != 0; });
}

/** A CLOCK AND A SCENE READING IT — what nearly every case below opens
 *  with, and the only state any of them shares. */
class WorldScene : public testing::Test {
 protected:
  motion::Ticker ticker;
  Scene scene{ticker};
};

}  // namespace

TEST_F(WorldScene, AKeyedReorderKeepsEveryNodesHandle) {
  const auto describe = [](const std::vector<std::string>& order) {
    Element root;
    root.key("root");
    for (const std::string& key : order)
      root.child(Element().key(key).mesh(card(10)));
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

TEST_F(WorldScene, AVisibleBackfaceKeepsAPlaneUnderAnOrbit) {
  geometry::mesh::camera::Camera behind = frontCamera(320.0f);
  behind.eye.z = -320;

  scene.render(Element().key("root").child(
      Element().key("card").mesh(card(24)).backface(Backface::Hidden)));
  EXPECT_FALSE(hasInk(plate(scene, behind)));

  scene.render(Element().key("root").child(
      Element().key("card").mesh(card(24)).backface(Backface::Visible)));
  EXPECT_TRUE(hasInk(plate(scene, behind)));
}

TEST_F(WorldScene, AGeometrySlotChangeKeepsTheNodeAndItsLanes) {
  choreograph::Output<float> lift = 0.0f;

  const auto describe = [&lift](bool asCloud) {
    Element body = Element().key("body").translateY(&lift);
    if (asCloud) {
      geometry::mesh::Cloud points;
      points.positions = {{-20, 0, 0}, {20, 0, 0}};
      body.cloud(points).stamp(card(4));
    } else {
      body.mesh(card(20));
    }
    return Element().key("root").child(std::move(body));
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
  root.key("root")
      .child(Element().key("left").chain(chain).stamp(card(3)).at({-40, 0, 0}))
      .child(Element().key("right").chain(chain).stamp(card(3)).at({40, 0, 0}));

  scene.render(root);
  EXPECT_EQ(scene.stats().cooked, 1);
  EXPECT_EQ(scene.stats().resources, 1);
  EXPECT_EQ(scene.referencesOf("left"), 2);
  EXPECT_EQ(scene.referencesOf("right"), 2);

  // One of them describes a different body: the shared entry stays, and
  // the other node keeps drawing from it.
  Element split;
  split.key("root")
      .child(Element().key("left").chain(chain).stamp(card(3)).at({-40, 0, 0}))
      .child(Element().key("right").mesh(card(9)).at({40, 0, 0}));
  scene.render(split);
  EXPECT_EQ(scene.stats().resources, 2);
  EXPECT_EQ(scene.referencesOf("left"), 1);
  EXPECT_EQ(scene.referencesOf("right"), 1);
}

TEST_F(WorldScene, ALaneRampsAPlacement) {
  const auto describe = [](float target) {
    return Element().key("root").child(
        Element().key("body").mesh(card(10)).translateX(
            motion::animate(motion::to(target), motion::Transition{200ms})));
  };

  const auto reach = [this] {
    const std::optional<glm::mat4> world = scene.transformOf("body");
    return world ? (*world)[3].x : std::numeric_limits<float>::quiet_NaN();
  };

  scene.render(describe(0.0f));
  EXPECT_FLOAT_EQ(reach(), 0.0f);

  scene.render(describe(100.0f));
  ticker.tick(0.1);
  scene.render(describe(100.0f));
  const float midway = reach();
  EXPECT_GT(midway, 0.0f);
  EXPECT_LT(midway, 100.0f);

  ticker.tick(0.2);
  scene.render(describe(100.0f));
  EXPECT_NEAR(reach(), 100.0f, 1e-3f);
}

TEST_F(WorldScene, ASettledSubtreeBakesOnceAndADrivenLaneBelowUnsettlesIt) {
  choreograph::Output<float> spin = 0.0f;
  const auto describe = [&spin] {
    return Element().key("root").child(Element().key("rig").child(
        Element().key("body").mesh(card(10)).rotateY(&spin)));
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
  choreograph::Output<float> pan = 0.0f;
  const auto describe = [](choreograph::Output<float>* lane) {
    return Element().key("root").child(
        Element().key("rig").translateX(lane).child(
            Element().key("body").mesh(card(20))));
  };

  // `body` declares no motion of its own, so its draw order is recorded on
  // the first frame — before any hold has warmed up, and while `rig`, whose
  // lane is live, is still painting live around it.
  scene.render(describe(&pan));

  // The lane is then assigned from OUTSIDE. Nothing about `body`'s own
  // description changed — only the matrix above it — and the order it
  // recorded carries the placement it was recorded with, so a replay draws
  // it where it used to stand.
  pan = 30.0f;
  scene.render(describe(&pan));
  EXPECT_EQ(scene.stats().replayed, 0);

  // …and it lands where a scene that has held nothing draws it.
  Scene fresh(ticker);
  choreograph::Output<float> panned = 30.0f;
  fresh.render(describe(&panned));
  EXPECT_EQ(plate(scene), plate(fresh));
}

TEST_F(WorldScene, ADrawIsAFunctionOfTheDescriptionAlone) {
  Scene second(ticker);
  const auto describe = [] {
    return Element()
        .key("root")
        .child(Element().key("sun").light(light::sun({-0.4f, -0.7f, -0.6f})))
        .child(Element().key("body").mesh(card(60)).rotateY(20.0f));
  };
  scene.render(describe());
  second.render(describe());

  const std::vector<uint8_t> a = plate(scene);
  const std::vector<uint8_t> b = plate(second);
  EXPECT_EQ(a, b);
  // …and it drew something, so the comparison is not two empty plates.
  EXPECT_NE(a, std::vector<uint8_t>(a.size(), 0));
}

TEST_F(WorldScene, EmittersAndViewpointsRideTheirNodesPlacement) {
  geometry::mesh::camera::Camera declared;
  declared.eye = {0, 0, 100};
  scene.render(Element().key("root").child(
      Element()
          .key("rig")
          .at({50, 0, 0})
          .child(Element().key("eye").camera(declared))
          .child(Element().key("lamp").light(light::point({0, 20, 0})))));

  const std::optional<geometry::mesh::camera::Camera> camera = scene.camera();
  ASSERT_TRUE(camera.has_value());
  EXPECT_FLOAT_EQ(camera->eye.x, 50.0f);
  EXPECT_FLOAT_EQ(camera->eye.z, 100.0f);

  const std::vector<light::Light> lights = scene.lights();
  ASSERT_EQ(lights.size(), 1u);
  EXPECT_FLOAT_EQ(lights.front().position.x, 50.0f);
  EXPECT_FLOAT_EQ(lights.front().position.y, 20.0f);
}

TEST_F(WorldScene, AnEmitterDialReachesTheLightItScales) {
  choreograph::Output<float> strength = 0.25f;
  choreograph::Output<float> red = 1.0f;

  const auto describe = [&] {
    return Element().key("root").child(
        Element()
            .key("lamp")
            .light(light::point({0, 0, 0}, {0.1f, 0.2f, 0.3f, 1.0f}, 0.6f))
            .intensity(&strength)
            .emission(&red, 0.5f, 0.5f));
  };

  scene.render(describe());
  std::vector<light::Light> lights = scene.lights();
  ASSERT_EQ(lights.size(), 1u);
  EXPECT_FLOAT_EQ(lights.front().intensity, 0.25f);
  EXPECT_FLOAT_EQ(lights.front().color.r, 1.0f);
  EXPECT_FLOAT_EQ(lights.front().color.g, 0.5f);

  // A LANE, not a description: the value moves and the tree is unchanged.
  strength = 0.9f;
  red = 0.2f;
  scene.render(describe());
  lights = scene.lights();
  ASSERT_EQ(lights.size(), 1u);
  EXPECT_FLOAT_EQ(lights.front().intensity, 0.9f);
  EXPECT_FLOAT_EQ(lights.front().color.r, 0.2f);
}

TEST_F(WorldScene, AnEmitterWithNoDialsShinesAsItWasDeclared) {
  scene.render(Element().key("root").child(Element().key("lamp").light(
      light::point({0, 0, 0}, {0.3f, 0.6f, 0.9f, 1.0f}, 0.4f))));
  const std::vector<light::Light> lights = scene.lights();
  ASSERT_EQ(lights.size(), 1u);
  EXPECT_FLOAT_EQ(lights.front().intensity, 0.4f);
  EXPECT_FLOAT_EQ(lights.front().color.b, 0.9f);
}

TEST_F(WorldScene, RetiringANodeHandsBackItsEntityAndItsArtefact) {
  scene.render(
      Element().key("root").child(Element().key("body").mesh(card(10))));
  EXPECT_EQ(scene.stats().resources, 1);
  EXPECT_NE(scene.handleOf("body"), 0u);

  scene.render(Element().key("root"));
  EXPECT_EQ(scene.handleOf("body"), 0u);
  EXPECT_EQ(scene.stats().resources, 0);
  EXPECT_EQ(scene.stats().reconcile.retired, 1);
}

// ---- frames: passes, ordering, readbacks -----------------------------------

namespace {

constexpr SkISize kFrameExtent{96, 96};

/** A set with one plain body on the left and one tagged "glow" on the
 *  right, so a selection is visible as which half is painted. */
Element pair() {
  return Element()
      .key("root")
      .child(Element().key("left").at({-46, 0, 0}).mesh(card(34)).tag("plain"))
      .child(Element().key("right").at({46, 0, 0}).mesh(card(34)).tag("glow"));
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
      SkImageInfo::MakeN32Premul(kFrameExtent.width(), kFrameExtent.height()));
  bitmap.eraseColor(SK_ColorTRANSPARENT);
  SkCanvas canvas(bitmap);
  scene.draw(canvas);
  return bitmap;
}

/** …and how much ink stands in one half of it. */
int inkIn(Scene& scene, bool leftHalf) {
  return test::paintedIn(present(scene), leftHalf);
}

}  // namespace

TEST_F(WorldScene, AFrameWithNoPassesDrawsTheSceneItIs) {
  scene.render(framed(pair()));
  EXPECT_TRUE(scene.error().empty());
  EXPECT_EQ(scene.stats().passes, 0);
  // Nothing ran, and both bodies are there: a frame that declares no pass
  // is its scene rather than an empty picture.
  EXPECT_GT(inkIn(scene, /*leftHalf=*/true), 0);
  EXPECT_GT(inkIn(scene, /*leftHalf=*/false), 0);
}

TEST_F(WorldScene, APassSeesWhatExtractWroteAndNotTheTree) {
  std::vector<std::string> keys;
  std::vector<std::string> ancestors;
  std::vector<std::string> tags;
  Frame frame = framed(pair());
  frame.pass(geometryPass("hand").writes("colour").body(
      [&](const View& view, Targets&) {
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

TEST_F(WorldScene, ACulledGeometryPassDrawsOnlyItsSelection) {
  Frame frame = framed(pair());
  frame.pass(geometryPass("glow").only(sel::tag("glow")).writes("colour"));
  scene.render(frame);

  ASSERT_TRUE(scene.error().empty());
  EXPECT_EQ(inkIn(scene, /*leftHalf=*/true), 0);
  EXPECT_GT(inkIn(scene, /*leftHalf=*/false), 0);
}

TEST_F(WorldScene, ANarrowedPostPassReachesOnlyItsCoverage) {
  Scene& plain = scene;
  Scene masked(ticker);
  Frame flat = framed(pair());
  flat.pass(geometryPass("main").writes("colour"));
  plain.render(flat);

  Frame graded = framed(pair());
  graded.pass(geometryPass("main").writes("colour"))
      .pass(postPass("dim")
                .reads("colour")
                .writes("dim")
                .only(sel::tag("glow"))
                .levels(0.2f, 0.0f));
  masked.render(graded);
  ASSERT_TRUE(masked.error().empty());

  // The unselected half is byte for byte what it was; the selected half
  // is not.
  EXPECT_EQ(inkIn(plain, true), inkIn(masked, true));
  const SkBitmap before = present(plain);
  const SkBitmap after = present(masked);
  const int y = kFrameExtent.height() / 2;
  EXPECT_EQ(before.getColor(kFrameExtent.width() / 4, y),
            after.getColor(kFrameExtent.width() / 4, y));
  EXPECT_NE(before.getColor(kFrameExtent.width() * 3 / 4, y),
            after.getColor(kFrameExtent.width() * 3 / 4, y));
}

TEST_F(WorldScene, AReadbackIsHandedOverTheFrameAfter) {
  int calls = 0;
  uint64_t at = 0;
  bool hadImage = false;
  const auto describe = [&] {
    Frame frame = framed(pair());
    frame.pass(geometryPass("main").writes("colour"))
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
  frame.pass(postPass("a").reads("second").writes("first"))
      .pass(postPass("b").reads("first").writes("second"));
  scene.render(frame);

  EXPECT_FALSE(scene.error().empty());
  EXPECT_EQ(scene.stats().passes, 0);
}

TEST_F(WorldScene, AFrameThatDeclaresPassesAndNoExtentSaysSo) {
  Frame frame(pair());
  frame.pass(geometryPass("main").writes("colour"));
  scene.render(frame);
  EXPECT_FALSE(scene.error().empty());
  EXPECT_EQ(scene.stats().passes, 0);
}

TEST_F(WorldScene, TheOrderingsCountsAreOnTheFramesTally) {
  Frame frame = framed(pair());
  frame.pass(geometryPass("main").writes("colour"))
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
  // …and the surfaces the ordering asked for are the surfaces that were
  // made.
  EXPECT_EQ(scene.targets().surfaces(), scene.plan().surfaces());
}
