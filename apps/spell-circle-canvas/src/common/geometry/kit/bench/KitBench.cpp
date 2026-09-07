// The kit's two shelves. The silhouettes: generating a path from a value,
// and comparing two values — the two things a caching consumer does per
// frame, one per describe and one per prune. The solids are in
// SolidsBench.cpp beside this file, in the same binary.

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

// The hatch, by the number of lines it lays: the flatten once, then the
// crossing pass over every edge per line.
void BM_HatchOutline(benchmark::State& state) {
  const SkPath outline = star(7, 0.42f)(kBox);
  const Hatch value{.spacing = 180.0f / (float)state.range(0), .angle = 0.4f};
  for (auto _ : state) benchmark::DoNotOptimize(hatchOutline(outline, value));
  state.counters["lines/s"] = benchmark::Counter(
      (double)state.range(0), benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_HatchOutline)
    ->Arg(16)
    ->Arg(128)
    ->Arg(1024)
    ->Unit(benchmark::kMicrosecond);

// The prune's own question, asked once per describe per shaped node: two
// values, are they the same silhouette? It has to stay far cheaper than
// generating one, or comparing to avoid generating is a loss.
void BM_Compare(benchmark::State& state) {
  const Star a = star(7, 0.42f, 0.14f), b = star(7, 0.42f, 0.14f);
  for (auto _ : state) benchmark::DoNotOptimize(a == b);
}
BENCHMARK(BM_Compare);

}  // namespace
