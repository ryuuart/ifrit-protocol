/** @file
 * The node arithmetic under load: the segment round trip everything else
 * stands on, nodes put where a curve turns, nodes taken away, a run of
 * points fitted as cubics, the exact in-between of two outlines, and the
 * offset.
 */

#include <benchmark/benchmark.h>
#include <sigilgeometry/path/Extremes.h>
#include <sigilgeometry/path/Fit.h>
#include <sigilgeometry/path/Interpolate.h>
#include <sigilgeometry/path/Operations.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilgeometry/path/Segments.h>
#include <sigilgeometry/path/Tidy.h>

#include <vector>

#include "Figures.h"

using namespace sigil::geometry::path;
using sigil::geometry::path::figures::rippledRing;

namespace {

/** The segment reader and the way back, by node count: every node once
 *  in each direction, and the floor everything below stands on. */
void BM_SegmentsRoundTrip(benchmark::State& state) {
  const int count = (int)state.range(0);
  const SkPath path = rippledRing(count);
  for ([[maybe_unused]] auto iteration : state) {
    SkPath back = toPath(segments(path), path.getFillType());
    benchmark::DoNotOptimize(back);
  }
  state.counters["nodes/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_SegmentsRoundTrip)
    ->RangeMultiplier(4)
    ->Range(16, 1024)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

/** Nodes added where the curve turns: two cubic solves per piece. */
void BM_Extremes(benchmark::State& state) {
  const int count = (int)state.range(0);
  const SkPath path = rippledRing(count);
  for ([[maybe_unused]] auto iteration : state) {
    SkPath split = extremes(path);
    benchmark::DoNotOptimize(split);
  }
  state.counters["nodes/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_Extremes)
    ->RangeMultiplier(4)
    ->Range(16, 1024)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

/** Nodes taken away: the passes repeat while anything moves, so the
 *  worst case is a straight run cut into every node there is. */
void BM_Tidy(benchmark::State& state) {
  const int count = (int)state.range(0);
  SkPathBuilder builder;
  builder.moveTo(0, 0);
  for (int i = 1; i <= count; ++i) builder.lineTo((float)i, 0);
  const SkPath path = builder.detach();
  for ([[maybe_unused]] auto iteration : state) {
    SkPath tidied = tidy(path);
    benchmark::DoNotOptimize(tidied);
  }
  state.counters["nodes/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_Tidy)
    ->RangeMultiplier(4)
    ->Range(16, 512)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity();

/** The least-squares fit by point count: every pass is the whole run,
 *  and a split is two of them. */
void BM_FitCurve(benchmark::State& state) {
  const int count = (int)state.range(0);
  std::vector<glm::vec2> wave;
  wave.reserve((size_t)count);
  for (int i = 0; i < count; ++i)
    wave.push_back({(float)i, 30.0f * std::sin((float)i * 0.08f)});
  for ([[maybe_unused]] auto iteration : state) {
    SkPath fitted = fitCurve(wave, 0.5f);
    benchmark::DoNotOptimize(fitted);
  }
  state.counters["points/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_FitCurve)
    ->RangeMultiplier(4)
    ->Range(16, 1024)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity();

/** The exact in-between: the compatibility check and one weighted
 *  average of two point arrays. */
void BM_Interpolate(benchmark::State& state) {
  const int count = (int)state.range(0);
  const SkPath a = rippledRing(count, 200.0f);
  const SkPath b = rippledRing(count, 240.0f);
  for ([[maybe_unused]] auto iteration : state) {
    std::optional<SkPath> half = interpolate(a, b, 0.5f);
    benchmark::DoNotOptimize(half);
  }
  state.counters["nodes/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_Interpolate)
    ->RangeMultiplier(4)
    ->Range(16, 1024)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

/** The offset at either end of its position dial: the stroker where the
 *  band straddles, the contour walk where it does not. */
void BM_Offset(benchmark::State& state) {
  const SkPath path = rippledRing(64);
  const float position = (float)state.range(0) / 100.0f;
  for ([[maybe_unused]] auto iteration : state) {
    SkPath grown = operations::offset(path, 8.0f, {.position = position});
    benchmark::DoNotOptimize(grown);
  }
}
BENCHMARK(BM_Offset)->Arg(0)->Arg(50)->Unit(benchmark::kMicrosecond);

}  // namespace
