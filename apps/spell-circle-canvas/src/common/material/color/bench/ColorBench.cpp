/** @file
 * The colour feature under load: the OKLab round trip per colour, and the
 * exponent LUT baked per size.
 */

#include <benchmark/benchmark.h>
#include <sigilmaterial/color/Color.h>

#ifdef SIGILMATERIAL_ENABLE_OCIO
#include <sigilmaterial/color/Ocio.h>
#endif

using namespace sigil::material;

namespace {

void OklabRoundTrip(benchmark::State& state) {
  Color c{0.3f, 0.6f, 0.9f, 1};
  for ([[maybe_unused]] auto iteration : state) {
    c = fromOklab(toOklab(c));
    benchmark::DoNotOptimize(c);
  }
}
BENCHMARK(OklabRoundTrip);

#ifdef SIGILMATERIAL_ENABLE_OCIO
void ExponentLutBake(benchmark::State& state) {
  const int n = (int)state.range(0);
  for ([[maybe_unused]] auto iteration : state) {
    Material m = color::exponent(2.2f, n);
    benchmark::DoNotOptimize(m);
  }
  state.SetItemsProcessed(state.iterations() * (int64_t)n * n * n);
}
BENCHMARK(ExponentLutBake)->Arg(17)->Arg(33);
#endif

}  // namespace

BENCHMARK_MAIN();
