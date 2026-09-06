/** @file
 * The field timed one sample at a time, because that is how it is spent:
 * one read per displaced vertex, per grain pixel, per step of a traced
 * streamline. Reported as a rate per call, so a body that grows a step
 * shows up as a rate that fell.
 *
 * The arms are laid out so the choices a caller actually makes can be
 * priced: which kind, how many octaves, whether it tiles, and whether it
 * warps — a warp costs one extra evaluation per axis, which is the one
 * prop here that changes the cost by a multiple rather than a fraction.
 */

#include <benchmark/benchmark.h>
#include <sigilcore/compute/Field.h>

using namespace sigil::core;
using noise::Field;
using noise::FieldKind;
using noise::Fold;

namespace {

benchmark::Counter perCall() {
  return benchmark::Counter(1, benchmark::Counter::kIsIterationInvariantRate);
}

/** One walk over a run of points that lands on no lattice point, so the
 *  interpolation is paid for on every sample. */
void sampleField(benchmark::State& state, const Field& field) {
  float x = 0.0f, sink = 0.0f;
  for ([[maybe_unused]] auto iteration : state) {
    x += 0.0173f;
    sink += field.at(x, x * 0.61f, x * 0.29f);
    benchmark::DoNotOptimize(sink);
  }
  state.counters["samples/s"] = perCall();
}

void BM_FieldValue(benchmark::State& state) {
  sampleField(state, {.kind = FieldKind::Value, .seed = 1, .dimension = 2});
}
BENCHMARK(BM_FieldValue);

void BM_FieldGradient(benchmark::State& state) {
  sampleField(state, {.kind = FieldKind::Gradient, .seed = 1, .dimension = 2});
}
BENCHMARK(BM_FieldGradient);

void BM_FieldSimplex(benchmark::State& state) {
  sampleField(state, {.kind = FieldKind::Simplex, .seed = 1, .dimension = 2});
}
BENCHMARK(BM_FieldSimplex);

/** Cellular noise reads nine cells in two dimensions and twenty-seven in
 *  three, three hashes each, so it is the one kind whose cost the
 *  dimension changes by a multiple. */
void BM_FieldWorley(benchmark::State& state) {
  sampleField(state, {.kind = FieldKind::Worley, .seed = 1, .dimension = 2});
}
BENCHMARK(BM_FieldWorley);

void BM_FieldValueSolid(benchmark::State& state) {
  sampleField(state, {.kind = FieldKind::Value, .seed = 1, .dimension = 3});
}
BENCHMARK(BM_FieldValueSolid);

void BM_FieldWorleySolid(benchmark::State& state) {
  sampleField(state, {.kind = FieldKind::Worley, .seed = 1, .dimension = 3});
}
BENCHMARK(BM_FieldWorleySolid);

/** Octaves are the linear term: the sum costs what one octave costs,
 *  times the count. */
void BM_FieldOctaves(benchmark::State& state) {
  sampleField(state, {.kind = FieldKind::Gradient,
                      .seed = 1,
                      .dimension = 2,
                      .octaves = (int)state.range(0)});
}
BENCHMARK(BM_FieldOctaves)->Arg(1)->Arg(2)->Arg(4)->Arg(8);

/** A period is a modulo per lattice coordinate — the cheapest prop here,
 *  and the arm exists to show that it is. */
void BM_FieldTiled(benchmark::State& state) {
  sampleField(state, {.kind = FieldKind::Gradient,
                      .seed = 1,
                      .dimension = 2,
                      .octaves = 4,
                      .period = 16});
}
BENCHMARK(BM_FieldTiled);

void BM_FieldWarped(benchmark::State& state) {
  sampleField(state, {.kind = FieldKind::Gradient,
                      .seed = 1,
                      .dimension = 2,
                      .octaves = 4,
                      .warp = 0.5f});
}
BENCHMARK(BM_FieldWarped);

void BM_FieldRidged(benchmark::State& state) {
  sampleField(state, {.kind = FieldKind::Gradient,
                      .seed = 1,
                      .dimension = 2,
                      .octaves = 4,
                      .fold = Fold::Ridged});
}
BENCHMARK(BM_FieldRidged);

}  // namespace
