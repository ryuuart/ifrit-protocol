// SigilData scale benchmarks: one mapping per call on each transform a
// per-mark loop runs through, and the tick ladder a redraw rebuilds.

#include <benchmark/benchmark.h>
#include <sigildata/scale/Scale.h>

using sigil::data::Scale;
using sigil::data::Transform;

static void mapping(benchmark::State& state, Scale scale) {
  double v = 1.0;
  for ([[maybe_unused]] auto _ : state) {
    benchmark::DoNotOptimize(scale(v));
    v = v < 999.0 ? v + 1.0 : 1.0;
  }
  state.SetItemsProcessed(state.iterations());
}

static void BM_ScaleLinear(benchmark::State& state) {
  mapping(state, Scale{.domain = {0, 1000}, .range = {40, 760}});
}
BENCHMARK(BM_ScaleLinear);

static void BM_ScaleLog(benchmark::State& state) {
  mapping(state, Scale{.domain = {1, 1000},
                       .range = {40, 760},
                       .transform = Transform::Log});
}
BENCHMARK(BM_ScaleLog);

static void BM_ScaleSqrt(benchmark::State& state) {
  mapping(state, Scale{.domain = {0, 1000},
                       .range = {0, 90},
                       .transform = Transform::Sqrt});
}
BENCHMARK(BM_ScaleSqrt);

static void BM_ScaleBand(benchmark::State& state) {
  mapping(state, Scale{.range = {0, 1000},
                       .transform = Transform::Band,
                       .steps = 64,
                       .padding = 0.1});
}
BENCHMARK(BM_ScaleBand);

static void BM_ScaleTicks(benchmark::State& state) {
  const Scale x{.domain = {2.3, 17.6}};
  for ([[maybe_unused]] auto _ : state)
    benchmark::DoNotOptimize(x.ticks((int)state.range(0)));
}
BENCHMARK(BM_ScaleTicks)->Arg(5)->Arg(10)->Arg(100);
