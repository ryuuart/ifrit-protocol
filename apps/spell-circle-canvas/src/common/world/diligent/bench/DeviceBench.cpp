/** @file
 * world_diligent_bench — the frame on the device: turning a recipe's
 * Slang body into a pipeline, executing a frame's passes, what a casting
 * light's shadow adds to a lit pixel, what the shadow stage costs by what
 * moves, and cooking a chain. Run a Release
 * build; Debug numbers say nothing. Needs a Vulkan runtime and reports a
 * skip without one.
 *
 * EVERY TIMED ARM HERE IS A PROPERTY OF THIS LIBRARY, because the bench
 * ledger judges every timed arm against a band. One cost on the way to a
 * first frame is not: the Slang compiler's standard library, which a
 * process loads once. It is measured outside every timed region and
 * reported as a COUNTER, which the ledger never judges. The counters on
 * the chain cook are there for the same reason from the other side: what
 * the device schedules is reported beside the arm rather than judged
 * inside it.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <sigilgeometry/device/Device.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/pop/device/Cook.h>
#include <sigilmaterial/advanced/Program.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/core/PixelSource.h>
#include <sigilmeasure/time/Stopwatch.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilworld/World.h>
#include <sigilworld/advanced/Diligent.h>
#include <sigilworld/advanced/Skia.h>
#include <sigilworld/advanced/Stats.h>
#include <sigilworld/diligent/Runtime.h>
#include <sigilworld/scene/Scene.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace sigil;
using namespace sigil::world;
// The device every executor here stands on is SigilGeometry's — it is
// the one point in the tree that can create a Diligent device — so it is
// spelled by its own library's name and not through the feature that
// draws on it.
namespace device = sigil::geometry::device;

namespace {

/** Repetitions for an arm whose cost the device schedules. The ledger
 *  discards the first repetition and takes the median of the rest, so
 *  what this buys is a median wide enough that one repetition the device
 *  returned early cannot be it. */
constexpr int kDeviceRepetitions = 9;

struct Paint {
  glm::vec4 baseColor{1, 1, 1, 1};
};

constexpr char kPaintSlang[] = R"(
float4 surface(float2 uv) { return baseColor; }
)";

/** A FRESH RECIPE per iteration, because the program cache is keyed by
 *  recipe identity: compiling one twice would measure the lookup. */
std::shared_ptr<const material::Recipe> freshRecipe() {
  return std::make_shared<const material::Recipe>(
      material::Recipe::of<Paint>("world.bench.paint")
          .body(material::Target::Slang, kPaintSlang));
}

/** What the first frame that names a new material pays: the scaffold,
 *  the recipe's declarations and its body assembled into one module,
 *  compiled to two SPIR-V stages and reflected — with the compiler's
 *  session already standing, since a process opens one and loads its
 *  standard library with it.
 *
 *  `first_compile_milliseconds` is that load plus the compile that provoked it,
 *  measured on the first compile this process makes and reported
 *  unchanged on every repetition, so the figure is the one-time cost
 *  rather than whichever repetition happened to be first. */
void BM_ProgramFromRecipeBody(benchmark::State& state) {
  static const double firstCompileMilliseconds = [] {
    diligent::installSlangCompiler();
    const measure::Stopwatch watch;
    std::shared_ptr<material::Program> program =
        material::program(freshRecipe(), material::Target::Slang,
                          material::Variant{diligent::kVariantLit});
    const double ms = measure::Milliseconds(watch.elapsed()).count();
    benchmark::DoNotOptimize(program);
    return ms;
  }();
  state.counters["first_compile_milliseconds"] = firstCompileMilliseconds;
  for ([[maybe_unused]] auto iteration : state) {
    std::shared_ptr<material::Program> program =
        material::program(freshRecipe(), material::Target::Slang,
                          material::Variant{diligent::kVariantLit});
    benchmark::DoNotOptimize(program);
  }
}
BENCHMARK(BM_ProgramFromRecipeBody)->Unit(benchmark::kMillisecond);

/** ONE FRAME ON THE DEVICE, steady state: the describe, the extract, the
 *  ordering and the passes, with every pipeline and every mesh already
 *  uploaded — which is what a frame after the first costs. */
