/** @file
 * The ocio feature under load: the exponent LUT baked per size.
 */

#include <benchmark/benchmark.h>
#include <sigilmaterial/ocio/Ocio.h>

using namespace sigil::material;

namespace {

void ExponentLutBake(benchmark::State& state) {
  const int n = (int)state.range(0);
  for ([[maybe_unused]] auto iteration : state) {
    Material m = ocio::exponent(2.2f, n);
    benchmark::DoNotOptimize(m);
  }
  state.SetItemsProcessed(state.iterations() * (int64_t)n * n * n);
}
BENCHMARK(ExponentLutBake)->Arg(17)->Arg(33);

}  // namespace

BENCHMARK_MAIN();
