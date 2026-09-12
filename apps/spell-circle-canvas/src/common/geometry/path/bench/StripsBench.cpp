/** @file
 * Strip joinery over a lattice the size of a real one: the outline of
 * every piece, the figure they unite into, and the laps found among
 * them, each measured over the same set of pieces.
 */

#include <benchmark/benchmark.h>
#include <sigilgeometry/path/Operations.h>

#include <cmath>
#include <vector>

using sigil::geometry::path::operations::Strip;

namespace {

/** A grid of bars crossed by a diagonal in every cell: ends that meet at
 *  the rim, and a great many crossings that do not. */
std::vector<Strip> lattice(int count) {
  std::vector<Strip> pieces;
  pieces.reserve((size_t)count);
  const int side = (int)std::sqrt((double)count / 3.0) + 1;
  const float pitch = 40.0f;
  for (int i = 0; i < side && (int)pieces.size() < count; ++i) {
    const float t = pitch * (float)i;
    pieces.push_back({{0, t}, {pitch * (float)side, t}, 6});
    if ((int)pieces.size() < count)
      pieces.push_back({{t, 0}, {t, pitch * (float)side}, 6});
    if ((int)pieces.size() < count)
      pieces.push_back({{t, 0}, {t + pitch, pitch * (float)side}, 4});
  }
  return pieces;
}

}  // namespace

static void StripOutlines(benchmark::State& state) {
  const std::vector<Strip> pieces = lattice((int)state.range(0));
  for (auto _ : state) {
    auto outlines = sigil::geometry::path::operations::stripOutlines(pieces);
    benchmark::DoNotOptimize(outlines.data());
    benchmark::ClobberMemory();
  }
}
BENCHMARK(StripOutlines)->Arg(100)->Arg(500);

static void StripsUnited(benchmark::State& state) {
  const std::vector<Strip> pieces = lattice((int)state.range(0));
  for (auto _ : state) {
    SkPath joined = sigil::geometry::path::operations::strips(pieces);
    benchmark::DoNotOptimize(joined.isEmpty());
    benchmark::ClobberMemory();
  }
}
BENCHMARK(StripsUnited)->Arg(100)->Arg(500);

static void StripLaps(benchmark::State& state) {
  const std::vector<Strip> pieces = lattice((int)state.range(0));
  for (auto _ : state) {
    auto laps = sigil::geometry::path::operations::stripLaps(pieces);
    benchmark::DoNotOptimize(laps.data());
    benchmark::ClobberMemory();
    state.counters["laps"] = (double)laps.size();
  }
}
BENCHMARK(StripLaps)->Arg(100)->Arg(500);