void BM_FrameOnDevice(benchmark::State& state) {
  const device::DeviceConfig config;
  std::string error;
  std::unique_ptr<device::Device> gpu = device::Device::create(config, &error);
  if (!gpu) {
    state.SkipWithError(error);
    return;
  }
  namespace gm = ::sigil::geometry::mesh;
  const Runtime runtime = diligent::runtime(*gpu);
  motion::Engine engine;
  Scene scene(engine);

  const std::shared_ptr<const material::Recipe> recipe = freshRecipe();
  // A four-by-four grid, so the row and the column are counted rather
  // than divided out inside a float expression.
  const Group set =
      group({directionalLight({-0.4f, -0.8f, -0.4f}).key("sun"),
             each(16,
                  [&](size_t i) {
                    const int column = (int)i % 4;
                    const int row = (int)i / 4;
                    return mesh(gm::superellipsoid({20, 20, 20}, 2.0f, 16, 12))
                        .key("body" + std::to_string(i))
                        .at({(float)column * 60.0f - 90.0f, 0.0f,
                             (float)row * 60.0f - 90.0f})
                        .fill(material::Material(recipe,
                                                 Paint{{0.7f, 0.6f, 0.4f, 1}}))
                        .collection("lit");
                  })})
          .key("set");

  geometry::mesh::camera::Camera camera;
  camera.eye = {0, 160, 320};
  Frame frame(set);
  frame.extent({640, 480})
      .defaultCamera(camera)
      .runtime(runtime)
      .pass(geometryPass("colour").writes("colour"))
      .pass(postPass("bloom").reads("colour").writes("bloom").blur(7.0f));

  scene.render(frame);
  for ([[maybe_unused]] auto iteration : state) {
    engine.advance(engine.elapsed() + motion::Duration(1.0 / 60.0));
    scene.render(frame);
  }
}
BENCHMARK(BM_FrameOnDevice)->Unit(benchmark::kMillisecond);

/** A WIDE BLOOM ON A FULL-SIZE FRAME, steady state: one point brighter
 *  than white on a 1440 by 960 frame, bloomed at a radius most of the
 *  frame wide, so what is timed is the chain of reduced levels the spread
 *  is taken at rather than taps at full size. */
void BM_WideBloomOnDevice(benchmark::State& state) {
  const device::DeviceConfig config;
  std::string error;
  std::unique_ptr<device::Device> gpu = device::Device::create(config, &error);
  if (!gpu) {
    state.SkipWithError(error);
    return;
  }
  namespace gm = ::sigil::geometry::mesh;
  const Runtime runtime = diligent::runtime(*gpu);
  motion::Engine engine;
  Scene scene(engine);
  const Group set =
      group({mesh(gm::quad(4, 4))
                 .key("spark")
                 .fill(material::from(material::Color{0, 0, 0, 1})
                           .surface({.metallic = 1.0f,
                                     .emission = {1.0f, 0.8f, 0.6f, 1},
                                     .emissionStrength = 6.0f}))})
          .key("set");
  geometry::mesh::camera::Camera camera;
  camera.eye = {0, 0, 200};
  Frame frame(set);
  frame.extent({1440, 960})
      .defaultCamera(camera)
      .runtime(runtime)
      .pass(geometryPass("colour").writes("colour"))
      .pass(postPass("bloom").reads("colour").writes("bloom").bloom(
          {.threshold = 1.0f,
           .intensity = 1.0f,
           .radius = (float)state.range(0)}));
  scene.render(frame);
  for ([[maybe_unused]] auto iteration : state) {
    engine.advance(engine.elapsed() + motion::Duration(1.0 / 60.0));
    scene.render(frame);
  }
}
BENCHMARK(BM_WideBloomOnDevice)
    ->Arg(64)
    ->Arg(1200)
    ->Unit(benchmark::kMillisecond);

