/** @file
 * world_diligent_bench — the frame on the device: turning a recipe's
 * Slang body into a pipeline, executing a frame's passes, and cooking a
 * chain. Run a Release build; Debug numbers say nothing. Needs a Vulkan
 * runtime and reports a skip without one.
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
#include <sigilgeometry/device/Device.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmeasure/time/Stopwatch.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilworld/diligent/Runtime.h>
#include <sigilworld/scene/Scene.h>

#include <algorithm>
#include <limits>
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
 *  `first_compile_ms` is that load plus the compile that provoked it,
 *  measured on the first compile this process makes and reported
 *  unchanged on every repetition, so the figure is the one-time cost
 *  rather than whichever repetition happened to be first. */
void BM_ProgramFromRecipeBody(benchmark::State& state) {
  static const double firstCompileMs = [] {
    diligent::installSlangCompiler();
    const measure::Stopwatch watch;
    std::shared_ptr<material::Program> program =
        material::program(freshRecipe(), material::Target::Slang,
                          material::Variant{diligent::kVariantLit});
    const double ms = watch.elapsedMs();
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
  const device::DeviceConfig config;
  std::string error;
  std::unique_ptr<device::Device> gpu = device::Device::create(config, &error);
  if (!gpu) {
    state.SkipWithError(error);
    return;
  }
  namespace gm = ::sigil::geometry::mesh;
  const Runtime runtime = diligent::runtime(*gpu);
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

  geometry::mesh::camera::Camera camera;
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
 *  `cook_fastest_ms` and `cook_slowest_ms` are the residue: the shortest
 *  and longest single cook inside a repetition, reported as counters and
 *  never judged. A fastest far under the mean is the device answering
 *  without having waited for the work, which is the thing that must not
 *  win a median; the two counters make it visible in the run that
 *  produced it rather than a mystery in the baseline. */
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
    const double ms = watch.elapsedMs();
    fastest = std::min(fastest, ms);
    slowest = std::max(slowest, ms);
  }
  state.counters["points/s"] = benchmark::Counter(
      (double)state.range(0), benchmark::Counter::kIsIterationInvariantRate);
  state.counters["cook_fastest_ms"] = fastest;
  state.counters["cook_slowest_ms"] = slowest;
}
BENCHMARK(BM_ChainOnDevice)
    ->Arg(20000)
    ->Arg(200000)
    ->Arg(1000000)
    ->Repetitions(kDeviceRepetitions)
    ->Unit(benchmark::kMillisecond);

}  // namespace

BENCHMARK_MAIN();
