/** @file
 * The triangulation, its dual and the outline at a tightness, over sheets
 * of constant density.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkRect.h>
#include <sigilgeometry/path/Hull.h>
#include <sigilgeometry/path/Scatter.h>
#include <sigilgeometry/path/Triangulate.h>

#include <cmath>
#include <glm/vec2.hpp>
#include <vector>

using namespace sigil::geometry::path;

namespace {

/** `count` points spread evenly over a square whose side grows with the
 *  square root of the count, so the density is constant and the arms
 *  measure the construction rather than the crowding. */
std::vector<glm::vec2> sheet(int count) {
  const float edge = 20.0f * std::sqrt((float)count);
  return sample(Region::of(SkRect::MakeWH(edge, edge)), uniform(count, 3));
}

/** THE TRIANGULATION. */
void BM_Delaunay(benchmark::State& state) {
  const int count = (int)state.range(0);
  const std::vector<glm::vec2> points = sheet(count);
  for ([[maybe_unused]] auto iteration : state) {
    Triangulation mesh = delaunay(points);
    benchmark::DoNotOptimize(mesh.triangles.data());
  }
  state.counters["points/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_Delaunay)
    ->RangeMultiplier(10)
    ->Range(1000, 100000)
    ->Unit(benchmark::kMillisecond)
    ->Complexity(benchmark::oNLogN);

/** THE DUAL, over a triangulation already built: one polygon clipped
 *  once per neighbour. */
void BM_Voronoi(benchmark::State& state) {
  const int count = (int)state.range(0);
  const std::vector<glm::vec2> points = sheet(count);
  const Triangulation mesh = delaunay(points);
  const SkRect box = SkRect::MakeWH(20.0f * std::sqrt((float)count),
                                    20.0f * std::sqrt((float)count));
  for ([[maybe_unused]] auto iteration : state) {
    std::vector<Polyline> cells = voronoi(mesh, box);
    benchmark::DoNotOptimize(cells.data());
  }
  state.counters["cells/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_Voronoi)
    ->RangeMultiplier(10)
    ->Range(1000, 100000)
    ->Unit(benchmark::kMillisecond)
    ->Complexity(benchmark::oN);

/** THE CONVEX HULL: a sort and two passes, and no triangulation at all. */
void BM_HullConvex(benchmark::State& state) {
  const int count = (int)state.range(0);
  const std::vector<glm::vec2> points = sheet(count);
  for ([[maybe_unused]] auto iteration : state) {
    std::vector<Polyline> rings = hull(points);
    benchmark::DoNotOptimize(rings.data());
  }
  state.counters["points/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_HullConvex)
    ->RangeMultiplier(10)
    ->Range(1000, 100000)
    ->Unit(benchmark::kMillisecond)
    ->Complexity(benchmark::oNLogN);

/** THE ALPHA SHAPE, which is the triangulation plus the stitch: what a
 *  bound costs over no bound at all. */
void BM_HullAlpha(benchmark::State& state) {
  const int count = (int)state.range(0);
  const std::vector<glm::vec2> points = sheet(count);
  for ([[maybe_unused]] auto iteration : state) {
    std::vector<Polyline> rings = hull(points, 40.0f);
    benchmark::DoNotOptimize(rings.data());
  }
  state.counters["points/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_HullAlpha)
    ->RangeMultiplier(10)
    ->Range(1000, 100000)
    ->Unit(benchmark::kMillisecond)
    ->Complexity(benchmark::oNLogN);

}  // namespace
