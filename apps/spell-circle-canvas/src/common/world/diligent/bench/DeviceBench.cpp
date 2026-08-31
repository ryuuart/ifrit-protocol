/** @file
 * world_diligent_bench — the way in and the frame: bringing the one
 * device up, turning a recipe's Slang body into a pipeline, and
 * executing a frame's passes on the device. Run a Release build; Debug
 * numbers say nothing. Needs a Vulkan runtime and reports a skip without
 * one.
 *
 * EVERY TIMED ARM HERE IS A PROPERTY OF THIS LIBRARY, because the bench
 * ledger judges every timed arm against a band. Two costs on the way to
 * a first frame are not: the driver's, which makes a Vulkan device more
 * slowly the more devices a process has already made, and the Slang
 * compiler's standard library, which a process loads once. Both are
 * measured outside every timed region and reported as COUNTERS, which
 * the ledger prints and never judges.
 */

#include <benchmark/benchmark.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilskia/device/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilworld/diligent/Device.h>
#include <sigilworld/diligent/Pop.h>
#include <sigilworld/diligent/Runtime.h>
#include <sigilworld/scene/Scene.h>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

// The adoption itself, which is this library's own seam rather than a
// public header of it.
#include "AdoptDevice.h"

using namespace sigil;
using namespace sigil::world;

namespace {

/** THE WAY IN, less the driver: the Vulkan handles read off Diligent's
 *  interfaces, those handles and the loader entry point this process
 *  already opened handed to SigilSkia, and Graphite stood up on what
 *  comes back. That is the whole of what this library does to turn one
 *  device into a device both APIs draw on, and it is measured against a
 *  device that is already standing.
 *
 *  `bringup_ms` is the whole way in — the driver's device creation and
 *  this — for the one device the arm adopts, taken once per repetition
 *  and outside every timed region. Teardown is untimed. */
void BM_DeviceAdopt(benchmark::State& state) {
  const diligent::DeviceConfig config;
  std::string error;
  const std::chrono::steady_clock::time_point started =
      std::chrono::steady_clock::now();
  std::unique_ptr<diligent::Device> device =
      diligent::Device::create(config, &error);
  const std::chrono::steady_clock::duration bringUp =
      std::chrono::steady_clock::now() - started;
  if (!device) {
    state.SkipWithError(error);
    return;
  }
  if (!device->gpu()) {
    state.SkipWithError("the device was created but not adopted");
    return;
  }
  state.counters["bringup_ms"] =
      std::chrono::duration<double, std::milli>(bringUp).count();

  for ([[maybe_unused]] auto iteration : state) {
    std::unique_ptr<skia::GpuDevice> gpu = diligent::adoptVulkanDevice(
        device->renderDevice(), device->context(), &error);
    std::unique_ptr<skia::GraphiteContext> graphite;
    if (gpu) graphite = skia::GraphiteContext::create(*gpu);
    benchmark::DoNotOptimize(graphite);
    state.PauseTiming();
    // Graphite borrows the adopted device, so it goes first; the adopted
    // device frees none of the Vulkan objects Diligent owns.
    graphite.reset();
    gpu.reset();
    state.ResumeTiming();
  }
}
BENCHMARK(BM_DeviceAdopt)->Unit(benchmark::kMillisecond);

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
 *  `first_compile_ms` is that load plus the compile that provoked it,
 *  measured on the first compile this process makes and reported
 *  unchanged on every repetition, so the figure is the one-time cost
 *  rather than whichever repetition happened to be first. */
void BM_ProgramFromRecipeBody(benchmark::State& state) {
  static const double firstCompileMs = [] {
    diligent::installSlangCompiler();
    const std::chrono::steady_clock::time_point started =
        std::chrono::steady_clock::now();
    std::shared_ptr<material::Program> program =
        material::program(freshRecipe(), material::Target::Slang,
                          material::Variant{diligent::kVariantLit});
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - started)
                          .count();
    benchmark::DoNotOptimize(program);
    return ms;
  }();
  state.counters["first_compile_ms"] = firstCompileMs;
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
  const diligent::DeviceConfig config;
  std::string error;
  std::unique_ptr<diligent::Device> device =
      diligent::Device::create(config, &error);
  if (!device) {
    state.SkipWithError(error);
    return;
  }
  namespace gm = ::sigil::geometry::mesh;
  const Runtime runtime = diligent::runtime(*device);
  motion::Ticker ticker;
  Scene scene(ticker);

  const std::shared_ptr<const material::Recipe> recipe = freshRecipe();
  Element set;
  set.key("set").child(
      Element().key("sun").light(light::sun({-0.4f, -0.8f, -0.4f})));
  // A four-by-four grid, so the row and the column are counted rather
  // than divided out inside a float expression.
  for (int i = 0; i < 16; ++i) {
    const int column = i % 4;
    const int row = i / 4;
    set.child(
        Element()
            .key("body" + std::to_string(i))
            .at({(float)column * 60.0f - 90.0f, 0.0f,
                 (float)row * 60.0f - 90.0f})
            .mesh(gm::superellipsoid({20, 20, 20}, 2.0f, 16, 12))
            .fill(material::Material(recipe, Paint{{0.7f, 0.6f, 0.4f, 1}}))
            .tag("lit"));
  }

  Camera camera;
  camera.eye = {0, 160, 320};
  Frame frame(set);
  frame.extent({640, 480})
      .camera(camera)
      .runtime(runtime)
      .pass(geometryPass("colour").writes("colour"))
      .pass(postPass("bloom").reads("colour").writes("bloom").blur(7.0f));

  scene.render(frame);
  for ([[maybe_unused]] auto iteration : state) {
    ticker.tick(1.0 / 60.0);
    scene.render(frame);
  }
}
BENCHMARK(BM_FrameOnDevice)->Unit(benchmark::kMillisecond);

/** A CHAIN COOKED ON THE DEVICE, steady state: the generator's lanes
 *  uploaded, one dispatch per operator, and the one readback that brings
 *  every lane home. The readback is in the number on purpose — a cook
 *  whose answer nobody could read would not be a cook. */
void BM_ChainOnDevice(benchmark::State& state) {
  const diligent::DeviceConfig config;
  std::string error;
  std::unique_ptr<diligent::Device> device =
      diligent::Device::create(config, &error);
  if (!device) {
    state.SkipWithError(error);
    return;
  }
  namespace gm = ::sigil::geometry::mesh;
  const gm::pop::Runtime runtime = diligent::popRuntime(*device);
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
  for ([[maybe_unused]] auto iteration : state) {
    gm::Cloud cooked = gm::pop::cook(chain, runtime);
    benchmark::DoNotOptimize(cooked);
  }
  state.counters["points/s"] = benchmark::Counter(
      (double)state.range(0), benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_ChainOnDevice)
    ->Arg(20000)
    ->Arg(200000)
    ->Unit(benchmark::kMillisecond);

}  // namespace

BENCHMARK_MAIN();
