/** @file
 * The multigrid dualised, by the reach it is asked for: a pentagrid at
 * three radii, which is the cost a whole aperiodic field is built at, and
 * the three- and four-family rings beside it at one reach so the shape of
 * the growth in the family count is readable.
 */

#include <benchmark/benchmark.h>
#include <sigilgeometry/path/Lattice.h>

using sigil::geometry::path::multigrid;
using sigil::geometry::path::multigridRing;
using sigil::geometry::path::MultigridTiling;

static void MultigridPentagridByReach(benchmark::State& state) {
  const auto families = multigridRing(5, 0.2);
  const double radius = (double)state.range(0);
  for (auto _ : state) {
    MultigridTiling tiling = multigrid(families, {.radius = radius});
    benchmark::DoNotOptimize(tiling.rhombs.data());
    benchmark::ClobberMemory();
    state.counters["rhombs"] = (double)tiling.rhombs.size();
  }
}
BENCHMARK(MultigridPentagridByReach)->Arg(10)->Arg(20)->Arg(40);

static void MultigridByFamilyCount(benchmark::State& state) {
  const auto families = multigridRing((int)state.range(0), 0.2);
  for (auto _ : state) {
    MultigridTiling tiling = multigrid(families, {.radius = 20.0});
    benchmark::DoNotOptimize(tiling.rhombs.data());
    benchmark::ClobberMemory();
    state.counters["rhombs"] = (double)tiling.rhombs.size();
  }
}
BENCHMARK(MultigridByFamilyCount)->Arg(3)->Arg(4)->Arg(5)->Arg(7);
