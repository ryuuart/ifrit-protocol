/** @file
 * The uniform grid: building the index, the radius query and the k-nearest
 * walk, all at constant density so the shape of the curve is the
 * algorithm and not the crowding.
 */

#include <benchmark/benchmark.h>
#include <sigilcore/compute/Chance.h>
#include <sigilgeometry/path/Neighbours.h>

#include <cmath>
#include <glm/vec3.hpp>
#include <vector>

using namespace sigil::geometry::path;

namespace {

/** A cloud of `count` points in a box whose edge grows with the cube root
 *  of the count, so the DENSITY is constant and a query at a fixed radius
 *  sweeps the same number of neighbours whatever the size. That is what
 *  makes the build linear and the query flat across the range. */
std::vector<glm::vec3> box(int count) {
  const float edge = 100.0f * std::cbrt((float)count);
  sigil::core::chance::Stream stream = sigil::core::chance::Stream::pcg(5);
  std::vector<glm::vec3> points;
  points.reserve((size_t)count);
  for (int i = 0; i < count; ++i)
    points.push_back(
        {stream.range(0, edge), stream.range(0, edge), stream.range(0, edge)});
  return points;
}

/** BUILDING THE INDEX: two counting passes over the points. */
void BM_NeighboursBuild(benchmark::State& state) {
  const int count = (int)state.range(0);
  const std::vector<glm::vec3> points = box(count);
  for ([[maybe_unused]] auto iteration : state) {
    Neighbours index(points);
    benchmark::DoNotOptimize(index.size());
  }
  state.counters["points/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_NeighboursBuild)
    ->RangeMultiplier(10)
    ->Range(10000, 100000)
    ->Unit(benchmark::kMillisecond)
    ->Complexity(benchmark::oN);

/** ONE RADIUS QUERY, at a radius holding a handful of points. The whole
 *  claim of the grid is that this does not grow with the count. */
void BM_NeighboursWithin(benchmark::State& state) {
  const int count = (int)state.range(0);
  const std::vector<glm::vec3> points = box(count);
  const Neighbours index(points);
  std::vector<uint32_t> found;
  size_t at = 0;
  for ([[maybe_unused]] auto iteration : state) {
    index.within(points[at++ % points.size()], 200.0f, found);
    benchmark::DoNotOptimize(found.size());
  }
  state.counters["queries/s"] =
      benchmark::Counter(1, benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_NeighboursWithin)
    ->RangeMultiplier(10)
    ->Range(10000, 100000)
    ->Unit(benchmark::kNanosecond);

/** ONE K-NEAREST QUERY: the ring walk plus the partial sort. */
void BM_NeighboursNearestK(benchmark::State& state) {
  const int count = (int)state.range(0);
  const std::vector<glm::vec3> points = box(count);
  const Neighbours index(points);
  size_t at = 0;
  for ([[maybe_unused]] auto iteration : state) {
    benchmark::DoNotOptimize(
        index.nearest(points[at++ % points.size()], 8).size());
  }
  state.counters["queries/s"] =
      benchmark::Counter(1, benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_NeighboursNearestK)
    ->RangeMultiplier(10)
    ->Range(10000, 100000)
    ->Unit(benchmark::kNanosecond);

}  // namespace
