/** @file
 * The colour feature under load: the OKLab round trip per colour.
 */

#include <benchmark/benchmark.h>
#include <sigilmaterial/color/Color.h>

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

}  // namespace
