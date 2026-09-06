/** @file
 * The stream and its shapes timed one draw at a time, because that is
 * how they are spent: one draw per scattered stamp, per jittered vertex,
 * per emitted particle. Reported as a rate per call, so a body that
 * grows a step shows up as a rate that fell.
 *
 * The arms are laid out so the two comparisons that matter can be read
 * straight off the table: what the stream costs over the bare mixer it
 * carries, and what a shape costs over the unit draw underneath it.
 */

#include <benchmark/benchmark.h>
#include <sigilcore/compute/Chance.h>

#include <cstdint>
#include <numeric>
#include <span>
#include <vector>

using namespace sigil::core;
using chance::Stream;

namespace {

benchmark::Counter perCall() {
  return benchmark::Counter(1, benchmark::Counter::kIsIterationInvariantRate);
}

/** One arm per source: the same call, over each of the words a stream
 *  can be walked by. */
void streamUnit(benchmark::State& state, Stream stream) {
  float sink = 0;
  for ([[maybe_unused]] auto iteration : state) {
    sink += stream.unit();
    benchmark::DoNotOptimize(sink);
  }
  state.counters["draws/s"] = perCall();
}

void BM_StreamPcg(benchmark::State& state) { streamUnit(state, Stream::pcg(1)); }
BENCHMARK(BM_StreamPcg);

void BM_StreamMix64(benchmark::State& state) {
  streamUnit(state, Stream::mix64(1));
}
BENCHMARK(BM_StreamMix64);

void BM_StreamXorshift(benchmark::State& state) {
  streamUnit(state, Stream::xorshift(1));
}
BENCHMARK(BM_StreamXorshift);

/** The radical inverse is a loop over the digits of the term index, so
 *  unlike every other source its cost grows with how far the stream has
 *  been walked. */
void BM_StreamHalton(benchmark::State& state) {
  streamUnit(state, Stream::halton(3));
}
BENCHMARK(BM_StreamHalton);

void BM_StreamSobol(benchmark::State& state) { streamUnit(state, Stream::sobol()); }
BENCHMARK(BM_StreamSobol);

void BM_StreamGolden(benchmark::State& state) {
  streamUnit(state, Stream::golden(0));
}
BENCHMARK(BM_StreamGolden);

void BM_StreamStratified(benchmark::State& state) {
  streamUnit(state, Stream::stratified(64, 1));
}
BENCHMARK(BM_StreamStratified);

void BM_ShapeUniform(benchmark::State& state) {
  Stream stream = Stream::pcg(1);
  float sink = 0;
  for ([[maybe_unused]] auto iteration : state) {
    sink += stream.sample(chance::Uniform{-1.0f, 1.0f});
    benchmark::DoNotOptimize(sink);
  }
  state.counters["draws/s"] = perCall();
}
BENCHMARK(BM_ShapeUniform);

/** The polar method rejects the pairs that fall outside the disc and
 *  spends the surviving pair on two draws, so the interesting number is
 *  the per-draw one this reports and not the per-pair cost. */
void BM_ShapeGaussian(benchmark::State& state) {
  Stream stream = Stream::pcg(1);
  float sink = 0;
  for ([[maybe_unused]] auto iteration : state) {
    sink += stream.sample(chance::Gaussian{0.0f, 1.0f});
    benchmark::DoNotOptimize(sink);
  }
  state.counters["draws/s"] = perCall();
}
BENCHMARK(BM_ShapeGaussian);

void BM_ShapeExponential(benchmark::State& state) {
  Stream stream = Stream::pcg(1);
  float sink = 0;
  for ([[maybe_unused]] auto iteration : state) {
    sink += stream.sample(chance::Exponential{2.0f});
    benchmark::DoNotOptimize(sink);
  }
  state.counters["draws/s"] = perCall();
}
BENCHMARK(BM_ShapeExponential);

/** A weighted choice walks the weights, so its cost is the size of the
 *  distribution and not the draw. */
void BM_ShapeWeighted(benchmark::State& state) {
  std::vector<float> weights((size_t)state.range(0));
  for (size_t i = 0; i < weights.size(); ++i) weights[i] = (float)(i % 7) + 1.0f;
  Stream stream = Stream::pcg(1);
  size_t sink = 0;
  for ([[maybe_unused]] auto iteration : state) {
    sink += stream.sample(chance::Weighted{weights}).value_or(0);
    benchmark::DoNotOptimize(sink);
  }
  state.counters["draws/s"] = perCall();
}
BENCHMARK(BM_ShapeWeighted)->Arg(4)->Arg(64)->Arg(1024);

void BM_Shuffle(benchmark::State& state) {
  std::vector<int> items((size_t)state.range(0));
  std::iota(items.begin(), items.end(), 0);
  Stream stream = Stream::pcg(1);
  for ([[maybe_unused]] auto iteration : state) {
    chance::shuffle(stream, std::span<int>(items));
    benchmark::DoNotOptimize(items.data());
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Shuffle)->Arg(16)->Arg(256)->Arg(4096);

/** One offer per item is what a reservoir costs, and it is the same
 *  whether the item is kept or passed over. */
void BM_ReservoirOffer(benchmark::State& state) {
  chance::Reservoir reservoir((size_t)state.range(0));
  Stream stream = Stream::pcg(1);
  for ([[maybe_unused]] auto iteration : state) {
    benchmark::DoNotOptimize(reservoir.offer(stream));
  }
  state.counters["offers/s"] = perCall();
}
BENCHMARK(BM_ReservoirOffer)->Arg(8)->Arg(512);

}  // namespace
