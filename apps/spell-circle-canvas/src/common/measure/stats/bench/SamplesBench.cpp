// SigilMeasure benchmarks: the sample ring's add and its percentile
// at the sample counts a HUD and a headless sweep use.

#include <benchmark/benchmark.h>
#include <sigilmeasure/stats/Samples.h>

using sigil::measure::Samples;

static Samples filled(size_t n) {
  Samples w(n);
  for (size_t i = 0; i < n; ++i) w.add((double)((i * 7919) % 1000) / 10.0);
  return w;
}

static void BM_SamplesAdd(benchmark::State& state) {
  Samples w((size_t)state.range(0));
  double v = 0.0;
  for ([[maybe_unused]] auto _ : state) {
    w.add(v);
    v += 0.25;
    benchmark::DoNotOptimize(w);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SamplesAdd)->Arg(60)->Arg(120)->Arg(1000);

static void BM_SamplesPercentile(benchmark::State& state) {
  const Samples w = filled((size_t)state.range(0));
  for ([[maybe_unused]] auto _ : state)
    benchmark::DoNotOptimize(w.percentile(0.99));
}
BENCHMARK(BM_SamplesPercentile)->Arg(60)->Arg(120)->Arg(1000);

static void BM_SamplesMean(benchmark::State& state) {
  const Samples w = filled((size_t)state.range(0));
  for ([[maybe_unused]] auto _ : state) benchmark::DoNotOptimize(w.mean());
}
BENCHMARK(BM_SamplesMean)->Arg(60)->Arg(120)->Arg(1000);
