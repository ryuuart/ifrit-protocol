/** @file
 * Filling a shape with points: independent draws, the poisson-disc front
 * and the blue-noise relaxation, all over one ring with a hole so the
 * containment test is the even-odd walk a real outline costs.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkPathBuilder.h>
#include <sigilgeometry/path/Scatter.h>

#include <vector>

#include "Figures.h"

using namespace sigil::geometry::path;
using sigil::geometry::path::figures::rippledRing;

namespace {

/** The region every scatter arm fills: a ring with a hole, so the
 *  containment test is the even-odd walk a real outline costs and not a
 *  rect test. */
Region ringRegion() {
  SkPathBuilder builder;
  builder.addPath(rippledRing(64, 400.0f));
  builder.addPath(rippledRing(64, 160.0f));
  return Region::of(builder.detach(), 0.5f);
}

/** INDEPENDENT DRAWS: one containment test per accepted point plus the
 *  rejections the bounding box costs. */
void BM_ScatterRandom(benchmark::State& state) {
  const int count = (int)state.range(0);
  const Region region = ringRegion();
  for ([[maybe_unused]] auto iteration : state) {
    std::vector<glm::vec2> points = sample(region, uniform(count));
    benchmark::DoNotOptimize(points.data());
  }
  state.counters["points/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_ScatterRandom)
    ->RangeMultiplier(10)
    ->Range(1000, 100000)
    ->Unit(benchmark::kMillisecond)
    ->Complexity(benchmark::oN);

/** THE POISSON FILL: the same region filled to a separation that yields
 *  about the same count, which is the arm that says what a minimum
 *  distance costs over independence. */
void BM_ScatterPoisson(benchmark::State& state) {
  const int count = (int)state.range(0);
  const Region region = ringRegion();
  const float radius = std::sqrt(region.area() / (float)count) * 0.8f;
  for ([[maybe_unused]] auto iteration : state) {
    std::vector<glm::vec2> points = sample(region, poisson(radius));
    benchmark::DoNotOptimize(points.data());
  }
  state.SetComplexityN(count);
}
BENCHMARK(BM_ScatterPoisson)
    ->RangeMultiplier(10)
    ->Range(1000, 100000)
    ->Unit(benchmark::kMillisecond)
    ->Complexity(benchmark::oN);

/** THE RELAXATION, which is one index build and one gather per pass. */
void BM_ScatterBlueNoise(benchmark::State& state) {
  const int count = (int)state.range(0);
  const Region region = ringRegion();
  for ([[maybe_unused]] auto iteration : state) {
    std::vector<glm::vec2> points = sample(region, blueNoise(count, 1, 4));
    benchmark::DoNotOptimize(points.data());
  }
  state.SetComplexityN(count);
}
BENCHMARK(BM_ScatterBlueNoise)
    ->RangeMultiplier(10)
    ->Range(1000, 100000)
    ->Unit(benchmark::kMillisecond)
    ->Complexity(benchmark::oN);

}  // namespace
