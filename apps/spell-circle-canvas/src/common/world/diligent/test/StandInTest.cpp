/** @file
 * A body whose program has not compiled yet: drawn as a stand-in by a
 * frame that will not wait, whole on a later frame, and whole on the
 * first frame of one that waits; a live frame waits for no pipeline a
 * newly arrived program is drawn with.
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

// Whether the sanitizer or coverage instrumentation is compiled in.
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
constexpr bool kInstrumented = true;
#elif defined(__has_feature)
constexpr bool kInstrumented =
    __has_feature(address_sanitizer) || __has_feature(thread_sanitizer);
#else
constexpr bool kInstrumented = false;
#endif

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
  // colour, and the frame says one body waited. The stand-in's own
  // pipeline starts building when the runtime is made, and on a device
  // other processes are driving a frame can come before it stands — the
  // body is then left out of that frame — so frames are drawn until the
  // stand-in is.
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(30);
  SkColor4f first = middle(scene);
  while (first.fR <= 0.95f && scene.stats().programsPending > 0 &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    scene.render(card(surface, on.runtime, /*standIns=*/true));
    first = middle(scene);
  }
  EXPECT_EQ(scene.stats().programsPending, 1);
  EXPECT_GT(first.fR, 0.95f);

  // A later frame, once the workers have compiled the program and built
  // its pipeline, draws it.
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
  double slowest = second;
  for (int k = 0; k < 6; ++k) {
    const double one = timed();
    steady = std::min(steady, one);
    slowest = std::max(slowest, one);
  }
  RecordProperty("firstMilliseconds", std::to_string(first));
  RecordProperty("secondMilliseconds", std::to_string(second));
  RecordProperty("steadyMilliseconds", std::to_string(steady));
  RecordProperty("slowestMilliseconds", std::to_string(slowest));
  // Against the SLOWEST frame that builds nothing, not the fastest: a GPU
  // other processes are driving stretches every frame of this run alike,
  // and the fastest of them is the one the load spared. What the bound
  // catches is a compile through the driver inside the first frame, which
  // the frames after it never repeat. The floor is for a frame so short
  // that the clock's own grain decides the ratio.
  EXPECT_LT(first, std::max(slowest * 4.0, 12.0))
      << first << " against " << slowest << " (fastest " << steady << ")";
}

TEST(StandIn, ALiveFrameDoesNotWaitForANewlyArrivedProgramsPipeline) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // A surface no other case compiled, worn by a body that enters a scene
  // already running: its program compiles on a worker, and the frame it
  // arrives in finds the pipeline it is drawn with not yet built.
  const material::Material arriving =
      uncompiled("world.test.stand-in.arrives-mid-run");
  const material::Material steadyCard =
      material::from(material::Color{0.2f, 0.3f, 0.4f, 1});
  const auto frameOf = [&](bool withArrival) {
    std::vector<Node> content{
        directionalLight({0, 0, -1}, {.intensity = 0.5f}).key("sun"),
        mesh(::sigil::geometry::mesh::quad(120, 120))
            .key("card")
            .fill(steadyCard)};
    if (withArrival)
      content.push_back(mesh(::sigil::geometry::mesh::quad(40, 40))
                            .key("arrival")
                            .at({0, 0, 5})
                            .fill(arriving));
    Frame frame(group(std::move(content)).key("set"));
    frame.extent(kExtent)
        .defaultCamera(diligent::levelEye())
        .runtime(on.runtime)
        .standIns(true)
        .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
    return frame;
  };
  motion::Engine engine;
  Scene scene(engine);
  const auto timed = [&](bool withArrival) {
    const auto start = std::chrono::steady_clock::now();
    scene.render(frameOf(withArrival));
    (void)middle(scene);
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - start)
        .count();
  };
  // The steady frame, once the running scene's own programs and
  // pipelines are in hand.
  const auto settled =
      std::chrono::steady_clock::now() + std::chrono::seconds(30);
  do {
    (void)timed(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  } while (scene.stats().programsPending > 0 &&
           std::chrono::steady_clock::now() < settled);
  ASSERT_EQ(scene.stats().programsPending, 0);
  double steady = timed(false);
  double slowest = steady;
  for (int k = 0; k < 6; ++k) {
    const double one = timed(false);
    steady = std::min(steady, one);
    slowest = std::max(slowest, one);
  }

  // The body enters, and every frame until it is drawn whole is timed.
  double longest = 0.0;
  int frames = 0;
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(30);
  do {
    longest = std::max(longest, timed(true));
    ++frames;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  } while (scene.stats().programsPending > 0 &&
           std::chrono::steady_clock::now() < deadline);
  ASSERT_EQ(scene.stats().programsPending, 0);
  RecordProperty("steadyMilliseconds", std::to_string(steady));
  RecordProperty("slowestMilliseconds", std::to_string(slowest));
  RecordProperty("longestMilliseconds", std::to_string(longest));
  RecordProperty("frames", std::to_string(frames));
  // Within a small factor of the slowest frame that builds nothing, for
  // the reason the first-frame case reads that one: a GPU other processes
  // drive stretches every frame alike. The floor is for a frame so short
  // that the clock's own grain decides the ratio. An instrumented build
  // slows the host side of a frame by more than the factor allows, and
  // its claim is memory, not time, so the bound is read only where the
  // code runs as shipped.
  if (!kInstrumented) {
    EXPECT_LT(longest, std::max(slowest * 4.0, 12.0))
        << longest << " against " << slowest << " (fastest " << steady << ")";
  }
  // The arrival is drawn whole: lit, short of its authored red.
  const SkColor4f arrived = middle(scene);
  EXPECT_LT(arrived.fR, 0.8f);
  EXPECT_GT(arrived.fR, 0.05f);
}
