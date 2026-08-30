/** @file
 * motion_bind_bench — the binding chain per evaluation: BoundFloat::apply
 * under each envelope, the affine chain alone, and the wiggle field by
 * octave count. Run a Release build; Debug numbers say nothing.
 */

#include <benchmark/benchmark.h>
#include <sigilmotion/bind/Bind.h>

#include <vector>

using namespace sigil::motion;

namespace {

/** A sweep of inputs across the unit range and a little past it, so the
 *  clamp and wrap branches are taken as well as the interior. */
const std::vector<float>& inputs() {
  static const std::vector<float> values = [] {
    std::vector<float> v;
    v.reserve(1024);
    for (int i = 0; i < 1024; ++i)
      v.push_back(-0.25f + 1.5f * (float)i / 1023.0f);
    return v;
  }();
  return values;
}

void countCalls(benchmark::State& state) {
  state.counters["calls/s"] = benchmark::Counter(
      (double)inputs().size(), benchmark::Counter::kIsIterationInvariantRate);
}

void sweep(benchmark::State& state, const BoundFloat& bound) {
  for ([[maybe_unused]] auto iteration : state) {
    float sink = 0;
    for (float v : inputs()) sink += bound.apply(v);
    benchmark::DoNotOptimize(sink);
  }
  countCalls(state);
}

/** The output every chain here binds to; its value is never read
 *  because apply() takes the input explicitly. */
choreograph::Output<float>& source() {
  static choreograph::Output<float> output = 0.0f;
  return output;
}

Bound shaped(Envelope envelope) {
  Bound bound = bind(&source()).source(0, 10).target(-70, 170);
  switch (envelope) {
    case Envelope::kNone:
      break;
    case Envelope::kPingPong:
      bound.pingPong();
      break;
    case Envelope::kCosine:
      bound.cosine();
      break;
    case Envelope::kTrapezoid:
      bound.trapezoid(0.1f, 0.3f, 0.7f, 0.9f);
      break;
    case Envelope::kSquare:
      bound.square(0.4f);
      break;
    case Envelope::kWave:
      bound.wave(&choreograph::easeInOutQuad);
      break;
  }
  return bound;
}

const char* envelopeName(Envelope envelope) {
  switch (envelope) {
    case Envelope::kNone:
      return "none";
    case Envelope::kPingPong:
      return "pingPong";
    case Envelope::kCosine:
      return "cosine";
    case Envelope::kTrapezoid:
      return "trapezoid";
    case Envelope::kSquare:
      return "square";
    case Envelope::kWave:
      return "wave";
  }
  return "";
}

/** The source-normalise, envelope and target chain, one envelope per
 *  arm; the kNone arm is the affine floor the others add to. */
void BM_Apply_Envelope(benchmark::State& state) {
  const auto envelope = (Envelope)state.range(0);
  state.SetLabel(envelopeName(envelope));
  sweep(state, shaped(envelope).value());
}
BENCHMARK(BM_Apply_Envelope)
    ->DenseRange((int)Envelope::kNone, (int)Envelope::kWave)
    ->Unit(benchmark::kMicrosecond);

/** Everything a chain can carry at once: window, curve, envelope,
 *  quantize, wrap, clamp — the most a single evaluation can cost without
 *  wiggle. */
void BM_Apply_FullChain(benchmark::State& state) {
  const Bound bound = bind(&source())
                          .window(0, 1)
                          .map(&choreograph::easeInOutCubic)
                          .pingPong()
                          .quantize(12)
                          .target(-70, 170)
                          .wrap(100)
                          .clamp(-50, 150);
  sweep(state, bound.value());
}
BENCHMARK(BM_Apply_FullChain)->Unit(benchmark::kMicrosecond);

/** The wiggle field on top of the affine chain, by octave: the value
 *  noise is summed once per octave, so the cost is expected to grow
 *  linearly with the count, and one octave minus the kNone envelope arm
 *  is the field's own price per sample. */
void BM_Apply_Wiggle(benchmark::State& state) {
  const int octaves = (int)state.range(0);
  const Bound bound =
      wiggle(&source(), 8.0f, 3.0f, 1u, octaves, 0.5f).target(-70, 170);
  sweep(state, bound.value());
  state.counters["octaves"] = (double)octaves;
  state.SetComplexityN(octaves);
}
BENCHMARK(BM_Apply_Wiggle)
    ->DenseRange(1, 8)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

}  // namespace

BENCHMARK_MAIN();
