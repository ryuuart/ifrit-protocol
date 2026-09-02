/** @file
 * Encode cost per format and per megapixel, plus the readback the
 * SkImage overload adds on top of it.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>

#include <cstdint>

#include "sigilimage/encode/Encode.h"

namespace {

using sigil::image::encodeImage;
using sigil::image::Format;

/** A gradient with a little per-pixel noise: flat colour compresses to
 *  nothing and would measure the header rather than the codec. */
SkBitmap fixture(int side) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(side, side));
  uint32_t seed = 0x9E3779B9u;
  for (int y = 0; y < side; ++y) {
    auto* row = bitmap.getAddr32(0, y);
    for (int x = 0; x < side; ++x) {
      seed = seed * 1664525u + 1013904223u;
      const int n = (int)(seed >> 28u);
      const int r = (x * 255 / side + n) & 0xFF;
      const int g = (y * 255 / side + n) & 0xFF;
      row[x] = SkPreMultiplyARGB(255, r, g, 128);
    }
  }
  return bitmap;
}

void encodeFormat(benchmark::State& state, Format format) {
  const int side = (int)state.range(0);
  const SkBitmap source = fixture(side);
  for (auto _ : state) {
    sk_sp<SkData> bytes = encodeImage(source.pixmap(), format);
    benchmark::DoNotOptimize(bytes);
  }
  state.SetItemsProcessed(state.iterations() * (int64_t)side * side);
}

void BM_EncodePng(benchmark::State& state) { encodeFormat(state, Format::Png); }
void BM_EncodeJpeg(benchmark::State& state) {
  encodeFormat(state, Format::Jpeg);
}
void BM_EncodeWebp(benchmark::State& state) {
  encodeFormat(state, Format::Webp);
}

/** What the SkImage door costs over the pixmap one: the readback. */
void BM_EncodePngFromImage(benchmark::State& state) {
  const int side = (int)state.range(0);
  const sk_sp<SkImage> image = fixture(side).asImage();
  for (auto _ : state) {
    sk_sp<SkData> bytes = encodeImage(*image, Format::Png);
    benchmark::DoNotOptimize(bytes);
  }
  state.SetItemsProcessed(state.iterations() * (int64_t)side * side);
}

}  // namespace

BENCHMARK(BM_EncodePng)->Arg(64)->Arg(256)->Arg(1024);
BENCHMARK(BM_EncodeJpeg)->Arg(64)->Arg(256)->Arg(1024);
BENCHMARK(BM_EncodeWebp)->Arg(64)->Arg(256);
BENCHMARK(BM_EncodePngFromImage)->Arg(256);

BENCHMARK_MAIN();
