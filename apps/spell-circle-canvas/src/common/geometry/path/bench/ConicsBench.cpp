/** @file
 * The conic by the number of steps a span is sampled at, and the two
 * costs beside that sampling: one point read on its own, and the
 * direction of travel there, which is the read a gizmo hung off a curve
 * makes every frame.
 */

#include <benchmark/benchmark.h>
#include <sigilgeometry/path/Conic.h>

#include <cmath>

using sigil::geometry::path::Conic;
using sigil::geometry::path::conicPath;
using sigil::geometry::path::ConicSpan;

namespace {

constexpr Conic kOrbit{
    .focus = {500, 330}, .semiLatus = 248.5f, .eccentricity = 0.42f};
constexpr Conic kEscape{
    .focus = {500, 330}, .semiLatus = 430.0f, .eccentricity = 1.55f};

void BM_ConicPath(benchmark::State& state) {
  const int steps = (int)state.range(0);
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(
        conicPath(kOrbit, {.fromDeg = 0, .toDeg = 360, .steps = steps})
            .countPoints());
  state.counters["points/s"] = benchmark::Counter(
      (double)steps + 1, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(steps);
}
BENCHMARK(BM_ConicPath)
    ->RangeMultiplier(4)
    ->Range(16, 4096)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

/** The same span on an open branch held to a reach: every step past it is
 *  dropped, so what this measures against the closed conic above is what
 *  the bound costs when most of the sweep is outside it. */
void BM_ConicPathHeldToReach(benchmark::State& state) {
  const int steps = (int)state.range(0);
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(
        conicPath(kEscape,
                  {.fromDeg = -180, .toDeg = 180, .steps = steps, .reach = 700})
            .countPoints());
  state.SetComplexityN(steps);
}
BENCHMARK(BM_ConicPathHeldToReach)
    ->RangeMultiplier(4)
    ->Range(16, 4096)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_ConicPoint(benchmark::State& state) {
  float anomaly = 0;
  for ([[maybe_unused]] auto iteration : state) {
    anomaly = std::fmod(anomaly + 0.37f, 360.0f);
    benchmark::DoNotOptimize(kOrbit.at(anomaly).x);
  }
  state.counters["points/s"] =
      benchmark::Counter(1, benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_ConicPoint);

void BM_ConicAlong(benchmark::State& state) {
  float anomaly = 0;
  for ([[maybe_unused]] auto iteration : state) {
    anomaly = std::fmod(anomaly + 0.37f, 360.0f);
    benchmark::DoNotOptimize(kOrbit.alongAt(anomaly).x);
  }
  state.counters["directions/s"] =
      benchmark::Counter(1, benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_ConicAlong);

}  // namespace
