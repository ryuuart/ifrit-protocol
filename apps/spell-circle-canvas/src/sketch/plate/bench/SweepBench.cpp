/** @file
 * The sweep's own overhead: opening a session, stepping it to a declared
 * moment and encoding the plate, for a scene small enough that what is
 * measured is the harness rather than the picture.
 */

// Registered the way a sketch file is, so the sweep walks a real registry
// rather than a fixture it would never otherwise see. It has to stand
// before the prelude: the macro chooses its form at include time.
#define SIGIL_SKETCH_STATIC "sweep_bench_probe"

#include <benchmark/benchmark.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/plate/Sweep.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <filesystem>

namespace {

using namespace sigil::sketch;
using namespace sigil::compose;

sigil::weave::FontContext& fonts() {
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

Assets& assets() {
  static auto* store = new Assets("");
  return *store;
}

struct Probe : Sketch {
  void setup(SketchContext& ctx) override {
    ctx.canvas(64, 48);
    ctx.captureAt(0.1);  // six steps: the harness, not the scene
    ctx.composer.render(
        box().width(10).height(10).fill(Fill::color({1, 0, 0, 1})));
  }
};

void Ledger(benchmark::State& state) {
  SweepOptions options;
  options.outDir =
      (std::filesystem::temp_directory_path() / "sigil_sweep_bench").string();
  options.ledger = true;
  options.noPromotion = true;
  options.only = find("sweep_bench_probe");
  for (auto&& _ : state)
    benchmark::DoNotOptimize(sweep(options, fonts(), assets()));
}
BENCHMARK(Ledger)->Unit(benchmark::kMillisecond);

}  // namespace

SIGIL_SKETCH(Probe, "Bench", "the sweep bench's own fixture")

BENCHMARK_MAIN();
