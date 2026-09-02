/** @file
 * Benchmarks of the spline features: arc-length sampling and
 * parallel-transport frames by count, and the pose read by distance over
 * a held rail and over the spline that builds one.
 */

// geometry_mesh_curve_bench — what evaluating a spline evenly costs and
// what carrying a frame along it adds. Run a Release build; Debug numbers
// say nothing.

#include <benchmark/benchmark.h>
#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilgeometry/mesh/curve/Pose.h>

#include <cmath>
#include <glm/glm.hpp>
#include <numbers>
#include <vector>

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;

namespace {

/** A closed trefoil: curvature that turns in all three axes, so frames
 *  have inflections to carry through rather than a planar arc. */
curve::Spline3 knot(int knots) {
  curve::Spline3 spline;
  spline.closed = true;
  for (int i = 0; i < knots; ++i) {
    const float t = 2.0f * std::numbers::pi_v<float> * (float)i / (float)knots;
    spline.points.emplace_back((std::sin(t) + 2.0f * std::sin(2 * t)) * 60.0f,
                               (std::cos(t) - 2.0f * std::cos(2 * t)) * 60.0f,
                               -std::sin(3 * t) * 60.0f);
  }
  return spline;
}

void BM_SampleArcLength(benchmark::State& state) {
  const curve::Spline3 spline = knot(9);
  const int count = (int)state.range(0);
  for ([[maybe_unused]] auto iteration : state) {
    std::vector<glm::vec3> points = spline.sampleArcLength(count);
    benchmark::DoNotOptimize(points.data());
  }
  state.SetComplexityN(count);
}
BENCHMARK(BM_SampleArcLength)
    ->RangeMultiplier(4)
    ->Range(16, 4096)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_Frames(benchmark::State& state) {
  const curve::Spline3 spline = knot(9);
  const int count = (int)state.range(0);
  for ([[maybe_unused]] auto iteration : state) {
    std::vector<curve::Frame3> rail = curve::frames(spline, count);
    benchmark::DoNotOptimize(rail.data());
  }
  state.SetComplexityN(count);
}
BENCHMARK(BM_Frames)
    ->RangeMultiplier(4)
    ->Range(16, 4096)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

/** The read a camera flying a spline makes every frame, over a rail it
 *  built once. */
void BM_PoseAlong_Rail(benchmark::State& state) {
  const curve::Spline3 spline = knot(12);
  const std::vector<curve::Frame3> rail =
      curve::frames(spline, (int)state.range(0));
  const float total = spline.length();
  float u = 0;
  for ([[maybe_unused]] auto iteration : state) {
    u += 0.013f;
    if (u > 1.0f) u -= 1.0f;
    benchmark::DoNotOptimize(
        curve::poseAlong(rail, u * total, path::Wrap::Around).position);
  }
  state.counters["poses/s"] =
      benchmark::Counter(1, benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_PoseAlong_Rail)
    ->RangeMultiplier(4)
    ->Range(64, 1024)
    ->Unit(benchmark::kMicrosecond);

/** The same read over the spline itself, which builds the rail first —
 *  the one-line spelling, and the reason a caller reading every frame
 *  holds the rail instead. */
void BM_PoseAlong_Spline(benchmark::State& state) {
  const curve::Spline3 spline = knot(12);
  const float total = spline.length();
  float u = 0;
  for ([[maybe_unused]] auto iteration : state) {
    u += 0.013f;
    if (u > 1.0f) u -= 1.0f;
    benchmark::DoNotOptimize(curve::poseAlong(spline, u * total,
                                              path::Wrap::Around,
                                              (int)state.range(0))
                                 .position);
  }
  state.counters["poses/s"] =
      benchmark::Counter(1, benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_PoseAlong_Spline)
    ->RangeMultiplier(4)
    ->Range(64, 1024)
    ->Unit(benchmark::kMicrosecond);

}  // namespace

BENCHMARK_MAIN();
