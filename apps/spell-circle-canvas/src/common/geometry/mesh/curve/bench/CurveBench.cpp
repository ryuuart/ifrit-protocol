/** @file
 * Benchmarks of the spline features: arc-length sampling and
 * parallel-transport frames by count, and the swept generators — tube,
 * ribbon and banner — by tessellation.
 */

// geometry_mesh_curve_bench — what evaluating a spline evenly costs, what
// carrying a frame along it adds, and how the three sweeps grow with the
// rings and sides they emit. Run a Release build; Debug numbers say
// nothing.

#include <benchmark/benchmark.h>
#include <sigilgeometry/mesh/curve/Curve.h>

#include <cmath>
#include <numbers>
#include <vector>

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

void countVertices(benchmark::State& state, const Mesh& m) {
  state.counters["vertices/s"] = benchmark::Counter(
      (double)m.vertexCount(), benchmark::Counter::kIsIterationInvariantRate);
  state.counters["triangles"] = (double)m.triangleCount();
  state.SetComplexityN((int64_t)m.vertexCount());
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

void BM_Tube(benchmark::State& state) {
  const curve::Spline3 spline = knot(9);
  curve::TubeOptions options;
  options.segments = (int)state.range(0);
  options.sides = (int)state.range(1);
  Mesh last;
  for ([[maybe_unused]] auto iteration : state) {
    last = curve::tube(spline, options);
    benchmark::DoNotOptimize(last.positions.data());
  }
  countVertices(state, last);
}
BENCHMARK(BM_Tube)
    ->ArgsProduct({{32, 256, 1024}, {6, 24}})
    ->ArgNames({"segments", "sides"})
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_Ribbon(benchmark::State& state) {
  const curve::Spline3 spline = knot(9);
  curve::RibbonOptions options;
  options.segments = (int)state.range(0);
  Mesh last;
  for ([[maybe_unused]] auto iteration : state) {
    last = curve::ribbon(spline, options);
    benchmark::DoNotOptimize(last.positions.data());
  }
  countVertices(state, last);
}
BENCHMARK(BM_Ribbon)
    ->RangeMultiplier(4)
    ->Range(32, 2048)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_Banner(benchmark::State& state) {
  const curve::Spline3 spline = knot(9);
  curve::BannerOptions options;
  options.sections = (int)state.range(0);
  options.span = 0.4f;
  Mesh last;
  for ([[maybe_unused]] auto iteration : state) {
    last = curve::banner(spline, options);
    benchmark::DoNotOptimize(last.positions.data());
  }
  countVertices(state, last);
}
BENCHMARK(BM_Banner)
    ->RangeMultiplier(4)
    ->Range(32, 2048)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

}  // namespace

BENCHMARK_MAIN();