/** WHAT A CASTING LIGHT'S SHADOW COSTS A LIT PIXEL, by the surface it
 *  falls on: the device time of the phase that shades a full-size frame
 *  covered by one surface, under one sun that casts a shadow or casts
 *  none. The surface is an opaque card (surface 0) or one carrying every
 *  lobe but transmission (surface 2), timed in the phase that lays the
 *  opaque bodies over, or clear glass with a volume in front of the card
 *  (surface 1), timed in the phase that sees through to what stands
 *  behind it — whose shadow is tinted by the glass, so it reads the tint
 *  as well as the filter — or the same glass casting nothing (surface 3),
 *  so the frame holds no tint and reads the filter alone. A casting
 *  light's arm less the same surface's arm under a light that casts
 *  nothing is the shadow's share, and surface 1's share less surface 3's
 *  is what the tint adds to it.
 *
 *  The time is the device's own clock for that one phase, read off a
 *  frame that asked for it, so neither the readback that waits for the
 *  frame nor anything the host does is in it. */
void BM_ShadowShareOnDevice(benchmark::State& state) {
  const device::DeviceConfig config;
  std::string error;
  std::unique_ptr<device::Device> gpu = device::Device::create(config, &error);
  if (!gpu) {
    state.SkipWithError(error);
    return;
  }
  namespace gm = ::sigil::geometry::mesh;
  const bool tinting = state.range(0) == 1;
  const bool glass = tinting || state.range(0) == 3;
  const bool everyLobe = state.range(0) == 2;
  const bool casts = state.range(1) != 0;
  const Runtime runtime = diligent::runtime(*gpu);
  motion::Engine engine;
  Scene scene(engine);
  std::vector<Node> content;
  content.push_back(
      mesh(gm::quad(400, 400))
          .key("card")
          .at({0, 0, -20})
          .fill(material::surface::program(
              {.baseColor = {0.5f, 0.5f, 0.5f, 1}, .roughness = 0.5f},
              material::surface::Reflection::SplitSum,
              {.sheen = everyLobe,
               .anisotropy = everyLobe,
               .iridescence = everyLobe,
               .specular = everyLobe,
               .clearcoat = everyLobe})));
  if (glass)
    content.push_back(mesh(gm::quad(400, 400))
                          .key("pane")
                          .castShadow(tinting)
                          .fill(material::from(material::Color{1, 1, 1, 1})
                                    .surface({.roughness = 0.0f,
                                              .transmission = 1.0f,
                                              .ior = 1.5f,
                                              .thickness = 10.0f})));
  content.push_back(
      directionalLight({-0.2f, -0.3f, -1}, {.castShadow = casts}).key("sun"));
  geometry::mesh::camera::Camera camera;
  camera.eye = {0, 0, 200};
  camera.target = {0, 0, 0};
  content.push_back(world::camera(camera).key("camera"));
  Frame frame(group(std::move(content)).key("set"));
  frame.extent({1440, 960})
      .runtime(runtime)
      .timeStages(true)
      .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  const std::string stage = glass ? "colour.seen" : "colour.laid";
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(160, 160));
  SkCanvas canvas(bitmap);
  // Every stage's time summed over the timed iterations, reported beside
  // the arm and never judged.
  std::map<std::string, double> stages;
  // One frame drawn and read back, which waits for the device; the phase's
  // time, or below zero while no timed frame has finished.
  const auto frameOnce = [&](bool counted) {
    engine.advance(engine.elapsed() + motion::Duration(1.0 / 60.0));
    scene.render(frame);
    draw(scene, canvas);
    const SceneStats& stats = scene.stats();
    if (stats.deviceTiming != DeviceTiming::Measured) return -1.0;
    double ms = -1.0;
    for (const DeviceStage& each : stats.deviceStages) {
      if (counted) stages[each.stage + "_ms"] += each.ms;
      if (each.stage == stage) ms = each.ms;
    }
    return ms;
  };
  // The device raises its clocks over the first frames it is given.
  int warm = 0;
  for (int k = 0; k < 120 && warm < 30; ++k)
    if (frameOnce(false) >= 0.0) ++warm;
  if (warm == 0) {
    state.SkipWithError("the device writes no timestamps");
    return;
  }
  for ([[maybe_unused]] auto iteration : state) {
    double ms = frameOnce(true);
    for (int k = 0; k < 12 && ms < 0.0; ++k) ms = frameOnce(true);
    state.SetIterationTime(std::max(ms, 0.0) / 1000.0);
  }
  for (const auto& [name, total] : stages)
    state.counters[name] =
        benchmark::Counter(total, benchmark::Counter::kAvgIterations);
}
BENCHMARK(BM_ShadowShareOnDevice)
    ->ArgNames({"surface", "casts"})
    ->Args({0, 0})
    ->Args({0, 1})
    ->Args({1, 0})
    ->Args({1, 1})
    ->Args({2, 0})
    ->Args({2, 1})
    ->Args({3, 0})
    ->Args({3, 1})
    ->UseManualTime()
    ->Iterations(60)
    ->Unit(benchmark::kMillisecond);

