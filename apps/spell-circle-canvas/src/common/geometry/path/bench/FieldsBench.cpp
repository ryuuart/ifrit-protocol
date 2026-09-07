/** @file
 * Walking, repeating and stepping a field: the streamline, the symmetry
 * copies and the cell sheet.
 */

#include <benchmark/benchmark.h>
#include <sigilcore/compute/Noise.h>
#include <sigilgeometry/path/Cells.h>
#include <sigilgeometry/path/Symmetry.h>
#include <sigilgeometry/path/Trace.h>

#include <cmath>
#include <numbers>
#include <vector>

#include "Figures.h"

using namespace sigil::geometry::path;
using sigil::geometry::path::figures::rippledRing;

namespace {

/** THE STREAMLINE: four field reads per step, so this arm is really what
 *  the field costs, which is the honest thing to measure — the walk
 *  itself is four adds. */
void BM_Streamline(benchmark::State& state) {
  const int steps = (int)state.range(0);
  sigil::core::noise::Field field;
  field.frequency = 0.004f;
  field.octaves = 3;
  const VectorField angled = flow(field, Flow::Angle, 2.0f);
  TraceOptions options;
  options.step = 2.0f;
  options.length = 2.0f * (float)steps;
  float at = 0;
  for ([[maybe_unused]] auto iteration : state) {
    at += 7.0f;
    Polyline line = streamline(angled, {at, at * 0.5f}, options);
    benchmark::DoNotOptimize(line.points.data());
  }
  state.counters["steps/s"] =
      benchmark::Counter(steps, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(steps);
}
BENCHMARK(BM_Streamline)
    ->RangeMultiplier(8)
    ->Range(64, 4096)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

/** THE COPIES A SYMMETRY MAKES, over a path: the matrices, and the
 *  concatenation of the figure under each of them. */
void BM_SymmetryCopies(benchmark::State& state) {
  const int order = (int)state.range(0);
  Symmetry symmetry;
  symmetry.order = order;
  symmetry.mirror = true;
  const SkPath figure = rippledRing(32, 120.0f);
  for ([[maybe_unused]] auto iteration : state) {
    SkPath all = copies(symmetry, figure);
    benchmark::DoNotOptimize(all.countPoints());
  }
  state.counters["copies/s"] = benchmark::Counter(
      order * 2, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(order);
}
BENCHMARK(BM_SymmetryCopies)
    ->RangeMultiplier(4)
    ->Range(2, 128)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

/** ONE STEP OF A CELL SHEET, with a nine-cell rule: what the substrate
 *  costs a reaction-diffusion or an automaton per cell per frame. */
void BM_CellsStep(benchmark::State& state) {
  const int edge = (int)state.range(0);
  Cells<float> sheet(edge, edge, 0.0f);
  sheet.setBoundary(Boundary::Wrap);
  for (int i = 0; i < edge; ++i) sheet.at(i, i) = 1.0f;
  for ([[maybe_unused]] auto iteration : state) {
    sheet.step([](const Cells<float>& from, int x, int y) {
      float total = 0;
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) total += from.read(x + dx, y + dy);
      return total * (1.0f / 9.0f);
    });
  }
  const auto cells = (int64_t)edge * (int64_t)edge;
  state.counters["cells/s"] = benchmark::Counter(
      (double)cells, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(cells);
}
BENCHMARK(BM_CellsStep)
    ->RangeMultiplier(4)
    ->Range(64, 1024)
    ->Unit(benchmark::kMillisecond)
    ->Complexity(benchmark::oN);

}  // namespace
