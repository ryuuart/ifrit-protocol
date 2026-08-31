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

#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

using namespace sigil;
using namespace sigil::world;
using namespace std::chrono_literals;

namespace {

Mesh triangle(float size) {
  Mesh m;
  m.positions = {{-size, -size, 0}, {size, -size, 0}, {0, size, 0}};
  m.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
  m.uvs = {{0, 0}, {1, 0}, {0.5f, 1}};
  m.indices = {0, 1, 2};
  return m;
}

/** A closed loop and a chain of stamps riding a window of it — the same
 *  VALUE wherever it is built, which is what makes it shareable. */
Chain cometChain() {
  std::vector<glm::vec3> loop = {
      {-80, 0, 0}, {0, 60, 0}, {80, 0, 0}, {0, -60, 0}};
  return geometry::mesh::pop::on(loop)
      .count(64)
      .window(0.9f, 0.4f)
      .spread(6.0f);
}

Camera frontCamera() {
  Camera camera;
  camera.eye = {0, 0, 320};
  camera.target = {0, 0, 0};
  return camera;
}

/** The bytes a scene draws into, at a fixed size, on the CPU. */
std::vector<uint8_t> plate(Scene& scene) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(96, 96));
  bitmap.eraseColor(SK_ColorBLACK);
  SkCanvas canvas(bitmap);
  scene.draw(canvas, frontCamera());
  const uint8_t* pixels = (const uint8_t*)bitmap.getPixels();
  return {pixels, pixels + bitmap.computeByteSize()};
}

}  // namespace

