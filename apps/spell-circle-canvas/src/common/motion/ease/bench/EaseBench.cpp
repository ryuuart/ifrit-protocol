/** @file
 * motion_ease_bench — one curve per evaluation through the `Easing` a
 * slot holds: a plain function, a shaped curve, and the CSS Bézier whose
 * body is a bisection. Run a Release build; Debug numbers say nothing.
 */

#include <benchmark/benchmark.h>
#include <sigilmotion/ease/Ease.h>

#include <vector>

using namespace sigil::motion;

namespace {

const std::vector<float>& inputs() {
  static const std::vector<float> values = [] {
    std::vector<float> positions;
    positions.reserve(1024);
    for (int step = 0; step < 1024; ++step)
      positions.push_back((float)step / 1023.0f);
    return positions;
  }();
  return values;
}

void sweep(benchmark::State& state, const Easing& curve) {
  for ([[maybe_unused]] auto iteration : state) {
    float sink = 0;
    for (float position : inputs()) sink += curve(position);
    benchmark::DoNotOptimize(sink);
  }
  state.counters["calls/s"] = benchmark::Counter(
      (double)inputs().size(), benchmark::Counter::kIsIterationInvariantRate);
}

void BM_EasePlainFunction(benchmark::State& state) {
  sweep(state, ease::outQuint);
}
BENCHMARK(BM_EasePlainFunction);

void BM_EaseShapedCurve(benchmark::State& state) {
  sweep(state, ease::outElastic(1.0f, 0.4f));
}
BENCHMARK(BM_EaseShapedCurve);

void BM_EaseCubicBezier(benchmark::State& state) {
  sweep(state, ease::cubicBezier(0.25f, 0.1f, 0.25f, 1.0f));
}
BENCHMARK(BM_EaseCubicBezier);

}  // namespace
