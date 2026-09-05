/** @file
 * image_decode_bench — decodeImage per megapixel. The fixtures are
 * encoded in memory before timing starts: a textured test card at each
 * size run through Skia's PNG and JPEG encoders, so the bytes decoded
 * are the same on every machine and no file is read inside the loop.
 * The committed 4 x 4 fixtures give the per-call floor a tiny image pays.
 * Run a Release build; Debug numbers say nothing.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkData.h>
#include <include/core/SkStream.h>
#include <sigilimage/decode/Decode.h>
#include <sigilimage/encode/Encode.h>

#include <cmath>
#include <cstddef>
#include <fstream>
#include <string>
#include <vector>

using namespace sigil::image;

namespace {

enum Format { kPng = 0, kJpeg = 1 };

/** A `size` x `size` card of gradients, rings and fine noise: enough
 *  structure that neither codec collapses it to a flat run. */
SkBitmap testCard(int size) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  uint32_t seed = 1;
  for (int y = 0; y < size; ++y) {
    auto* row = bitmap.getAddr32(0, y);
    for (int x = 0; x < size; ++x) {
      seed = seed * 1664525u + 1013904223u;
      const float u = (float)x / (float)size, v = (float)y / (float)size;
      const float ring =
          0.5f + 0.5f * std::sin(40.0f * std::hypot(u - 0.5f, v - 0.5f));
      const int r = (int)(255 * u), g = (int)(255 * v), b = (int)(255 * ring);
      const int n = (int)(seed / (1u << 28u));
      row[x] = SkPreMultiplyARGB(255, std::min(255, r + n),
                                 std::min(255, g + n), std::min(255, b + n));
    }
  }
  return bitmap;
}

std::vector<std::byte> encode(const SkBitmap& bitmap, int format) {
  const sk_sp<SkData> data = sigil::image::encodeImage(
      bitmap.pixmap(),
      format == kPng ? sigil::image::Format::Png : sigil::image::Format::Jpeg,
      {.quality = 90});
  if (!data) return {};
  const auto* begin = static_cast<const std::byte*>(data->data());
  return {begin, begin + data->size()};
}

std::vector<std::byte> readFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  const std::string text{std::istreambuf_iterator<char>(in),
                         std::istreambuf_iterator<char>()};
  const auto* begin = reinterpret_cast<const std::byte*>(text.data());
  return {begin, begin + text.size()};
}

void countPixels(benchmark::State& state, int width, int height, size_t bytes) {
  state.counters["Mpx/s"] =
      benchmark::Counter((double)width * height / 1e6,
                         benchmark::Counter::kIsIterationInvariantRate);
  state.SetBytesProcessed(state.iterations() * (int64_t)bytes);
}

void BM_DecodeImage(benchmark::State& state) {
  const int size = (int)state.range(0);
  const int format = (int)state.range(1);
  state.SetLabel(format == kPng ? "png" : "jpeg");
  const std::vector<std::byte> bytes = encode(testCard(size), format);
  const std::filesystem::path hint = format == kPng ? "card.png" : "card.jpg";
  for ([[maybe_unused]] auto iteration : state) {
    std::optional<ImageAsset> asset =
        decodeImage(bytes.data(), bytes.size(), {}, hint);
    benchmark::DoNotOptimize(asset);
  }
  countPixels(state, size, size, bytes.size());
}
BENCHMARK(BM_DecodeImage)
    ->ArgsProduct({{256, 1024, 2048}, {kPng, kJpeg}})
    ->ArgNames({"px", "format"})
    ->Unit(benchmark::kMillisecond);

void BM_DecodeImage_Fixture(benchmark::State& state) {
  const int format = (int)state.range(0);
  const std::string name = format == kPng ? "still.png" : "still.jpg";
  state.SetLabel(name);
  const std::vector<std::byte> bytes =
      readFile(std::string(SIGILIMAGE_TEST_ASSET_DIR) + "/" + name);
  std::optional<ImageAsset> asset;
  for ([[maybe_unused]] auto iteration : state) {
    asset = decodeImage(bytes.data(), bytes.size(), {}, name);
    benchmark::DoNotOptimize(asset);
  }
  if (!asset) {
    state.SkipWithError("fixture did not decode");
    return;
  }
  countPixels(state, asset->width(), asset->height(), bytes.size());
}
BENCHMARK(BM_DecodeImage_Fixture)
    ->Arg(kPng)
    ->Arg(kJpeg)
    ->Unit(benchmark::kMicrosecond);

void BM_ProbeImage(benchmark::State& state) {
  const int format = (int)state.range(0);
  state.SetLabel(format == kPng ? "png" : "jpeg");
  const std::vector<std::byte> bytes = encode(testCard(1024), format);
  for ([[maybe_unused]] auto iteration : state) {
    std::optional<ImageProbe> probe = probeImage(bytes.data(), bytes.size());
    benchmark::DoNotOptimize(probe);
  }
  state.counters["calls/s"] =
      benchmark::Counter(1, benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_ProbeImage)->Arg(kPng)->Arg(kJpeg)->Unit(benchmark::kMicrosecond);

}  // namespace

BENCHMARK_MAIN();
