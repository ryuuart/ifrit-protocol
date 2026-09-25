/** @file
 * motion_schedule_bench — what a schedule costs a host per frame. Three
 * things are spent: BUILDING the schedule once for the counts this frame
 * has, READING one unit's local progress, which happens once per unit per
 * frame and is the number that decides whether a page of animated type is
 * free, and resolving a staggered value for one child among its siblings,
 * which is what a host pays per child when it lays a run out. Run a
 * Release build; Debug numbers say nothing.
 */

#include <benchmark/benchmark.h>
#include <sigilmotion/schedule/Schedule.h>
#include <sigilmotion/schedule/Stagger.h>

#include <chrono>

using namespace sigil::motion;
using namespace std::chrono_literals;

namespace {

Timing flat() { return Timing{.delay = stagger(30ms), .duration = 450ms}; }

Timing scattered() {
  return Timing{.delay = stagger(30ms, {.from = StaggerFrom::Random}),
                .duration = 450ms};
}

Timing nested() {
  return Timing{
      .delay = stagger(120ms), .duration = 180ms, .within = stagger(20ms)};
}

/** Resolving the timing against a frame's counts: the orderings are
 *  dealt here, so the scattered one pays a sort and the rest do not. */
void BM_ScheduleBuild(benchmark::State& state, Timing timing) {
  const auto count = (uint32_t)state.range(0);
  Schedule schedule;  // reused in place, as a host reuses it across frames
  for ([[maybe_unused]] auto iteration : state) {
    schedule.build(timing, count, 4);
    benchmark::DoNotOptimize(schedule.totalMs);
  }
  state.SetItemsProcessed(state.iterations());
}

/** One unit's local progress at a master progress — the per-unit read. */
void BM_LocalProgress(benchmark::State& state, Timing timing) {
  const auto count = (uint32_t)state.range(0);
  const Schedule schedule(timing, count, 4);
  float master = 0.0f;
  for ([[maybe_unused]] auto iteration : state) {
    master = master >= 1.0f ? 0.0f : master + 0.001f;
    for (uint32_t unit = 0; unit < count; ++unit)
      benchmark::DoNotOptimize(schedule.localProgress(master, unit, 0));
  }
  state.SetItemsProcessed(state.iterations() * count);
}

/** Every child's value out of one stagger — the run's steps dealt once
 *  and read by index until the count changes. */
void BM_StaggerAt(benchmark::State& state, Staggered<Duration> delay) {
  const auto count = (size_t)state.range(0);
  for ([[maybe_unused]] auto iteration : state)
    for (size_t index = 0; index < count; ++index)
      benchmark::DoNotOptimize(delay.at({index, count}));
  state.SetItemsProcessed(state.iterations() * (int64_t)count);
}

}  // namespace

BENCHMARK_CAPTURE(BM_ScheduleBuild, flat, flat())->Arg(32)->Arg(512);
BENCHMARK_CAPTURE(BM_ScheduleBuild, scattered, scattered())->Arg(32)->Arg(512);
BENCHMARK_CAPTURE(BM_ScheduleBuild, nested, nested())->Arg(32)->Arg(512);
BENCHMARK_CAPTURE(BM_LocalProgress, flat, flat())->Arg(32)->Arg(512);
BENCHMARK_CAPTURE(BM_LocalProgress, nested, nested())->Arg(32)->Arg(512);
BENCHMARK_CAPTURE(BM_StaggerAt, step, stagger(30ms))->Arg(32)->Arg(512);
BENCHMARK_CAPTURE(BM_StaggerAt, grid,
                  stagger(30ms, {.from = StaggerFrom::Center, .grid = {16, 32}}))
    ->Arg(512);
