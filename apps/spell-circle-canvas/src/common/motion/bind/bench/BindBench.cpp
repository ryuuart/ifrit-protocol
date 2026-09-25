/** @file
 * motion_bind_bench — a binding per evaluation: Binding::apply under each
 * envelope, every stage at once, and the wiggle field by octave count.
 * Run a Release build; Debug numbers say nothing.
 */

#include <benchmark/benchmark.h>
#include <sigilmotion/bind/Binding.h>
#include <sigilmotion/ease/Ease.h>

#include <vector>

using namespace sigil::motion;

namespace {

/** A sweep of inputs across the unit range and a little past it, so the
 *  clamp and wrap branches are taken as well as the interior. */
const std::vector<float>& inputs() {
  static const std::vector<float> values = [] {
    std::vector<float> sweep;
    sweep.reserve(1024);
    for (int index = 0; index < 1024; ++index)
      sweep.push_back(-0.25f + 1.5f * (float)index / 1023.0f);
    return sweep;
  }();
  return values;
}

void countCalls(benchmark::State& state) {
  state.counters["calls/s"] = benchmark::Counter(
      (double)inputs().size(), benchmark::Counter::kIsIterationInvariantRate);
}

void sweep(benchmark::State& state, const Binding& binding) {
  for ([[maybe_unused]] auto iteration : state) {
    float sink = 0;
    for (float value : inputs()) sink += binding.apply(value);
    benchmark::DoNotOptimize(sink);
  }
  countCalls(state);
}

/** The arms of the envelope benchmark: no shape, the there-and-back, and
 *  each envelope the factories build. */
enum class Arm { None, Alternate, Cosine, Trapezoid, Square, Shaped };

Binding shaped(Arm arm) {
  Binding binding{.from = {0.0f, 10.0f}, .to = {-70.0f, 170.0f}};
  switch (arm) {
    case Arm::None:
      break;
    case Arm::Alternate:
      binding.alternate = true;
      break;
    case Arm::Cosine:
      binding.envelope = envelope::cosine();
      break;
    case Arm::Trapezoid:
      binding.envelope = envelope::trapezoid(0.1f, 0.3f, 0.7f, 0.9f);
      break;
    case Arm::Square:
      binding.envelope = envelope::square(0.4f);
      break;
    case Arm::Shaped:
      binding.envelope = envelope::shaped(ease::inOutQuad);
      break;
  }
  return binding;
}

const char* armName(Arm arm) {
  switch (arm) {
    case Arm::None:
      return "none";
    case Arm::Alternate:
      return "alternate";
    case Arm::Cosine:
      return "cosine";
    case Arm::Trapezoid:
      return "trapezoid";
    case Arm::Square:
      return "square";
    case Arm::Shaped:
      return "shaped";
  }
  return "";
}

/** The from, envelope and to stages, one envelope per arm; the none arm
 *  is the range-to-range floor the others add to. */
void BM_Apply_Envelope(benchmark::State& state) {
  const auto arm = (Arm)state.range(0);
  state.SetLabel(armName(arm));
  sweep(state, shaped(arm));
}
BENCHMARK(BM_Apply_Envelope)
    ->DenseRange((int)Arm::None, (int)Arm::Shaped)
    ->Unit(benchmark::kMicrosecond);

/** Every stage but the wiggle at once: clamped from, alternate, curve,
 *  quantize, reverse, to, wrap, clamp — the most a single evaluation can
 *  cost without noise. */
void BM_Apply_AllStages(benchmark::State& state) {
  const Binding binding{.from = {0.0f, 1.0f},
                        .clampFrom = true,
                        .alternate = true,
                        .ease = ease::inOutCubic,
                        .quantize = 12,
                        .reverse = true,
                        .to = {-70.0f, 170.0f},
                        .wrap = 100.0f,
                        .clamp = {-50.0f, 150.0f}};
  sweep(state, binding);
}
BENCHMARK(BM_Apply_AllStages)->Unit(benchmark::kMicrosecond);

/** The wiggle field on top of the output range, by octave: the value
 *  noise is summed once per octave, so the cost is expected to grow
 *  linearly with the count, and one octave minus the none envelope arm
 *  is the field's own price per sample. */
void BM_Apply_Wiggle(benchmark::State& state) {
  const int octaves = (int)state.range(0);
  const Binding binding{.to = {-70.0f, 170.0f},
                        .wiggle = {.amount = 8.0f,
                                   .frequency = 3.0f,
                                   .seed = 1u,
                                   .octaves = octaves,
                                   .falloff = 0.5f}};
  sweep(state, binding);
  state.counters["octaves"] = (double)octaves;
  state.SetComplexityN(octaves);
}
BENCHMARK(BM_Apply_Wiggle)
    ->DenseRange(1, 8)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

}  // namespace
