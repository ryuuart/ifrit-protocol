/** @file
 * A body whose program has not compiled yet: drawn as a stand-in by a
 * frame that will not wait, whole on a later frame, and whole on the
 * first frame of one that waits.
 *
 * Every case SKIPS where there is no Vulkan runtime to ask.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/advanced/Program.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilworld/World.h>
#include <sigilworld/advanced/Stats.h>
#include <sigilworld/scene/Scene.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "DeviceSeams.h"
#include "TestMaterial.h"

using namespace sigil;
using namespace sigil::world;
using namespace sigil::world::test;

namespace {

constexpr glm::ivec2 kExtent{96, 96};

/** A RECIPE NO OTHER CASE COMPILED, so its program is not held when the
 *  case starts. The stand-in is its colour unlit, which leaves the final
 *  stage as authored; the program is lit by the light facing it, which
 *  this light leaves well short of the authored colour. */
material::Material uncompiled(const std::string& name) {
  const auto recipe = std::make_shared<const material::Recipe>(
      material::Recipe::of<Paint>(name).body(
          material::Target::Slang,
          "\nfloat4 surface(float2 uv) { return baseColor; }\n"));
  return material::Material(recipe, Paint{{1, 0, 0, 1}});
}

Frame card(const material::Material& surface, const Runtime& runtime,
           bool standIns, bool panel = false) {
  std::vector<Node> content{
      directionalLight({0, 0, -1}, {.intensity = 0.5f}).key("sun"),
      mesh(::sigil::geometry::mesh::quad(120, 120)).key("card").fill(surface)};
  // A light with a size, which a program built for the sun alone has no
  // code for.
  if (panel)
    content.push_back(
        rectLight({0, 0, -1}, {.width = 60, .height = 60, .intensity = 1.0f})
            .key("panel")
            .at({0, 0, 40}));
  Frame frame(group(std::move(content)).key("set"));
  frame.extent(kExtent)
      .defaultCamera(diligent::levelEye())
      .runtime(runtime)
      .standIns(standIns)
      .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  return frame;
}

SkColor4f middle(Scene& scene) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(kExtent.x, kExtent.y));
  SkCanvas canvas(bitmap);
  canvas.clear(SK_ColorBLACK);
  draw(scene, canvas);
  return bitmap.getColor4f(kExtent.x / 2, kExtent.y / 2);
}

}  // namespace

TEST(StandIn, ALiveFrameDrawsTheStandInUntilTheProgramArrives) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const material::Material surface = uncompiled("world.test.stand-in.live");
  motion::Engine engine;
  Scene scene(engine);
  scene.render(card(surface, on.runtime, /*standIns=*/true));
  // The program is compiling on a worker: the body stands in its own
  // colour, and the frame says one body waited.
  EXPECT_EQ(scene.stats().programsPending, 1);
  const SkColor4f first = middle(scene);
  EXPECT_GT(first.fR, 0.95f);

  // A later frame, once the workers have compiled the program and built
  // its pipeline, draws it.
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(30);
  do {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    scene.render(card(surface, on.runtime, /*standIns=*/true));
  } while (scene.stats().programsPending > 0 &&
           std::chrono::steady_clock::now() < deadline);
  EXPECT_EQ(scene.stats().programsPending, 0);
  const SkColor4f later = middle(scene);
  EXPECT_GT(later.fR, 0.05f);
  EXPECT_LT(later.fR, 0.8f);
}

TEST(StandIn, AFrameThatWaitsHoldsTheProgramOnItsFirstFrame) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const material::Material surface = uncompiled("world.test.stand-in.waits");
  motion::Engine engine;
  Scene scene(engine);
  scene.render(card(surface, on.runtime, /*standIns=*/false));
  EXPECT_EQ(scene.stats().programsPending, 0);
  const SkColor4f first = middle(scene);
  EXPECT_GT(first.fR, 0.05f);
  EXPECT_LT(first.fR, 0.8f);
}

TEST(StandIn, ALightKindEnteringStandsInForOneFrameAtMost) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const material::Material surface =
      uncompiled("world.test.stand-in.light-enters");
  motion::Engine engine;
  Scene scene(engine);
  // The body opens and its program and pipeline stream in.
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(30);
  do {
    scene.render(card(surface, on.runtime, /*standIns=*/true));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  } while (scene.stats().programsPending > 0 &&
           std::chrono::steady_clock::now() < deadline);
  ASSERT_EQ(scene.stats().programsPending, 0);

  // A rect light enters: the frame that meets it may stand the body in
  // while its program for the new set compiles, and the frame after it,
  // asked at once, draws the body whole.
  scene.render(card(surface, on.runtime, /*standIns=*/true, /*panel=*/true));
  EXPECT_LE(scene.stats().programsPending, 1);
  scene.render(card(surface, on.runtime, /*standIns=*/true, /*panel=*/true));
  EXPECT_EQ(scene.stats().programsPending, 0);
  const SkColor4f lit = middle(scene);
  EXPECT_LT(lit.fR, 0.95f) << "the body is lit, not its unlit stand-in";
}

TEST(StandIn, AFirstLiveFrameBuildsNoPipelineOnTheThreadThatDraws) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // A RUNTIME OF ITS OWN, so its pipelines are cold: the ones every frame
  // draws with start building on the device's workers when it is made.
  const ::sigil::world::Runtime fresh = diligent::runtime(*on.device);
  // Two surfaces no other case compiled, so the first frame holds neither
  // program: every body in it is a stand-in, whose pipeline the runtime
  // started when it was made.
  const material::Material surface = uncompiled("world.test.stand-in.cold");
  const material::Material wall = uncompiled("world.test.stand-in.cold-wall");
  const auto lit = [&] {
    Frame frame = card(surface, fresh, /*standIns=*/true);
    frame.scene(group({directionalLight({0, -0.4f, -1},
                                        {.intensity = 0.8f, .castShadow = true})
                           .key("sun"),
                       mesh(::sigil::geometry::mesh::quad(120, 120))
                           .key("card")
                           .fill(surface),
                       mesh(::sigil::geometry::mesh::quad(400, 400))
                           .key("wall")
                           .at({0, 0, -60})
                           .fill(wall)})
                    .key("set"));
    return frame;
  };
  motion::Engine engine;
  Scene scene(engine);
  // What a first frame makes that is not a pipeline — the readback's
  // staging copy, the uploads — is made by a plain card first, whose
  // frame is also the moment the pipelines started with the runtime are
  // built in.
  scene.render(card(material::from(material::Color{0.2f, 0.3f, 0.4f, 1}), fresh,
                    /*standIns=*/true));
  (void)middle(scene);
  const auto timed = [&] {
    const auto start = std::chrono::steady_clock::now();
    scene.render(lit());
    (void)middle(scene);
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - start)
        .count();
  };
  const double first = timed();
  const double second = timed();
  double steady = second;
  for (int k = 0; k < 4; ++k) steady = std::min(steady, timed());
  RecordProperty("firstMs", std::to_string(first));
  RecordProperty("secondMs", std::to_string(second));
  RecordProperty("steadyMs", std::to_string(steady));
  // Within a small factor of a frame that builds nothing, with a floor
  // for a frame so short that the clock's own grain decides the ratio.
  EXPECT_LT(first, std::max(steady * 6.0, 6.0)) << first << " " << steady;
}
