/** @file
 * Benchmarks of the mesh currency: the parametric sheet every surface is
 * evaluated through, and the two whole-mesh rewrites — appending one mesh
 * to another and unwelding a primitive colour lane into vertices.
 */

#include <benchmark/benchmark.h>
#include <sigilgeometry/mesh/Mesh.h>

#include <cmath>
#include <numbers>
#include <vector>

using namespace sigil::geometry::mesh;

namespace {

void countVertices(benchmark::State& state, const Mesh& mesh) {
  state.counters["vertices"] = (double)mesh.positions.size();
  state.SetComplexityN((int64_t)mesh.positions.size());
}

/** The sheet seam, over the wave every named surface is a variation of. */
void BM_Grid(benchmark::State& state) {
  const int n = (int)state.range(0);
  Mesh last;
  for ([[maybe_unused]] auto iteration : state) {
    last = grid(n, n, [](float u, float v) {
      const float a = u * 2.0f * std::numbers::pi_v<float>;
      return glm::vec3{u * 100.0f, v * 100.0f, 10.0f * std::sin(a) * v};
    });
    benchmark::DoNotOptimize(last.positions.data());
  }
  countVertices(state, last);
}
BENCHMARK(BM_Grid)
    ->RangeMultiplier(4)
    ->Range(8, 256)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

Mesh sheet(int n) {
  return grid(n, n,
              [](float u, float v) { return glm::vec3{u, v, 0.0f}; });
}

/** Appending is how every consumer builds one mesh out of many, so its
 *  cost is per vertex of what arrives, not of what is there already. */
void BM_Append(benchmark::State& state) {
  const Mesh incoming = sheet((int)state.range(0));
  for ([[maybe_unused]] auto iteration : state) {
    Mesh out = incoming;
    out.append(incoming);
    benchmark::DoNotOptimize(out.positions.data());
  }
  state.SetComplexityN(state.range(0) * state.range(0));
}
BENCHMARK(BM_Append)
    ->RangeMultiplier(4)
    ->Range(8, 256)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

/** The bake unwelds every triangle, so it is the one operation whose
 *  output is three vertices per triangle whatever the input sharing was. */
void BM_BakePrimColor(benchmark::State& state) {
  Mesh source = sheet((int)state.range(0));
  std::vector<glm::vec4>& lane = source.prim("Color");
  lane.assign(source.triangleCount(), glm::vec4{1, 0, 0, 1});
  Mesh last;
  for ([[maybe_unused]] auto iteration : state) {
    last = bakePrimColor(source);
    benchmark::DoNotOptimize(last.positions.data());
  }
  countVertices(state, last);
}
BENCHMARK(BM_BakePrimColor)
    ->RangeMultiplier(4)
    ->Range(8, 256)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

}  // namespace