/** WHAT HAVING THE SHADOW TERMS IN A PROGRAM COSTS A CARD NO LIGHT CASTS
 *  ON: the device time of the phase that lays a full-size card under a
 *  sun that casts nothing. With the terms (terms 1), a second light in
 *  the frame casts — linked to no body, so it lights nothing and none of
 *  its maps darkens the card — which builds the card's program with the
 *  shadow filter and its loop in it; without (terms 0) nothing in the
 *  frame casts and the program holds neither. The two arms shade the same
 *  pixels to the same values, so their difference is what the terms cost
 *  a program that never runs them. */
void BM_UnshadowedCardOnDevice(benchmark::State& state) {
  const device::DeviceConfig config;
  std::string error;
  std::unique_ptr<device::Device> gpu = device::Device::create(config, &error);
  if (!gpu) {
    state.SkipWithError(error);
    return;
  }
  namespace gm = ::sigil::geometry::mesh;
  const bool terms = state.range(0) != 0;
  const Runtime runtime = diligent::runtime(*gpu);
  motion::Engine engine;
  Scene scene(engine);
  std::vector<Node> content;
  content.push_back(mesh(gm::quad(400, 400))
                        .key("card")
                        .collection("card")
                        .fill(material::surface::program(
                            {.baseColor = {0.5f, 0.5f, 0.5f, 1},
                             .roughness = 0.5f},
                            material::surface::Reflection::SplitSum)));
  content.push_back(
      directionalLight({-0.2f, -0.3f, -1}, {.castShadow = false}).key("sun"));
  if (terms)
    content.push_back(
        directionalLight({0.2f, -0.3f, -1}, {.castShadow = true})
            .key("elsewhere")
            .lightLink(Collection{.exclude = {"card"}})
            .shadowLink(Collection{.exclude = {"card"}}));
  geometry::mesh::camera::Camera camera;
  camera.eye = {0, 0, 200};
  camera.target = {0, 0, 0};
  content.push_back(world::camera(camera).key("camera"));
  Frame frame(group(std::move(content)).key("set"));
  frame.extent({1440, 960})
      .runtime(runtime)
      .timeStages(true)
      .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(160, 160));
  SkCanvas canvas(bitmap);
  const auto frameOnce = [&] {
    engine.advance(engine.elapsed() + motion::Duration(1.0 / 60.0));
    scene.render(frame);
    draw(scene, canvas);
    const SceneStats& stats = scene.stats();
    if (stats.deviceTiming != DeviceTiming::Measured) return -1.0;
    for (const DeviceStage& each : stats.deviceStages)
      if (each.stage == "colour.laid") return each.ms;
    return -1.0;
  };
  // The device raises its clocks over the first frames it is given.
  int warm = 0;
  for (int k = 0; k < 120 && warm < 30; ++k)
    if (frameOnce() >= 0.0) ++warm;
  if (warm == 0) {
    state.SkipWithError("the device writes no timestamps");
    return;
  }
  for ([[maybe_unused]] auto iteration : state) {
    double ms = frameOnce();
    for (int k = 0; k < 12 && ms < 0.0; ++k) ms = frameOnce();
    state.SetIterationTime(std::max(ms, 0.0) / 1000.0);
  }
}
BENCHMARK(BM_UnshadowedCardOnDevice)
    ->ArgNames({"terms"})
    ->Arg(0)
    ->Arg(1)
    ->UseManualTime()
    ->Iterations(60)
    ->Unit(benchmark::kMillisecond);

