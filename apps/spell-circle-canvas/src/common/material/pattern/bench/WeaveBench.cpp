/** @file
 * The cloth under load: the sett expanded and its pivots found, one
 * crossing read, and a square of cloth baked one pixel per thread.
 */

#include <benchmark/benchmark.h>
#include <sigilmaterial/pattern/Weave.h>

#include <vector>

using namespace sigil::material;

namespace {

/** The Black Watch sett, the whole repeat: 252 ends over 24 runs. */
std::vector<pattern::ThreadRun> settRuns() {
  return {{18, 0}, {6, 1},  {2, 0},  {6, 1},  {2, 0},  {18, 1},
          {2, 0},  {6, 1},  {2, 0},  {6, 1},  {18, 0}, {18, 2},
          {6, 0},  {18, 2}, {18, 0}, {18, 1}, {2, 0},  {6, 1},
          {2, 0},  {18, 1}, {18, 0}, {18, 2}, {6, 0},  {18, 2}};
}

pattern::Cloth tartan() {
  const std::vector<uint8_t> count = pattern::threadcount(settRuns());
  return {.warp = count,
          .weft = count,
          .shades = {rgb(0x101010), rgb(0x2C2C80), rgb(0x006818)},
          .rib = 0.22f};
}

void WeaveThreadcount(benchmark::State& state) {
  const std::vector<pattern::ThreadRun> runs = settRuns();
  for ([[maybe_unused]] auto iteration : state) {
    std::vector<uint8_t> count = pattern::threadcount(runs);
    benchmark::DoNotOptimize(count.data());
  }
  state.SetItemsProcessed(state.iterations() * 252);
}
BENCHMARK(WeaveThreadcount);

/** Quadratic in the threadcount, and the reason a sett is expanded once
 *  and its pivots read once rather than per frame. */
void WeavePivots(benchmark::State& state) {
  const std::vector<uint8_t> count = pattern::threadcount(settRuns());
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(pattern::pivots(count).size());
}
BENCHMARK(WeavePivots);

void WeaveCrossing(benchmark::State& state) {
  const pattern::Cloth cloth = tartan();
  int x = 0, y = 0;
  for ([[maybe_unused]] auto iteration : state) {
    benchmark::DoNotOptimize(cloth.at(x, y));
    x = (x + 7) & 255;
    y = (y + 3) & 255;
  }
}
BENCHMARK(WeaveCrossing);

void WeaveClothImage(benchmark::State& state) {
  const int side = (int)state.range(0);
  const pattern::Cloth cloth = tartan();
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(
        pattern::clothImage(cloth, {0, 0}, SkISize::Make(side, side)));
  state.SetItemsProcessed(state.iterations() * (int64_t)side * side);
}
BENCHMARK(WeaveClothImage)->Arg(64)->Arg(512);

void WeaveClothTile(benchmark::State& state) {
  const pattern::Cloth cloth = tartan();
  for ([[maybe_unused]] auto iteration : state) {
    pattern::Tile tile = pattern::clothTile(cloth, 2.0f);
    benchmark::DoNotOptimize(tile.image());
  }
}
BENCHMARK(WeaveClothTile);

}  // namespace
