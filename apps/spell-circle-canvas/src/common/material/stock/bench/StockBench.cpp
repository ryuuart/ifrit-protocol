/** @file
 * material_stock_bench — the warm-up a host pays before its first frame:
 * gathering the catalogues off disk, and compiling what they hold. One
 * arm per family beside the whole stock, so a recipe added to one
 * catalogue is attributable to it. Run a Release build; Debug numbers
 * say nothing.
 */

#include <benchmark/benchmark.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/kit/Recipes.h>
#include <sigilmaterial/sdf/Sdf.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/stock/Stock.h>

#include <vector>

using namespace sigil::material;

namespace {

using Catalogue = std::vector<Material> (*)();

/** Reading a catalogue is reading its shader files, so this is the wait
 *  the gather spends — and the whole stock is the three of them asked
 *  side by side rather than one after another. */
void gather(benchmark::State& state, Catalogue catalogue) {
  for ([[maybe_unused]] auto iteration : state) {
    std::vector<Material> recipes = catalogue();
    benchmark::DoNotOptimize(recipes.data());
  }
}

void GatherField(benchmark::State& state) { gather(state, &field::everyRecipe); }
BENCHMARK(GatherField);
void GatherSdf(benchmark::State& state) { gather(state, &sdf::everyRecipe); }
BENCHMARK(GatherSdf);
void GatherKit(benchmark::State& state) { gather(state, &kit::everyRecipe); }
BENCHMARK(GatherKit);
void GatherStock(benchmark::State& state) {
  gather(state, &stock::everyRecipe);
}
BENCHMARK(GatherStock);

/** THE COMPILE. A program compiles once per (recipe, target, variant)
 *  and is then held for the process, so the cache is dropped between
 *  iterations with the clock stopped: what is timed is a cold warm-up,
 *  which is the one a host actually pays. The recipes are gathered
 *  outside the loop, so this arm is the compiler's time and not the
 *  disk's. */
void warm(benchmark::State& state, Catalogue catalogue) {
  skia::install();
  const std::vector<Material> recipes = catalogue();
  size_t compiled = 0;
  for ([[maybe_unused]] auto iteration : state) {
    state.PauseTiming();
    ProgramCache::shared().clear();
    state.ResumeTiming();
    WarmupResult result = warmup(recipes, Target::SkSL);
    compiled = result.unique;
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed((int64_t)(state.iterations() * compiled));
}

void WarmField(benchmark::State& state) { warm(state, &field::everyRecipe); }
BENCHMARK(WarmField);
void WarmSdf(benchmark::State& state) { warm(state, &sdf::everyRecipe); }
BENCHMARK(WarmSdf);
void WarmKit(benchmark::State& state) { warm(state, &kit::everyRecipe); }
BENCHMARK(WarmKit);
void WarmStock(benchmark::State& state) { warm(state, &stock::everyRecipe); }
BENCHMARK(WarmStock);

/** The whole call a host makes, gather and compile together, against a
 *  cache holding nothing: the number a first frame waits on. */
void WarmStockFromCold(benchmark::State& state) {
  skia::install();
  for ([[maybe_unused]] auto iteration : state) {
    state.PauseTiming();
    ProgramCache::shared().clear();
    state.ResumeTiming();
    WarmupResult result = stock::warmup(Target::SkSL);
    benchmark::DoNotOptimize(result);
  }
}
BENCHMARK(WarmStockFromCold);

}  // namespace
