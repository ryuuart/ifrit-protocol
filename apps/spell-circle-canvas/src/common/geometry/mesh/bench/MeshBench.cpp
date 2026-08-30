/** @file
 * Benchmarks of the mesh generators: extrude, revolve and the grid
 * presets by vertex count.
 */

// geometry_mesh_bench — the procedural generators by output size: how an
// extrusion's cost grows with the outline it lifts, and a lathe's with
// the profile and sweep it turns. Run a Release build; Debug numbers say
// nothing.

#include <benchmark/benchmark.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <sigilgeometry/mesh/Mesh.h>

#include <cmath>
#include <numbers>
#include <vector>

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;

namespace {

/** A closed ring of `segments` cubic arcs with a hole inside, so the caps
 *  triangulate with hole support and the walls sweep two contours. */
SkPath ringWithHole(int segments) {
  SkPathBuilder builder;
  for (float radius : {200.0f, 90.0f}) {
    const float step = 2.0f * std::numbers::pi_v<float> / (float)segments;
    auto point = [&](float a) {
      const float r = radius * (1.0f + 0.1f * std::sin(5.0f * a));
      return SkPoint{r * std::cos(a), r * std::sin(a)};
    };
    builder.moveTo(point(0));
    for (int i = 0; i < segments; ++i) {
      const float a0 = step * (float)i, a1 = step * (float)(i + 1);
      const SkPoint p0 = point(a0), p3 = point(a1);
      const float tangent = radius * step / 3.0f;
      builder.cubicTo(
          {p0.fX - tangent * std::sin(a0), p0.fY + tangent * std::cos(a0)},
          {p3.fX + tangent * std::sin(a1), p3.fY - tangent * std::cos(a1)}, p3);
    }
    builder.close();
  }
  builder.setFillType(SkPathFillType::kEvenOdd);
  return builder.detach();
}

std::vector<glm::vec2> vaseProfile(int points) {
  std::vector<glm::vec2> profile;
  for (int i = 0; i < points; ++i) {
    const float v = (float)i / (float)(points - 1);
    profile.emplace_back(60.0f + 30.0f * std::sin(v * 4.0f), v * 200.0f);
  }
  return profile;
}

void countVertices(benchmark::State& state, const Mesh& mesh) {
  state.counters["vertices/s"] =
      benchmark::Counter((double)mesh.vertexCount(),
                         benchmark::Counter::kIsIterationInvariantRate);
  state.counters["triangles"] = (double)mesh.triangleCount();
  state.SetComplexityN((int64_t)mesh.vertexCount());
}

void BM_Extrude(benchmark::State& state) {
  const SkPath outline = ringWithHole((int)state.range(0));
  Mesh last;
  for ([[maybe_unused]] auto iteration : state) {
    last = mesh::extrude(outline);
    benchmark::DoNotOptimize(last.positions.data());
  }
  countVertices(state, last);
}
BENCHMARK(BM_Extrude)
    ->RangeMultiplier(4)
    ->Range(8, 512)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_ExtrudeWallsOnly(benchmark::State& state) {
  const SkPath outline = ringWithHole((int)state.range(0));
  mesh::ExtrudeOptions options;
  options.frontCap = options.backCap = false;
  Mesh last;
  for ([[maybe_unused]] auto iteration : state) {
    last = mesh::extrude(outline, options);
    benchmark::DoNotOptimize(last.positions.data());
  }
  countVertices(state, last);
}
BENCHMARK(BM_ExtrudeWallsOnly)
    ->RangeMultiplier(4)
    ->Range(8, 512)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_Revolve(benchmark::State& state) {
  const std::vector<glm::vec2> profile = vaseProfile((int)state.range(0));
  mesh::RevolveOptions options;
  options.segments = (int)state.range(1);
  Mesh last;
  for ([[maybe_unused]] auto iteration : state) {
    last = mesh::revolve(profile, options);
    benchmark::DoNotOptimize(last.positions.data());
  }
  countVertices(state, last);
}
BENCHMARK(BM_Revolve)
    ->ArgsProduct({{8, 64, 512}, {16, 128}})
    ->ArgNames({"profile", "segments"})
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_Torus(benchmark::State& state) {
  const int n = (int)state.range(0);
  Mesh last;
  for ([[maybe_unused]] auto iteration : state) {
    last = mesh::torus(100.0f, 30.0f, n, n / 2);
    benchmark::DoNotOptimize(last.positions.data());
  }
  countVertices(state, last);
}
BENCHMARK(BM_Torus)
    ->RangeMultiplier(4)
    ->Range(16, 1024)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

}  // namespace

BENCHMARK_MAIN();
