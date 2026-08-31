/** @file
 * scry_platform_bench — the handlers' per-call costs: a path resolved,
 * a MIME type looked up, a file opened whole into an aligned buffer, an
 * image-slot file synthesized, and a surface reallocated at each size.
 * Run a Release build; Debug numbers say nothing.
 */

#include <Ultralight/Buffer.h>
#include <Ultralight/String.h>
#include <benchmark/benchmark.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "FileSystem.h"
#include "SkiaSurface.h"

using namespace sigil::scry;

namespace {

std::filesystem::path scratch() {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "sigilscry_platform_bench";
  std::filesystem::create_directories(dir);
  return dir;
}

void BM_Resolve(benchmark::State& state) {
  PrefixFileSystem fs("/sdk/resources", "/site", nullptr);
  const ultralight::String path("assets/fonts/body.woff2");
  for ([[maybe_unused]] auto _ : state)
    benchmark::DoNotOptimize(fs.resolve(path));
}
BENCHMARK(BM_Resolve);

void BM_MimeType(benchmark::State& state) {
  PrefixFileSystem fs("/sdk/resources", "/site", nullptr);
  const ultralight::String path("assets/fonts/body.woff2");
  for ([[maybe_unused]] auto _ : state)
    benchmark::DoNotOptimize(fs.GetFileMimeType(path));
}
BENCHMARK(BM_MimeType);

/** A file of range(0) KiB opened whole. */
void BM_OpenFile(benchmark::State& state) {
  const std::filesystem::path dir = scratch();
  const std::string name = "blob_" + std::to_string(state.range(0)) + ".bin";
  {
    std::ofstream out(dir / name, std::ios::binary);
    std::vector<char> bytes((size_t)state.range(0) * 1024, 'x');
    out.write(bytes.data(), (std::streamsize)bytes.size());
  }
  PrefixFileSystem fs("/sdk/resources", dir.string(), nullptr);
  const ultralight::String path(name.c_str());
  for ([[maybe_unused]] auto _ : state)
    benchmark::DoNotOptimize(fs.OpenFile(path));
  state.SetBytesProcessed(state.iterations() * state.range(0) * 1024);
}
BENCHMARK(BM_OpenFile)->Arg(4)->Arg(1024);

void BM_ImageSlotFile(benchmark::State& state) {
  PrefixFileSystem fs("/sdk/resources", "/site", nullptr);
  const ultralight::String path("gauge.imgsrc");
  for ([[maybe_unused]] auto _ : state)
    benchmark::DoNotOptimize(fs.OpenFile(path));
}
BENCHMARK(BM_ImageSlotFile);

/** Reallocation at range(0) pixels a side — what a view resize costs
 *  the surface, alternating two sizes so every iteration reallocates. */
void BM_SurfaceResize(benchmark::State& state) {
  const uint32_t side = (uint32_t)state.range(0);
  SkiaSurface surface(side, side);
  bool toggle = false;
  for ([[maybe_unused]] auto _ : state) {
    surface.Resize(toggle ? side : side + 1, side);
    toggle = !toggle;
  }
  state.SetBytesProcessed(state.iterations() * (int64_t)side * side * 4);
}
BENCHMARK(BM_SurfaceResize)->Arg(256)->Arg(1280);

}  // namespace

BENCHMARK_MAIN();
