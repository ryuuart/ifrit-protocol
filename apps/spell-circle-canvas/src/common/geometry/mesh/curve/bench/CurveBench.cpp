/** @file
 * Benchmarks of the spline features: arc-length sampling and
 * parallel-transport frames by count, the sweep by profile and
 * tessellation, and the ring seam a device executor replaces on its
 * own.
 */

// geometry_mesh_curve_bench — what evaluating a spline evenly costs, what
// carrying a frame along it adds, and how a sweep grows with the rings
// and the profile points it emits. Run a Release build; Debug numbers say
// nothing.

#include <benchmark/benchmark.h>
#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilgeometry/mesh/curve/Pose.h>
#include <sigilgeometry/mesh/curve/Sweep.h>

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

void BM_Sweep_Circle(benchmark::State& state) {
  const curve::Spline3 spline = knot(9);
  const path::Polyline profile = curve::profile::circle((int)state.range(1));
  const curve::SweepOptions options{.segments = (int)state.range(0),
                                    .scale = 6};
  Mesh last;
  for ([[maybe_unused]] auto iteration : state) {
    last = curve::sweep(spline, profile, options);
    benchmark::DoNotOptimize(last.positions.data());
  }
  countVertices(state, last);
}
BENCHMARK(BM_Sweep_Circle)
    ->ArgsProduct({{32, 256, 1024}, {6, 24}})
    ->ArgNames({"segments", "sides"})
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

/** THE RING SEAM ALONE: what a rail and a profile become as a dispatch,
 *  and the executor's own call over it, without the topology, the caps or
 *  the pour into a mesh's lanes. It is the half a device replaces, so a
 *  device executor's arm has a host number to stand beside. */
void BM_Sweep_Rings(benchmark::State& state) {
  const curve::Spline3 spline = knot(9);
  const path::Polyline profile = curve::profile::circle(24);
  const curve::SweepOptions options{.segments = (int)state.range(0),
                                    .scale = 6};
  const std::vector<curve::Frame3> rail =
      curve::frames(spline, options.segments, options.up);
  curve::kernel::Dispatch work;
  curve::describe(rail, profile, options, &work);
  std::vector<glm::vec4> positions(work.vertices());
  std::vector<glm::vec4> normals(work.vertices());
  for ([[maybe_unused]] auto iteration : state) {
    options.runtime->rings(work, positions.data(), normals.data());
    benchmark::DoNotOptimize(positions.data());
  }
  state.counters["vertices/s"] =
      benchmark::Counter((double)state.iterations() * (double)work.vertices(),
                         benchmark::Counter::kIsRate);
}
BENCHMARK(BM_Sweep_Rings)
    ->Arg(32)
    ->Arg(256)
    ->Arg(1024)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_Sweep_Line(benchmark::State& state) {
  const curve::Spline3 spline = knot(9);
  const path::Polyline profile = curve::profile::line();
  const curve::SweepOptions options{
      .segments = (int)state.range(0),
      .scale = 24,
      .normals = curve::SweepOptions::Normals::Frame};
  Mesh last;
  for ([[maybe_unused]] auto iteration : state) {
    last = curve::sweep(spline, profile, options);
    benchmark::DoNotOptimize(last.positions.data());
  }
  countVertices(state, last);
}
BENCHMARK(BM_Sweep_Line)
    ->RangeMultiplier(4)
    ->Range(32, 2048)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

// The hung rail, which walks the loop in parameter rather than by arc
// length and re-derives its own tangents: a different cost per section
// from the transported rail above.
void BM_Sweep_Hang(benchmark::State& state) {
  const curve::Spline3 spline = knot(9);
  const path::Polyline profile = curve::profile::line();
  const curve::SweepOptions options{
      .scale = 24, .normals = curve::SweepOptions::Normals::Frame};
  const int sections = (int)state.range(0);
  Mesh last;
  for ([[maybe_unused]] auto iteration : state) {
    last = curve::sweep(curve::hangFrames(spline, sections, 1, 0.4f), profile,
                        options);
    benchmark::DoNotOptimize(last.positions.data());
  }
  countVertices(state, last);
}
BENCHMARK(BM_Sweep_Hang)
    ->RangeMultiplier(4)
    ->Range(32, 2048)
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