/** WHAT THE SHADOW STAGE COSTS A SET UNDER A PANEL, by what moves in
 *  it: the device time of a geometry pass's shadow stage over a slab and
 *  nine rounded blocks, each wearing a map so its surface decides where
 *  it covers, under a panel light casting through its six maps. Nothing
 *  moves (motion 0); the block under the panel turns on its axis
 *  (motion 1), which only the map looking straight down sees; or the
 *  panel glides across the set (motion 2), which every map sees.
 *
 *  `maps_drawn` is the maps a frame drew, on average over the timed
 *  frames, beside the stage's time. The time is the device's own clock
 *  for the shadow stage alone, read off a frame that asked for it. */
void BM_ShadowStageOnDevice(benchmark::State& state) {
  const device::DeviceConfig config;
  std::string error;
  std::unique_ptr<device::Device> gpu = device::Device::create(config, &error);
  if (!gpu) {
    state.SkipWithError(error);
    return;
  }
  namespace gm = ::sigil::geometry::mesh;
  const int64_t motion = state.range(0);
  const Runtime runtime = diligent::runtime(*gpu);
  motion::Engine engine;
  Scene scene(engine);
  SkBitmap checks;
  checks.allocPixels(SkImageInfo::MakeN32Premul(64, 64));
  for (int row = 0; row < 64; ++row)
    for (int column = 0; column < 64; ++column)
      *checks.getAddr32(column, row) =
          ((row / 8 + column / 8) % 2) ? 0xFFFFFFFFu : 0xFF606060u;
  checks.setImmutable();
  material::Material dressed =
      material::surface::program({.baseColor = {1, 1, 1, 1}});
  dressed.slot(material::surface::kBaseColorSlot,
               material::Texture(media::PixelSource(checks.asImage())));
  const gm::Mesh block = gm::superellipsoid({40, 60, 40}, 2.5f, 48, 24);
  const auto frameAt = [&](int step) {
    const float turn = motion == 1 ? (float)step * 3.0f : 0.0f;
    const float glide = motion == 2 ? std::sin((float)step * 0.05f) : 0.0f;
    std::vector<Node> content;
    content.push_back(
        rectLight({0, -1, 0}, {.width = 120,
                               .height = 80,
                               .intensity = 20.0f,
                               .castShadow = true})
            .key("panel")
            .at({160.0f * glide, 300, 0}));
    content.push_back(mesh(gm::quad(800, 800))
                          .key("slab")
                          .rotateX(-90)
                          .fill(material::surface::program(
                              {.baseColor = {0.3f, 0.3f, 0.3f, 1},
                               .roughness = 0.8f})));
    for (int row = -1; row <= 1; ++row)
      for (int column = -1; column <= 1; ++column)
        content.push_back(
            mesh(block)
                .key("block " + std::to_string(row) + " " +
                     std::to_string(column))
                .at({220.0f * (float)column, 60, 220.0f * (float)row})
                .rotateY(row == 0 && column == 0 ? turn : 0.0f)
                .fill(dressed));
    geometry::mesh::camera::Camera camera;
    camera.eye = {0, 500, 700};
    camera.target = {0, 0, 0};
    content.push_back(world::camera(camera).key("camera"));
    Frame frame(group(std::move(content)).key("set"));
    frame.extent({1440, 900})
        .runtime(runtime)
        .timeStages(true)
        .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
    return frame;
  };
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(160, 160));
  SkCanvas canvas(bitmap);
  int step = 0;
  double mapsDrawn = 0.0;
  // One frame drawn and read back, which waits for the device; the shadow
  // stage's time, or below zero while no timed frame has finished.
  const auto frameOnce = [&](bool counted) {
    engine.advance(engine.elapsed() + motion::Duration(1.0 / 60.0));
    scene.render(frameAt(step++));
    draw(scene, canvas);
    const SceneStats& stats = scene.stats();
    if (counted) mapsDrawn += (double)stats.shadowMapsDrawn;
    if (stats.deviceTiming != DeviceTiming::Measured) return -1.0;
    for (const DeviceStage& each : stats.deviceStages)
      if (each.stage == "colour.shadow") return each.ms;
    return -1.0;
  };
  // The device raises its clocks over the first frames it is given.
  int warm = 0;
  for (int k = 0; k < 120 && warm < 30; ++k)
    if (frameOnce(false) >= 0.0) ++warm;
  if (warm == 0) {
    state.SkipWithError("the device writes no timestamps");
    return;
  }
  int64_t frames = 0;
  for ([[maybe_unused]] auto iteration : state) {
    double ms = frameOnce(true);
    ++frames;
    for (int k = 0; k < 12 && ms < 0.0; ++k) {
      ms = frameOnce(true);
      ++frames;
    }
    state.SetIterationTime(std::max(ms, 0.0) / 1000.0);
  }
  state.counters["maps_drawn"] = frames ? mapsDrawn / (double)frames : 0.0;
}
BENCHMARK(BM_ShadowStageOnDevice)
    ->ArgNames({"motion"})
    ->Arg(0)
    ->Arg(1)
    ->Arg(2)
    ->UseManualTime()
    ->Iterations(60)
    ->Unit(benchmark::kMillisecond);

