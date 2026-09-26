// SigilMeasure benchmarks: the live-stream instruments at the sizes a HUD
// and a headless sweep use — a window's add, its tail and its mean, and
// the per-value cost of a smoothed reading and a rate.

#include <benchmark/benchmark.h>
#include <sigilmeasure/stats/Rate.h>
#include <sigilmeasure/stats/Smoothed.h>
#include <sigilmeasure/stats/Window.h>

#include <chrono>

using namespace sigil::measure;

static Window<> filled(size_t count) {
  Window<> window(count);
  for (size_t index = 0; index < count; ++index)
    window.add((double)((index * 7919) % 1000) / 10.0);
  return window;
}

static void BM_WindowAdd(benchmark::State& state) {
  Window<> window((size_t)state.range(0));
  double value = 0.0;
  for ([[maybe_unused]] auto iteration : state) {
    window.add(value);
    value += 0.25;
    benchmark::DoNotOptimize(window);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_WindowAdd)->Arg(60)->Arg(120)->Arg(1000);

/** A window over time pays for its stamps and the trim at the front. */
static void BM_WindowAddOverSpan(benchmark::State& state) {
  Window<> window({.span = std::chrono::seconds(4)});
  double second = 0.0;
  for ([[maybe_unused]] auto iteration : state) {
    window.add(second, Duration(second));
    second += 1.0 / 120.0;
    benchmark::DoNotOptimize(window);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_WindowAddOverSpan);

static void BM_WindowQuantile(benchmark::State& state) {
  const Window<> window = filled((size_t)state.range(0));
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(window.quantile(0.99));
}
BENCHMARK(BM_WindowQuantile)->Arg(60)->Arg(120)->Arg(1000);

static void BM_WindowMean(benchmark::State& state) {
  const Window<> window = filled((size_t)state.range(0));
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(window.mean());
}
BENCHMARK(BM_WindowMean)->Arg(60)->Arg(120)->Arg(1000);

static void BM_SmoothedAdd(benchmark::State& state) {
  Smoothed level{12};
  double value = 0.0;
  for ([[maybe_unused]] auto iteration : state) {
    level.add(value);
    value += 0.25;
    benchmark::DoNotOptimize(level);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SmoothedAdd);

static void BM_RateMark(benchmark::State& state) {
  Rate rate{std::chrono::seconds(1)};
  double second = 0.0;
  for ([[maybe_unused]] auto iteration : state) {
    rate.mark(Duration(second));
    second += 1.0 / 1000.0;
    benchmark::DoNotOptimize(rate.perSecond());
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RateMark);
