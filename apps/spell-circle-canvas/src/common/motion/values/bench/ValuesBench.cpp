/** @file
 * motion_values_bench — the Animatable slot per lane: what a consumer
 * pays to read a property that may be a constant, a described motion, a
 * live value or a live value shaped through a binding, and what copying a
 * lane of slots costs, since every form but the constant lives behind an
 * allocation. The read here is the one a consumer writes: ask which form
 * the slot holds, then evaluate that form — a described motion read at a
 * time with no engine. Run a Release build; Debug numbers say nothing.
 */

#include <benchmark/benchmark.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Oscillator.h>
#include <sigilmotion/values/Tween.h>

#include <chrono>
#include <vector>

using namespace sigil::motion;
using namespace std::chrono_literals;

namespace {

/** The live value every live and shaped slot reads. */
const Animatable<float>& live() {
  static const Animatable<float> cell = animatable(0.5f);
  return cell;
}

enum Kind {
  kConstant = 0,
  kDescribed = 1,
  kLive = 2,
  kBound = 3,
  kMixed = 4
};

const char* kindName(int kind) {
  switch (kind) {
    case kConstant:
      return "constant";
    case kDescribed:
      return "described";
    case kLive:
      return "live";
    case kBound:
      return "bound";
    default:
      return "mixed";
  }
}

Animatable<float> make(int kind, int index) {
  switch (kind == kMixed ? index % 4 : kind) {
    case kConstant:
      return Animatable<float>((float)index);
    case kDescribed:
      return animate({.from = 0.0f, .to = (float)index, .duration = 400ms});
    case kLive:
      return live();
    default:
      return bind(live(), {.from = {0.0f, 1.0f}, .to = {-70.0f, 170.0f}});
  }
}

/** A lane of `count` slots of one kind, or of every kind in rotation. */
std::vector<Animatable<float>> lane(int kind, int count) {
  std::vector<Animatable<float>> slots;
  slots.reserve((size_t)count);
  for (int index = 0; index < count; ++index) slots.push_back(make(kind, index));
  return slots;
}

/** The consumer's read of one slot at @p time into any motion it
 *  describes. */
float resolve(const Animatable<float>& slot, Duration time) {
  if (const float* constant = slot.constant()) return *constant;
  if (const Tween<float>* described = slot.described())
    return described->at(time);
  return slot.value();
}

void countSlots(benchmark::State& state, int count) {
  state.counters["slots/s"] = benchmark::Counter(
      (double)count, benchmark::Counter::kIsIterationInvariantRate);
}

void BM_Resolve(benchmark::State& state) {
  const int count = 1024;
  const std::vector<Animatable<float>> slots = lane((int)state.range(0), count);
  state.SetLabel(kindName((int)state.range(0)));
  Duration time{};
  for ([[maybe_unused]] auto iteration : state) {
    time += 16ms;
    float sink = 0;
    for (const Animatable<float>& slot : slots) sink += resolve(slot, time);
    benchmark::DoNotOptimize(sink);
  }
  countSlots(state, count);
}
BENCHMARK(BM_Resolve)
    ->DenseRange(kConstant, kMixed)
    ->Unit(benchmark::kMicrosecond);

void BM_Copy(benchmark::State& state) {
  const int count = 1024;
  const std::vector<Animatable<float>> slots = lane((int)state.range(0), count);
  state.SetLabel(kindName((int)state.range(0)));
  for ([[maybe_unused]] auto iteration : state) {
    std::vector<Animatable<float>> copy = slots;
    benchmark::DoNotOptimize(copy.data());
    benchmark::ClobberMemory();
  }
  countSlots(state, count);
}
BENCHMARK(BM_Copy)->DenseRange(kConstant, kMixed)->Unit(benchmark::kMicrosecond);

void BM_Construct(benchmark::State& state) {
  const int count = 1024;
  const int kind = (int)state.range(0);
  state.SetLabel(kindName(kind));
  for ([[maybe_unused]] auto iteration : state) {
    std::vector<Animatable<float>> slots = lane(kind, count);
    benchmark::DoNotOptimize(slots.data());
    benchmark::ClobberMemory();
  }
  countSlots(state, count);
}
BENCHMARK(BM_Construct)
    ->DenseRange(kConstant, kMixed)
    ->Unit(benchmark::kMicrosecond);

/** The repeating signal read at a time, per wave: the fold plus one
 *  shape, which is what a value driven off a clock costs per frame. */
void OscillatorAt(benchmark::State& state) {
  const Oscillator wave{.wave = (Wave)state.range(0),
                        .hertz = 3.0f,
                        .amplitude = 40.0f,
                        .centre = 100.0f};
  double seconds = 0.0;
  for ([[maybe_unused]] auto iteration : state) {
    seconds += 1.0 / 60.0;
    benchmark::DoNotOptimize(wave.at(Duration(seconds)));
  }
}
BENCHMARK(OscillatorAt)
    ->Arg((int)Wave::Sine)
    ->Arg((int)Wave::Triangle)
    ->Arg((int)Wave::Square);

/** A keyframed tween read at a time with no engine. The argument is the
 *  keyframe count, since the step that owns a time is found by walking
 *  the keyframes. */
void KeyframesAt(benchmark::State& state) {
  Tween<float> path{.from = 0.0f, .duration = 1s, .ease = ease::linear};
  const int keyframes = (int)state.range(0);
  for (int index = 0; index < keyframes; ++index)
    path.keyframes.push_back({.to = (float)((index * 37) % 11)});
  Duration time{};
  for ([[maybe_unused]] auto iteration : state) {
    time = time < 1s ? Duration(time + 1ms) : Duration{};
    benchmark::DoNotOptimize(path.at(time));
  }
}
BENCHMARK(KeyframesAt)->Arg(4)->Arg(64);

}  // namespace
