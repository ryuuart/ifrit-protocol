/** @file
 * motion_schedule_bench — what a schedule costs a host per frame. Two
 * things are spent: BUILDING the cascade once for the counts this frame
 * has, and READING one unit's local time, which happens once per unit
 * per frame and is the number that decides whether a page of animated
 * type is free. Run a Release build; Debug numbers say nothing.
 */

#include <benchmark/benchmark.h>
#include <sigilmotion/schedule/Schedule.h>

using namespace sigil::motion;

namespace {

Spread flat() { return Spread{.eachMs = 30, .durationMs = 450}; }

Spread scattered() {
  Spread s = flat();
  s.from = Spread::From::Random;
  return s;
}

Spread nested() {
  Spread s{.eachMs = 120, .durationMs = 400};
  s.then(Spread{.eachMs = 20, .durationMs = 180});
  return s;
}

/** Resolving the spread against a frame's counts: the orderings are
 *  dealt here, so the scattered one pays a sort and the rest do not. */
void BM_CascadeBuild(benchmark::State& state, Spread spec) {
  const auto count = (uint32_t)state.range(0);
  Cascade cascade;  // reused in place, as a host reuses it across frames
  for (auto _ : state) {
    cascade.build(spec, count, 4);
    benchmark::DoNotOptimize(cascade.totalMs);
  }
  state.SetItemsProcessed(state.iterations());
}

/** One unit's local time at a master progress — the per-unit read. */
void BM_LocalTime(benchmark::State& state, Spread spec) {
  const auto count = (uint32_t)state.range(0);
  Cascade cascade;
  cascade.build(spec, count, 4);
  float master = 0.0f;
  for (auto _ : state) {
    master = master >= 1.0f ? 0.0f : master + 0.001f;
    for (uint32_t i = 0; i < count; ++i)
      benchmark::DoNotOptimize(cascade.localTime(master, i, 0));
  }
  state.SetItemsProcessed(state.iterations() * count);
}

}  // namespace

BENCHMARK_CAPTURE(BM_CascadeBuild, flat, flat())->Arg(32)->Arg(512);
BENCHMARK_CAPTURE(BM_CascadeBuild, scattered, scattered())->Arg(32)->Arg(512);
BENCHMARK_CAPTURE(BM_CascadeBuild, nested, nested())->Arg(32)->Arg(512);
BENCHMARK_CAPTURE(BM_LocalTime, flat, flat())->Arg(32)->Arg(512);
BENCHMARK_CAPTURE(BM_LocalTime, nested, nested())->Arg(32)->Arg(512);

BENCHMARK_MAIN();
