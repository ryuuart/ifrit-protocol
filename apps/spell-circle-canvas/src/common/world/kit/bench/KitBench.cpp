/** @file
 * What a preset costs to compose: the trees a frame builds fresh every
 * time it is described.
 *
 * Release only; a Debug timing says nothing about the library.
 */

#include <benchmark/benchmark.h>
#include <sigilworld/kit/Kit.h>

namespace {

using namespace sigil;
using namespace sigil::world;

void ThreePoint(benchmark::State& state) {
  const kit::Rig rig;
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(kit::threePoint(rig));
}

/** The rail is redrawn per frame, so its cost is a per-frame cost. */
void Turntable(benchmark::State& state) {
  const kit::Turntable table;
  float seconds = 0.0f;
  for ([[maybe_unused]] auto iteration : state) {
    seconds += 1.0f / 60.0f;
    benchmark::DoNotOptimize(kit::turntable(table, seconds));
  }
}

void LitSet(benchmark::State& state) {
  const kit::Set set;
  float seconds = 0.0f;
  for ([[maybe_unused]] auto iteration : state) {
    seconds += 1.0f / 60.0f;
    benchmark::DoNotOptimize(
        kit::litSet(Element().key("subject"), set, seconds));
  }
}

BENCHMARK(ThreePoint);
BENCHMARK(Turntable);
BENCHMARK(LitSet);

}  // namespace

BENCHMARK_MAIN();
