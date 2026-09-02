// The silhouette shelf's cost: generating a path from a value, and
// comparing two values — the two things a caching consumer does per
// frame, one per describe and one per prune.

#include <benchmark/benchmark.h>
#include <sigilgeometry/kit/Silhouettes.h>

using namespace sigil::geometry::shapes;

namespace {

constexpr SkSize kBox{240, 180};

void BM_GenerateAnalytic(benchmark::State& state) {
  const Star value = star(7, 0.42f, 0.14f);
  for (auto _ : state) benchmark::DoNotOptimize(value(kBox));
}
BENCHMARK(BM_GenerateAnalytic);

void BM_GenerateSampled(benchmark::State& state) {
  const Lissajous value = lissajous(5, 4, 30.0f, 1.0f, (int)state.range(0));
  for (auto _ : state) benchmark::DoNotOptimize(value(kBox));
}
BENCHMARK(BM_GenerateSampled)->Arg(180)->Arg(720)->Arg(2880);

void BM_GenerateBlob(benchmark::State& state) {
  const Blob value = blob(11, 0.2f, (int)state.range(0));
  for (auto _ : state) benchmark::DoNotOptimize(value(kBox));
}
BENCHMARK(BM_GenerateBlob)->Arg(8)->Arg(64);

void BM_GenerateWrapped(benchmark::State& state) {
  const auto value = rounded(star(7, 0.42f), 6.0f);
  for (auto _ : state) benchmark::DoNotOptimize(value(kBox));
}
BENCHMARK(BM_GenerateWrapped);

// The prune's own question, asked once per describe per shaped node: two
// values, are they the same silhouette? It has to stay far cheaper than
// generating one, or comparing to avoid generating is a loss.
void BM_Compare(benchmark::State& state) {
  const Star a = star(7, 0.42f, 0.14f), b = star(7, 0.42f, 0.14f);
  for (auto _ : state) benchmark::DoNotOptimize(a == b);
}
BENCHMARK(BM_Compare);

}  // namespace

BENCHMARK_MAIN();
