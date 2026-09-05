/** @file
 * The mixers timed one call at a time, because that is how they are
 * spent: a per-index jitter calls the hash once per stamp, a cook calls
 * the PCG stream once per point, and a signature folds a handful of
 * words. Reported as a rate per call so a body that grows a step shows
 * up as a rate that fell.
 */

#include <benchmark/benchmark.h>
#include <sigilcore/compute/Compute.h>

#include <cstdint>
#include <string_view>

using namespace sigil::core;

namespace {

benchmark::Counter perCall() {
  return benchmark::Counter(1, benchmark::Counter::kIsIterationInvariantRate);
}

void BM_NoiseHash(benchmark::State& state) {
  uint32_t i = 0;
  float sink = 0;
  for ([[maybe_unused]] auto iteration : state) {
    sink += noise::hash(7u, i++);
    benchmark::DoNotOptimize(sink);
  }
  state.counters["calls/s"] = perCall();
}
BENCHMARK(BM_NoiseHash);

void BM_NoisePcgHash(benchmark::State& state) {
  uint32_t i = 0, sink = 0;
  for ([[maybe_unused]] auto iteration : state) {
    sink ^= noise::pcgHash(i++);
    benchmark::DoNotOptimize(sink);
  }
  state.counters["calls/s"] = perCall();
}
BENCHMARK(BM_NoisePcgHash);

/** The stream, which is the shape a generator scatters points with: one
 *  carried word, one unit float per draw. */
void BM_NoisePcgUnitNext(benchmark::State& state) {
  uint32_t carried = 1u;
  float sink = 0;
  for ([[maybe_unused]] auto iteration : state) {
    sink += noise::pcgUnitNext(carried);
    benchmark::DoNotOptimize(sink);
  }
  state.counters["calls/s"] = perCall();
}
BENCHMARK(BM_NoisePcgUnitNext);

/** The second stream, three shift-xors against the PCG multiply. */
void BM_NoiseXorshiftUnitNext(benchmark::State& state) {
  uint32_t carried = 1u;
  float sink = 0;
  for ([[maybe_unused]] auto iteration : state) {
    sink += noise::xorshiftUnitNext(carried);
    benchmark::DoNotOptimize(sink);
  }
  state.counters["calls/s"] = perCall();
}
BENCHMARK(BM_NoiseXorshiftUnitNext);

void BM_Fnv1aWord(benchmark::State& state) {
  uint64_t running = hash::kFnvOffset, i = 0;
  for ([[maybe_unused]] auto iteration : state) {
    running = hash::fnv1a(running, i++);
    benchmark::DoNotOptimize(running);
  }
  state.counters["calls/s"] = perCall();
}
BENCHMARK(BM_Fnv1aWord);

/** Text folds a byte at a time, so the interesting number is per byte of
 *  the names a store actually keys on. */
void BM_Fnv1aText(benchmark::State& state) {
  const std::string_view name = "procedural/sphere/subdivided";
  uint64_t running = hash::kFnvOffset;
  for ([[maybe_unused]] auto iteration : state) {
    running = hash::fnv1a(running, name);
    benchmark::DoNotOptimize(running);
  }
  state.SetBytesProcessed((int64_t)state.iterations() * (int64_t)name.size());
  state.counters["calls/s"] = perCall();
}
BENCHMARK(BM_Fnv1aText);

void BM_Combine(benchmark::State& state) {
  size_t running = 0;
  uint32_t i = 0;
  for ([[maybe_unused]] auto iteration : state) {
    running = hash::combine(running, i++);
    benchmark::DoNotOptimize(running);
  }
  state.counters["calls/s"] = perCall();
}
BENCHMARK(BM_Combine);

}  // namespace
