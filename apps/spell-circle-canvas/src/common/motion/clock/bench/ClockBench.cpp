/** @file
 * motion_clock_bench — one frame of the clock and the ticker: the frame
 * clock's own arithmetic, the timeline step that is phase one, and the
 * derivation pass that is phase two. Run a Release build; Debug numbers
 * say nothing.
 */

#include <benchmark/benchmark.h>
#include <sigilmotion/clock/FrameClock.h>
#include <sigilmotion/clock/Ticker.h>

#include <memory>
#include <vector>

using namespace sigil::motion;
namespace ch = choreograph;

namespace {

/** One frame at a rate no display runs, so the arms below step often
 *  enough to be measured without any of them retiring. */
constexpr double kFrameSeconds = 1.0 / 240.0;

/** Longer than any arm here can ever step: a finished motion removes
 *  itself from the timeline, and the arm would then be measuring an empty
 *  ticker rather than the motions it registered. */
constexpr float kEndlessSeconds = 1.0e6f;

/** Cells are held by pointer because the timeline and every derivation
 *  keep the address they were registered with. */
using Cells = std::vector<std::unique_ptr<ch::Output<float>>>;

ch::Output<float>* grow(Cells& cells) {
  cells.push_back(std::make_unique<ch::Output<float>>(0.0f));
  return cells.back().get();
}

void countCells(benchmark::State& state, int64_t cells) {
  state.counters["cells/s"] = benchmark::Counter(
      (double)cells, benchmark::Counter::kIsIterationInvariantRate);
}

/** The clock alone: what every frame pays before anything animates. */
void BM_FrameClock_Tick(benchmark::State& state) {
  FrameClock clock;
  double now = 0.0;
  for ([[maybe_unused]] auto iteration : state) {
    now += kFrameSeconds;
    benchmark::DoNotOptimize(clock.tick(now));
  }
}
BENCHMARK(BM_FrameClock_Tick);

/** Phase one: the master timeline stepped with N motions on it. */
void BM_Tick_Timeline(benchmark::State& state) {
  const int64_t count = state.range(0);
  // The cells outlive the ticker: a motion disconnects from the cell it
  // writes when it is destroyed, so the cell has to still be there.
  Cells cells;
  cells.reserve((size_t)count);
  Ticker ticker;
  for (int64_t i = 0; i < count; ++i)
    ticker.timeline().apply(grow(cells)).then<ch::RampTo>(1.0f, kEndlessSeconds);

  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(ticker.tick(kFrameSeconds));
  countCells(state, count);
}
BENCHMARK(BM_Tick_Timeline)
    ->RangeMultiplier(8)
    ->Range(1, 512)
    ->Unit(benchmark::kMicrosecond);

/** Phase two: N derived cells recomputed from one source through a bound
 *  chain, the cost a host pays for reading a schedule instead of copying
 *  it by hand inside a steppable. */
void BM_Tick_Derivations(benchmark::State& state) {
  const int64_t count = state.range(0);
  ch::Output<float> source = 0.0f;
  Cells cells;
  cells.reserve((size_t)count);
  Ticker ticker;
  for (int64_t i = 0; i < count; ++i)
    ticker.derive(grow(cells), bind(&source).source(0, 10).target(-70, 170));

  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(ticker.tick(kFrameSeconds));
  countCells(state, count);
}
BENCHMARK(BM_Tick_Derivations)
    ->RangeMultiplier(8)
    ->Range(1, 512)
    ->Unit(benchmark::kMicrosecond);

}  // namespace

BENCHMARK_MAIN();