TEST(WorldScene, AKeyedReorderKeepsEveryNodesHandle) {
  motion::Ticker ticker;
  Scene scene(ticker);
  const auto describe = [](const std::vector<std::string>& order) {
    Element root;
    root.key("root");
    for (const std::string& key : order)
      root.child(Element().key(key).mesh(triangle(10)));
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

TEST(WorldScene, AGeometrySlotChangeKeepsTheNodeAndItsLanes) {
  motion::Ticker ticker;
  Scene scene(ticker);
  choreograph::Output<float> lift = 0.0f;

  const auto describe = [&lift](bool asCloud) {
    Element body = Element().key("body").translateY(&lift);
    if (asCloud) {
      Cloud points;
      points.positions = {{-20, 0, 0}, {20, 0, 0}};
      body.cloud(points).stamp(triangle(4));
    } else {
      body.mesh(triangle(20));
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

TEST(WorldScene, TwoNodesDescribingOneChainShareOneCook) {
  motion::Ticker ticker;
  Scene scene(ticker);
  const Chain chain = cometChain();
  Element root;
  root.key("root")
      .child(
          Element().key("left").chain(chain).stamp(triangle(3)).at({-40, 0, 0}))
      .child(Element()
                 .key("right")
                 .chain(chain)
                 .stamp(triangle(3))
                 .at({40, 0, 0}));

  scene.render(root);
  EXPECT_EQ(scene.stats().cooked, 1);
  EXPECT_EQ(scene.stats().resources, 1);
  EXPECT_EQ(scene.referencesOf("left"), 2);
  EXPECT_EQ(scene.referencesOf("right"), 2);

  // One of them describes a different body: the shared entry stays, and
  // the other node keeps drawing from it.
  Element split;
  split.key("root")
      .child(
          Element().key("left").chain(chain).stamp(triangle(3)).at({-40, 0, 0}))
      .child(Element().key("right").mesh(triangle(9)).at({40, 0, 0}));
  scene.render(split);
  EXPECT_EQ(scene.stats().resources, 2);
  EXPECT_EQ(scene.referencesOf("left"), 1);
  EXPECT_EQ(scene.referencesOf("right"), 1);
}

TEST(WorldScene, ALaneRampsAPlacement) {
  motion::Ticker ticker;
  Scene scene(ticker);
  const auto describe = [](float target) {
    return Element().key("root").child(
        Element()
            .key("body")
            .mesh(triangle(10))
            .translateX(motion::animate(motion::to(target),
                                        motion::Transition{200ms})));
  };

  const auto reach = [&scene] {
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

TEST(WorldScene, ASettledSubtreeBakesOnceAndADrivenLaneBelowUnsettlesIt) {
  motion::Ticker ticker;
  Scene scene(ticker);
  choreograph::Output<float> spin = 0.0f;
  const auto describe = [&spin] {
    return Element().key("root").child(Element().key("rig").child(
        Element().key("body").mesh(triangle(10)).rotateY(&spin)));
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

TEST(WorldScene, ADrawIsAFunctionOfTheDescriptionAlone) {
  motion::Ticker ticker;
  Scene first(ticker);
  Scene second(ticker);
  const auto describe = [] {
    return Element()
        .key("root")
        .child(Element().key("sun").light(sun({-0.4f, -0.7f, -0.6f})))
        .child(Element().key("body").mesh(triangle(60)).rotateY(20.0f));
  };
  first.render(describe());
  second.render(describe());

  const std::vector<uint8_t> a = plate(first);
  const std::vector<uint8_t> b = plate(second);
  EXPECT_EQ(a, b);
  // …and it drew something, so the comparison is not two empty plates.
  EXPECT_NE(a, std::vector<uint8_t>(a.size(), 0));
}

TEST(WorldScene, EmittersAndViewpointsRideTheirNodesPlacement) {
  motion::Ticker ticker;
  Scene scene(ticker);
  Camera declared;
  declared.eye = {0, 0, 100};
  scene.render(Element().key("root").child(
      Element()
          .key("rig")
          .at({50, 0, 0})
          .child(Element().key("eye").camera(declared))
          .child(Element().key("lamp").light(point({0, 20, 0})))));

  const std::optional<Camera> camera = scene.camera();
  ASSERT_TRUE(camera.has_value());
  EXPECT_FLOAT_EQ(camera->eye.x, 50.0f);
  EXPECT_FLOAT_EQ(camera->eye.z, 100.0f);

  const std::vector<Light> lights = scene.lights();
  ASSERT_EQ(lights.size(), 1u);
  EXPECT_FLOAT_EQ(lights.front().position.x, 50.0f);
  EXPECT_FLOAT_EQ(lights.front().position.y, 20.0f);
}

TEST(WorldScene, AnEmitterDialReachesTheLightItScales) {
  motion::Ticker ticker;
  Scene scene(ticker);
  choreograph::Output<float> strength = 0.25f;
  choreograph::Output<float> red = 1.0f;

  const auto describe = [&] {
    return Element().key("root").child(
        Element()
            .key("lamp")
            .light(point({0, 0, 0}, {0.1f, 0.2f, 0.3f, 1.0f}, 0.6f))
            .intensity(&strength)
            .emission(&red, 0.5f, 0.5f));
  };

  scene.render(describe());
  std::vector<Light> lights = scene.lights();
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

TEST(WorldScene, AnEmitterWithNoDialsShinesAsItWasDeclared) {
  motion::Ticker ticker;
  Scene scene(ticker);
  scene.render(Element().key("root").child(Element().key("lamp").light(
      point({0, 0, 0}, {0.3f, 0.6f, 0.9f, 1.0f}, 0.4f))));
  const std::vector<Light> lights = scene.lights();
  ASSERT_EQ(lights.size(), 1u);
  EXPECT_FLOAT_EQ(lights.front().intensity, 0.4f);
  EXPECT_FLOAT_EQ(lights.front().color.b, 0.9f);
}

TEST(WorldScene, RetiringANodeHandsBackItsEntityAndItsArtefact) {
  motion::Ticker ticker;
  Scene scene(ticker);
  scene.render(
      Element().key("root").child(Element().key("body").mesh(triangle(10))));
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
      .child(
          Element().key("left").at({-46, 0, 0}).mesh(triangle(34)).tag("plain"))
      .child(
          Element().key("right").at({46, 0, 0}).mesh(triangle(34)).tag("glow"));
}

Frame framed(Element scene) {
  Frame frame(std::move(scene));
  frame.extent(kFrameExtent).camera(frontCamera());
  return frame;
}

/** How much ink stands in one half of what a frame presented. */
int paintedIn(Scene& scene, bool leftHalf) {
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(kFrameExtent.width(), kFrameExtent.height()));
  bitmap.eraseColor(SK_ColorTRANSPARENT);
  SkCanvas canvas(bitmap);
  scene.draw(canvas);
  int count = 0;
  const int mid = bitmap.width() / 2;
  for (int y = 0; y < bitmap.height(); ++y)
    for (int x = 0; x < bitmap.width(); ++x) {
      if (leftHalf != (x < mid)) continue;
      if (SkColorGetA(bitmap.getColor(x, y)) > 0) ++count;
    }
  return count;
}

}  // namespace

TEST(WorldScene, AFrameWithNoPassesDrawsTheSceneItIs) {
  motion::Ticker ticker;
  Scene scene(ticker);
  scene.render(framed(pair()));
  EXPECT_EQ(scene.stats().passes, 0);
  EXPECT_TRUE(scene.plan().steps().empty());
  EXPECT_TRUE(scene.error().empty());
}

TEST(WorldScene, APassSeesWhatExtractWroteAndNotTheTree) {
  motion::Ticker ticker;
  Scene scene(ticker);
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

TEST(WorldScene, ACulledGeometryPassDrawsOnlyItsSelection) {
  motion::Ticker ticker;
  Scene scene(ticker);
  Frame frame = framed(pair());
  frame.pass(geometryPass("glow").only(sel::tag("glow")).writes("colour"));
  scene.render(frame);

  ASSERT_TRUE(scene.error().empty());
  ASSERT_EQ(scene.plan().steps().size(), 1u);
  EXPECT_EQ(scene.plan().steps().front().realisation, Selection::Cull);
  EXPECT_EQ(paintedIn(scene, /*leftHalf=*/true), 0);
  EXPECT_GT(paintedIn(scene, /*leftHalf=*/false), 0);
}

TEST(WorldScene, ANarrowedPostPassReachesOnlyItsCoverage) {
  motion::Ticker ticker;
  Scene plain(ticker);
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
  EXPECT_EQ(paintedIn(plain, true), paintedIn(masked, true));
  SkBitmap before;
  SkBitmap after;
  before.allocPixels(
      SkImageInfo::MakeN32Premul(kFrameExtent.width(), kFrameExtent.height()));
  after.allocPixels(
      SkImageInfo::MakeN32Premul(kFrameExtent.width(), kFrameExtent.height()));
  before.eraseColor(SK_ColorTRANSPARENT);
  after.eraseColor(SK_ColorTRANSPARENT);
  SkCanvas one(before);
  SkCanvas two(after);
  plain.draw(one);
  masked.draw(two);
  const int y = kFrameExtent.height() / 2;
  EXPECT_EQ(before.getColor(kFrameExtent.width() / 4, y),
            after.getColor(kFrameExtent.width() / 4, y));
  EXPECT_NE(before.getColor(kFrameExtent.width() * 3 / 4, y),
            after.getColor(kFrameExtent.width() * 3 / 4, y));
}

TEST(WorldScene, AReadbackIsHandedOverTheFrameAfter) {
  motion::Ticker ticker;
  Scene scene(ticker);
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

TEST(WorldScene, ACycleInThePassesIsAnErrorAndNothingRuns) {
  motion::Ticker ticker;
  Scene scene(ticker);
  Frame frame = framed(pair());
  frame.pass(postPass("a").reads("second").writes("first"))
      .pass(postPass("b").reads("first").writes("second"));
  scene.render(frame);

  EXPECT_FALSE(scene.error().empty());
  EXPECT_EQ(scene.stats().passes, 0);
}

TEST(WorldScene, AFrameThatDeclaresPassesAndNoExtentSaysSo) {
  motion::Ticker ticker;
  Scene scene(ticker);
  Frame frame(pair());
  frame.pass(geometryPass("main").writes("colour"));
  scene.render(frame);
  EXPECT_FALSE(scene.error().empty());
  EXPECT_EQ(scene.stats().passes, 0);
}

TEST(WorldScene, TheOrderingsCountsAreOnTheFramesTally) {
  motion::Ticker ticker;
  Scene scene(ticker);
  Frame frame = framed(pair());
  frame.pass(geometryPass("main").writes("colour"))
      .pass(postPass("half").reads("colour").writes("half"))
      .pass(postPass("quarter").reads("half").writes("quarter"))
      .pass(postPass("eighth").reads("quarter").writes("eighth"));
  scene.render(frame);

  ASSERT_TRUE(scene.error().empty());
  EXPECT_EQ(scene.stats().passes, 4);
  EXPECT_EQ(scene.stats().surfaces, scene.plan().surfaces());
  EXPECT_EQ(scene.stats().aliased, 2);
  EXPECT_GT(scene.stats().barriers, 0);
  // …and the surfaces the ordering asked for are the surfaces that were
  // made.
  EXPECT_EQ(scene.targets().surfaces(), scene.plan().surfaces());
}
