/** @file
 * Benchmarks of the Illustrator blend: expanding a two-key blend by
 * step count and by sample density, and the same blend ridden along a
 * spine.
 */

// geometry_path_blend_bench — what a blend costs as the number of drawn
// steps and the resampling density grow, and what threading the steps
// onto a spine adds. Run a Release build; Debug numbers say nothing.

#include <benchmark/benchmark.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <sigilgeometry/path/blend/Blend.h>

#include <cmath>
#include <numbers>
#include <vector>

using namespace sigil::geometry::path;

namespace {

/** A closed star whose points are wobbled, so resampling has real
 *  curvature to follow rather than a circle's uniform one. */
SkPath star(int points, float radius) {
  SkPathBuilder builder;
  const float step = std::numbers::pi_v<float> / (float)points;
  for (int i = 0; i < points * 2; ++i) {
    const float a = step * (float)i;
    const float r = (i % 2 == 0) ? radius : radius * 0.45f;
    const SkPoint p{r * std::cos(a), r * std::sin(a)};
    if (i == 0)
      builder.moveTo(p);
    else
      builder.lineTo(p);
  }
  builder.close();
  return builder.detach();
}

std::vector<blend::Key> twoKeys() {
  blend::Key from;
  from.path = star(5, 120);
  from.fill = {1, 0.4f, 0.1f, 1};
  blend::Key to;
  to.path = star(9, 220);
  to.fill = {0.1f, 0.5f, 1, 1};
  return {from, to};
}

void countSteps(benchmark::State& state,
                const std::vector<blend::Step>& steps) {
  state.counters["steps/s"] = benchmark::Counter(
      (double)steps.size(), benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN((int64_t)steps.size());
}

void BM_BlendSteps(benchmark::State& state) {
  const std::vector<blend::Key> keys = twoKeys();
  blend::Options options;
  options.steps = (int)state.range(0);
  std::vector<blend::Step> last;
  for ([[maybe_unused]] auto iteration : state) {
    last = blend::make(keys, options);
    benchmark::DoNotOptimize(last.data());
  }
  countSteps(state, last);
}
BENCHMARK(BM_BlendSteps)
    ->RangeMultiplier(4)
    ->Range(4, 256)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_BlendSamples(benchmark::State& state) {
  const std::vector<blend::Key> keys = twoKeys();
  blend::Options options;
  options.steps = 24;
  options.samples = (int)state.range(0);
  std::vector<blend::Step> last;
  for ([[maybe_unused]] auto iteration : state) {
    last = blend::make(keys, options);
    benchmark::DoNotOptimize(last.data());
  }
  countSteps(state, last);
}
BENCHMARK(BM_BlendSamples)
    ->RangeMultiplier(4)
    ->Range(32, 2048)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_BlendAlongSpine(benchmark::State& state) {
  const std::vector<blend::Key> keys = twoKeys();
  SkPathBuilder spine;
  spine.moveTo(-400, 0);
  spine.cubicTo({-120, -260}, {120, 260}, {400, 0});
  blend::Options options;
  options.steps = (int)state.range(0);
  options.spine = spine.detach();
  options.orientation = blend::Orientation::AlignToPath;
  std::vector<blend::Step> last;
  for ([[maybe_unused]] auto iteration : state) {
    last = blend::make(keys, options);
    benchmark::DoNotOptimize(last.data());
  }
  countSteps(state, last);
}
BENCHMARK(BM_BlendAlongSpine)
    ->RangeMultiplier(4)
    ->Range(4, 256)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

}  // namespace
