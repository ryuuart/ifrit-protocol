/** @file
 * The divided range over a body that does nothing but touch its item, at
 * three sizes: below one grain, a few grains, and many. What is timed is
 * therefore the split itself — the arithmetic in a real body would swamp
 * it — so a change in how a range is handed out shows up here rather than
 * inside whichever consumer noticed it.
 */

#include <benchmark/benchmark.h>
#include <sigilcore/schedule/Parallel.h>

#include <cstddef>
#include <vector>

using namespace sigil::core;

namespace {

constexpr size_t kGrain = 4096;

void ParallelForOverItems(benchmark::State& state) {
  const size_t count = (size_t)state.range(0);
  std::vector<float> values(count, 1.0f);
  for (auto&& _ : state) {
    schedule::parallelFor(count, kGrain, [&](size_t first, size_t last) {
      for (size_t i = first; i != last; ++i) values[i] += 1.0f;
    });
    benchmark::DoNotOptimize(values.data());
  }
  state.SetItemsProcessed((int64_t)state.iterations() * (int64_t)count);
}

/** The same work asked for one element at a time, which is what a
 *  consumer holding a container reaches for. */
void ParallelForEachElement(benchmark::State& state) {
  const size_t count = (size_t)state.range(0);
  std::vector<float> values(count, 1.0f);
  for (auto&& _ : state) {
    schedule::parallelForEach(values, kGrain,
                              [](float& value) { value += 1.0f; });
    benchmark::DoNotOptimize(values.data());
  }
  state.SetItemsProcessed((int64_t)state.iterations() * (int64_t)count);
}

BENCHMARK(ParallelForOverItems)->Arg(1024)->Arg(32768)->Arg(1048576);
BENCHMARK(ParallelForEachElement)->Arg(1024)->Arg(32768)->Arg(1048576);

}  // namespace