/** A CHAIN COOKED ON THE DEVICE, steady state: the generator's lanes
 *  uploaded, one dispatch per operator, and the one readback that brings
 *  every lane home. The readback is in the number on purpose — a cook
 *  whose answer nobody could read would not be a cook.
 *
 *  WHAT IS JUDGED IS THE MEAN COOK, over repetitions pinned here rather
 *  than taken from the ledger's default. The device does not always make
 *  a cook wait for the same thing: a repetition now and then returns a
 *  multiple faster than the cost the arm reproduces, and a baseline that
 *  adopted one of those would hold a number no later run can reach.
 *  Enough repetitions that no single one can carry the median is the
 *  cheapest defence, and the count is the arm's own property because the
 *  reason for it is.
 *
 *  `cook_fastest_milliseconds` and `cook_slowest_milliseconds` are the residue:
 * the shortest and longest single cook inside a repetition, reported as
 * counters and never judged. A fastest far under the mean is the device
 * answering without having waited for the work, which is the thing that must
 * not win a median; the two counters make it visible in the run that produced
 * it rather than a mystery in the baseline. */
void BM_ChainOnDevice(benchmark::State& state) {
  const device::DeviceConfig config;
  std::string error;
  std::unique_ptr<device::Device> gpu = device::Device::create(config, &error);
  if (!gpu) {
    state.SkipWithError(error);
    return;
  }
  namespace gm = ::sigil::geometry::mesh;
  const gm::pop::Runtime runtime = gm::pop::deviceRuntime(*gpu);
  const std::vector<glm::vec3> loop = {{-200, 0, 0},
                                       {-60, 110, 40},
                                       {90, 30, -60},
                                       {180, -80, 20},
                                       {0, -120, 30}};
  const gm::pop::Chain chain = gm::pop::on(loop)
                                   .count((int)state.range(0))
                                   .spread(24.0f)
                                   .select("core", {0, 0, 0}, 140.0f, 0.35f)
                                   .jitter(18.0f)
                                   .masked("core")
                                   .vary(0.5f)
                                   .fade({1, 0.4f, 0.2f, 1}, {0.2f, 0.5f, 1, 1})
                                   .peak(9.0f);
  // The first cook makes the pipeline and the lane buffers; what is
  // measured is every cook after it.
  gm::Cloud warm = gm::pop::cook(chain, runtime);
  benchmark::DoNotOptimize(warm);
  double fastest = std::numeric_limits<double>::infinity();
  double slowest = 0.0;
  for ([[maybe_unused]] auto iteration : state) {
    const measure::Stopwatch watch;
    gm::Cloud cooked = gm::pop::cook(chain, runtime);
    benchmark::DoNotOptimize(cooked);
    const double ms = measure::Milliseconds(watch.elapsed()).count();
    fastest = std::min(fastest, ms);
    slowest = std::max(slowest, ms);
  }
  state.counters["points/s"] = benchmark::Counter(
      (double)state.range(0), benchmark::Counter::kIsIterationInvariantRate);
  state.counters["cook_fastest_milliseconds"] = fastest;
  state.counters["cook_slowest_milliseconds"] = slowest;
}
BENCHMARK(BM_ChainOnDevice)
    ->Arg(20000)
    ->Arg(200000)
    ->Arg(1000000)
    ->Repetitions(kDeviceRepetitions)
    ->Unit(benchmark::kMillisecond);

}  // namespace
