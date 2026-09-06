// The statistics a study is summarised with, timed at the run lengths
// they are spent on: a frame ring of a hundred-odd, a sweep of a
// thousand, a point cloud of a hundred thousand. Reported per value
// where a pass is what costs, so an accumulation that grew a term shows
// up as a rate that fell.

#include <benchmark/benchmark.h>
#include <sigilmeasure/stats/Histogram.h>
#include <sigilmeasure/stats/Moments.h>
#include <sigilmeasure/stats/Rescale.h>
#include <sigilmeasure/stats/Samples.h>

#include <cstddef>
#include <vector>

using namespace sigil::measure;

namespace {

std::vector<double> run(size_t n) {
  std::vector<double> values;
  values.reserve(n);
  for (size_t i = 0; i < n; ++i)
    values.push_back((double)((i * 7919) % 10007) / 97.0);
  return values;
}

/** One value at a time is how a running summary is actually fed, so the
 *  arm that matters is the per-add cost and not the per-run one. */
void BM_MomentsAdd(benchmark::State& state) {
  Moments moments;
  double value = 0.0;
  for ([[maybe_unused]] auto iteration : state) {
    moments.add(value);
    value += 0.25;
    benchmark::DoNotOptimize(moments);
  }
  state.counters["values/s"] =
      benchmark::Counter(1, benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_MomentsAdd);

void BM_MomentsOf(benchmark::State& state) {
  const std::vector<double> values = run((size_t)state.range(0));
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(Moments::of(values).variance());
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_MomentsOf)->Arg(120)->Arg(1000)->Arg(100000);

void BM_HistogramAdd(benchmark::State& state) {
  Histogram histogram(0.0, 100.0, (size_t)state.range(0));
  double value = 0.0;
  for ([[maybe_unused]] auto iteration : state) {
    histogram.add(value);
    value = value < 100.0 ? value + 0.125 : 0.0;
    benchmark::DoNotOptimize(histogram);
  }
  state.counters["values/s"] =
      benchmark::Counter(1, benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_HistogramAdd)->Arg(16)->Arg(256);

/** A histogram over a run in hand pays for the pass that finds its ends
 *  as well as the pass that bins, which is the price of not having to
 *  choose a range. */
void BM_HistogramOver(benchmark::State& state) {
  const std::vector<double> values = run((size_t)state.range(0));
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(Histogram::over(values, 64).total());
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_HistogramOver)->Arg(1000)->Arg(100000);

/** The comparison the two arms exist for: several quantiles cost one
 *  sort here and one sort each through the single-fraction call, so the
 *  gap should widen with the number of fractions asked for. */
void BM_QuantileOneAtATime(benchmark::State& state) {
  const std::vector<double> values = run((size_t)state.range(0));
  const std::vector<double> fractions = {0.5, 0.9, 0.99};
  for ([[maybe_unused]] auto iteration : state)
    for (double p : fractions) benchmark::DoNotOptimize(quantile(values, p));
  state.SetItemsProcessed(state.iterations() * state.range(0) * 3);
}
BENCHMARK(BM_QuantileOneAtATime)->Arg(120)->Arg(10000);

void BM_QuantilesOneSort(benchmark::State& state) {
  const std::vector<double> values = run((size_t)state.range(0));
  const std::vector<double> fractions = {0.5, 0.9, 0.99};
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(quantiles(values, fractions));
  state.SetItemsProcessed(state.iterations() * state.range(0) * 3);
}
BENCHMARK(BM_QuantilesOneSort)->Arg(120)->Arg(10000);

void BM_ZScore(benchmark::State& state) {
  const std::vector<double> values = run((size_t)state.range(0));
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(zScore(values)(1.0));
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_ZScore)->Arg(1000)->Arg(100000);

}  // namespace
