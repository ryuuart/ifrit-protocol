/** @file
 * world_diligent_bench — the way in and the frame: bringing the one
 * device up, turning a recipe's Slang body into a pipeline, and
 * executing a frame's passes on the device. Run a Release build; Debug
 * numbers say nothing. Needs a Vulkan runtime and reports a skip without
 * one.
 */

#include <benchmark/benchmark.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilworld/diligent/Device.h>
#include <sigilworld/diligent/Pop.h>
#include <sigilworld/diligent/Runtime.h>
#include <sigilworld/scene/Scene.h>

#include <memory>
#include <string>
#include <vector>

using namespace sigil;
using namespace sigil::world;

namespace {

/** A process pays this once, and it is what the first frame ever drawn
 *  waits on. Teardown is untimed: what is measured is the way in. */
void BM_DeviceBringUp(benchmark::State& state) {
  const diligent::DeviceConfig config;
  std::string error;
  if (!diligent::Device::create(config, &error)) {
    state.SkipWithError(error);
    return;
  }
  for ([[maybe_unused]] auto iteration : state) {
    std::unique_ptr<diligent::Device> device =
        diligent::Device::create(config, &error);
    benchmark::DoNotOptimize(device);
    state.PauseTiming();
    device.reset();
    state.ResumeTiming();
  }
}
BENCHMARK(BM_DeviceBringUp)->Unit(benchmark::kMillisecond);

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
 *  compiled to two SPIR-V stages and reflected. */
void BM_ProgramFromRecipeBody(benchmark::State& state) {
  diligent::installSlangCompiler();
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
