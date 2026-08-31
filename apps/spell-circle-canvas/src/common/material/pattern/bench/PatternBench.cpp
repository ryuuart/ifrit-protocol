/** @file
 * The tiles under load: the bake per generator and pitch, and the
 * repeating texture's shader per call.
 */

#include <benchmark/benchmark.h>
#include <sigilmaterial/pattern/Patterns.h>

using namespace sigil::material;

namespace {

void TileBakeHalftone(benchmark::State& state) {
  const float pitch = (float)state.range(0);
  for ([[maybe_unused]] auto iteration : state) {
    pattern::Tile t =
        pattern::halftone(pitch, pitch * 0.35f, {0.1f, 0.1f, 0.12f, 1});
    benchmark::DoNotOptimize(t.image());
  }
}
BENCHMARK(TileBakeHalftone)->Arg(4)->Arg(16)->Arg(64);

void TileBakeSpeckle(benchmark::State& state) {
  const int count = (int)state.range(0);
  for ([[maybe_unused]] auto iteration : state) {
    pattern::Tile t = pattern::speckle(64, count, 0.5f, 2, {{1, 1, 1, 1}});
    benchmark::DoNotOptimize(t.image());
  }
  state.SetItemsProcessed(state.iterations() * count);
}
BENCHMARK(TileBakeSpeckle)->Arg(50)->Arg(500);

void TileTexture(benchmark::State& state) {
  const pattern::Tile t = pattern::checker(8, {0, 0, 0, 1}, {1, 1, 1, 1});
  t.image();
  for ([[maybe_unused]] auto iteration : state) {
    sk_sp<SkShader> s = t.texture().shader();
    benchmark::DoNotOptimize(s);
  }
}
BENCHMARK(TileTexture);

}  // namespace

BENCHMARK_MAIN();
