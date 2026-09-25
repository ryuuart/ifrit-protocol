/** @file
 * motion_clock_bench — one frame of the engine: the frame's own
 * arithmetic with nothing on it, N animations stepped, N timers called,
 * a step-rate timer catching up, and the read of N values bound to one
 * moving source, which is where a derived value's cost lands: it is
 * read, never stepped. Run a Release build; Debug numbers say
 * nothing.
 */

#include <benchmark/benchmark.h>
#include <sigilmotion/bind/Binding.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilmotion/values/Animatable.h>

#include <chrono>
#include <vector>

using namespace sigil::motion;
using namespace std::chrono_literals;

namespace {

/** One frame at a rate no display runs, so the arms below step often
 *  enough to be measured without any of them retiring. */
constexpr Duration kFrame = std::chrono::duration<double>(1.0 / 240.0);

/** Longer than any arm here can ever step: a finished motion leaves the
 *  engine, and the arm would then be measuring an empty engine rather
 *  than the motions it started. */
constexpr Duration kEndless = std::chrono::duration<double>(1.0e6);

void countValues(benchmark::State& state, int64_t values) {
  state.counters["values/s"] = benchmark::Counter(
      (double)values, benchmark::Counter::kIsIterationInvariantRate);
}

/** The engine alone: what every frame pays before anything animates. */
void BM_Engine_Frame(benchmark::State& state) {
  Engine engine;
  engine.setPolicy(ClockPolicy::Advance);
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(engine.advance(engine.elapsed() + kFrame));
}
BENCHMARK(BM_Engine_Frame);

/** A wall frame: the reading, the clamp and the speed. */
void BM_Engine_WallFrame(benchmark::State& state) {
  Engine engine;
  double now = 0.0;
  for ([[maybe_unused]] auto iteration : state) {
    now += kFrame.count();
    benchmark::DoNotOptimize(engine.advanceWall(now));
  }
}
BENCHMARK(BM_Engine_WallFrame);

/** N animations, each on its own live value, stepped one frame. */
void BM_Engine_Animations(benchmark::State& state) {
  const int64_t count = state.range(0);
  std::vector<Animatable<float>> values;
  values.reserve((size_t)count);
  Engine engine;
  for (int64_t index = 0; index < count; ++index) {
    values.push_back(animatable(0.0f));
    engine.animate(values.back(), {.to = 1.0f, .duration = kEndless});
  }
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(engine.advance(engine.elapsed() + kFrame));
  countValues(state, count);
}
BENCHMARK(BM_Engine_Animations)
    ->RangeMultiplier(8)
    ->Range(1, 512)
    ->Unit(benchmark::kMicrosecond);

/** N timers called once a frame. */
void BM_Engine_Timers(benchmark::State& state) {
  const int64_t count = state.range(0);
  Engine engine;
  float sink = 0.0f;
  for (int64_t index = 0; index < count; ++index)
    engine.timer([&sink](Duration delta) { sink += (float)delta.count(); });
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(engine.advance(engine.elapsed() + kFrame));
  benchmark::DoNotOptimize(sink);
  countValues(state, count);
}
BENCHMARK(BM_Engine_Timers)
    ->RangeMultiplier(8)
    ->Range(1, 512)
    ->Unit(benchmark::kMicrosecond);

/** A step-rate timer at 240 a second drawn at 60: four fixed steps a
 *  frame, counted from total time. */
void BM_Engine_StepRate(benchmark::State& state) {
  Engine engine;
  int steps = 0;
  engine.timer([&steps] { ++steps; }, {.stepRate = 240.0});
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(engine.advance(engine.elapsed() + 1s / 60.0));
  benchmark::DoNotOptimize(steps);
}
BENCHMARK(BM_Engine_StepRate);

/** N values bound to one animated source, read after a frame through a
 *  binding with a range on each side — the cost a host pays for deriving
 *  a value instead of copying it by hand inside a timer. */
void BM_Engine_BoundReads(benchmark::State& state) {
  const int64_t count = state.range(0);
  Engine engine;
  Animatable<float> source = animatable(0.0f);
  engine.animate(source, {.to = 10.0f, .duration = kEndless});
  std::vector<Animatable<float>> bound;
  bound.reserve((size_t)count);
  for (int64_t index = 0; index < count; ++index)
    bound.push_back(bind(source, {.from = {0.0f, 10.0f}, .to = {-70.0f, 170.0f}}));
  for ([[maybe_unused]] auto iteration : state) {
    engine.advance(engine.elapsed() + kFrame);
    float sink = 0.0f;
    for (const Animatable<float>& value : bound) sink += value.value();
    benchmark::DoNotOptimize(sink);
  }
  countValues(state, count);
}
BENCHMARK(BM_Engine_BoundReads)
    ->RangeMultiplier(8)
    ->Range(1, 512)
    ->Unit(benchmark::kMicrosecond);

}  // namespace
