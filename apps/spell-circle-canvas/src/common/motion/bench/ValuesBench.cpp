// motion_values_bench — the Animatable slot per lane: what a consumer
// pays to read a property that may be plain, transitioning, bound or
// bound through a shaping chain, and what copying a lane of slots
// costs, since a transition or a chain lives behind an allocation.
// Resolving stays with the consumer — the library ships the value, not
// a resolve surface — so the read here is the one every consumer
// writes: ask which kind the slot holds, then evaluate that kind. Run a
// Release build; Debug numbers say nothing.

#include <benchmark/benchmark.h>
#include <sigilmotion/Values.h>

#include <chrono>
#include <vector>

using namespace sigil::motion;
using namespace std::chrono_literals;

namespace {

/** The live output every bound slot reads. */
choreograph::Output<float>& live() {
  static choreograph::Output<float> output = 0.5f;
  return output;
}

enum Kind {
  kPlain = 0,
  kTransition = 1,
  kBound = 2,
  kBoundMapped = 3,
  kMixed = 4
};

const char* kindName(int kind) {
  switch (kind) {
    case kPlain:
      return "plain";
    case kTransition:
      return "transition";
    case kBound:
      return "bound";
    case kBoundMapped:
      return "boundMapped";
    default:
      return "mixed";
  }
}

Animatable<float> make(int kind, int i) {
  switch (kind == kMixed ? i % 4 : kind) {
    case kPlain:
      return Animatable<float>((float)i);
    case kTransition:
      return animate(from(0.0f).to((float)i), {400ms});
    case kBound:
      return Animatable<float>(&live());
    default:
      return bind(&live()).source(0, 1).target(-70, 170);
  }
}

/** A lane of `count` slots of one kind, or of every kind in rotation. */
std::vector<Animatable<float>> lane(int kind, int count) {
  std::vector<Animatable<float>> slots;
  slots.reserve((size_t)count);
  for (int i = 0; i < count; ++i) slots.push_back(make(kind, i));
  return slots;
}

/** The consumer's read of one slot at a moment `t` seconds into any
 *  transition it holds. */
float resolve(const Animatable<float>& slot, float t) {
  if (const float* plain = slot.plain()) return *plain;
  if (const Transitioned<float>* anim = slot.transitioned()) {
    const float duration = (float)anim->spec.duration.count() / 1000.0f;
    const float progress = duration > 0 ? std::min(t / duration, 1.0f) : 1.0f;
    const float start = anim->from.value_or(anim->value);
    return start + (anim->value - start) * anim->spec.easing()(progress);
  }
  if (const BoundFloat* map = slot.boundMap())
    return map->apply(live().value());
  return slot.binding()->value();
}

void countSlots(benchmark::State& state, int count) {
  state.counters["slots/s"] = benchmark::Counter(
      (double)count, benchmark::Counter::kIsIterationInvariantRate);
}

void BM_Resolve(benchmark::State& state) {
  const int count = 1024;
  const std::vector<Animatable<float>> slots = lane((int)state.range(0), count);
  state.SetLabel(kindName((int)state.range(0)));
  float t = 0;
  for ([[maybe_unused]] auto iteration : state) {
    t += 0.016f;
    float sink = 0;
    for (const Animatable<float>& slot : slots) sink += resolve(slot, t);
    benchmark::DoNotOptimize(sink);
  }
  countSlots(state, count);
}
BENCHMARK(BM_Resolve)
    ->DenseRange(kPlain, kMixed)
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
BENCHMARK(BM_Copy)->DenseRange(kPlain, kMixed)->Unit(benchmark::kMicrosecond);

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
    ->DenseRange(kPlain, kMixed)
    ->Unit(benchmark::kMicrosecond);

}  // namespace

BENCHMARK_MAIN();
