/** @file
 * The pixel difference per megapixel: two identical pictures, which cost
 * a memory comparison a row, against two that differ in every row, which
 * read every pixel as a colour.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>

#include "sigilmedia/difference/Difference.h"

namespace {

SkBitmap ground(int side) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(side, side);
  bitmap.eraseColor(SK_ColorWHITE);
  return bitmap;
}

void BM_DifferenceIdentical(benchmark::State& state) {
  const int side = static_cast<int>(state.range(0));
  const SkBitmap first = ground(side);
  const SkBitmap second = ground(side);
  for (auto _ : state)
    benchmark::DoNotOptimize(
        sigil::media::difference(first.pixmap(), second.pixmap()));
  state.SetItemsProcessed(state.iterations() * side * side);
}

void BM_DifferenceEveryRow(benchmark::State& state) {
  const int side = static_cast<int>(state.range(0));
  const SkBitmap first = ground(side);
  SkBitmap second = ground(side);
  for (int y = 0; y < side; ++y)
    *second.getAddr32(y, y) = SkPreMultiplyColor(SK_ColorBLACK);
  for (auto _ : state)
    benchmark::DoNotOptimize(
        sigil::media::difference(first.pixmap(), second.pixmap()));
  state.SetItemsProcessed(state.iterations() * side * side);
}

}  // namespace

BENCHMARK(BM_DifferenceIdentical)->Arg(256)->Arg(1024);
BENCHMARK(BM_DifferenceEveryRow)->Arg(256)->Arg(1024);
