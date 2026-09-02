/** @file
 * scry_gpu_bench — the Metal driver's interop per call, with no
 * renderer and no page: a raster upload into a slot, a blit between
 * device textures, a paint through the web-thread recorder (recorded,
 * inserted and submitted), and a wrap of a texture as an SkImage. The
 * host-side cost of queuing each, not the GPU's execution of it. Run a
 * Release build; Debug numbers say nothing.
 */

#import <Metal/Metal.h>

#include <benchmark/benchmark.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/gpu/graphite/Recorder.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <cstdio>
#include <memory>
#include <vector>

#include "metal/MetalDriver.h"

using namespace sigil::scry;

namespace {

sigil::core::hardware::GpuDevice *device() {
  static std::unique_ptr<sigil::core::hardware::GpuDevice> instance =
      sigil::core::hardware::GpuDevice::createOwned(sigil::core::hardware::Backend::Metal);
  return instance.get();
}

sigil::skia::GraphiteContext &graphite() {
  static std::unique_ptr<sigil::skia::GraphiteContext> instance =
      sigil::skia::GraphiteContext::create(*device());
  return *instance;
}

MetalDriver &driver() {
  static std::unique_ptr<MetalDriver> instance = MetalDriver::create(*device(), graphite());
  return *instance;
}

void BM_Upload(benchmark::State &state) {
  const int size = (int)state.range(0);
  const sigil::core::hardware::TextureHandle slot = driver().createImageTexture(size, size);
  const std::vector<uint32_t> pixels((size_t)size * size, 0xff2266aau);
  for ([[maybe_unused]] auto _ : state)
    driver().uploadToTexture(slot, pixels.data(), size, size, (size_t)size * 4);
  state.SetBytesProcessed(state.iterations() * (int64_t)size * size * 4);
  driver().releaseTexture(slot);
}
BENCHMARK(BM_Upload)->Arg(256)->Arg(1024);

void BM_Copy(benchmark::State &state) {
  const int size = (int)state.range(0);
  const sigil::core::hardware::TextureHandle src = driver().createImageTexture(size, size);
  const sigil::core::hardware::TextureHandle dst = driver().createPublishTexture(size, size);
  for ([[maybe_unused]] auto _ : state)
    benchmark::DoNotOptimize(driver().copyDeviceTexture(src, dst, size, size));
  state.SetBytesProcessed(state.iterations() * (int64_t)size * size * 4);
  driver().releaseTexture(src);
  driver().releaseTexture(dst);
}
BENCHMARK(BM_Copy)->Arg(256)->Arg(1024);

void BM_Paint(benchmark::State &state) {
  const int size = (int)state.range(0);
  const sigil::core::hardware::TextureHandle slot = driver().createImageTexture(size, size);
  for ([[maybe_unused]] auto _ : state)
    driver().paintTexture(slot, size, size, [size](SkCanvas &canvas) {
      canvas.clear(SK_ColorDKGRAY);
      SkPaint paint;
      paint.setAntiAlias(true);
      paint.setColor(SK_ColorCYAN);
      canvas.drawCircle(size / 2.0f, size / 2.0f, size / 3.0f, paint);
    });
  state.SetBytesProcessed(state.iterations() * (int64_t)size * size * 4);
  driver().releaseTexture(slot);
}
BENCHMARK(BM_Paint)->Arg(256)->Arg(1024);

/** The wrap alone: a new SkImage over the same texture every iteration,
 *  which is what a frame acquisition costs before the engine's
 *  per-version cache. */
void BM_Wrap(benchmark::State &state) {
  const sigil::core::hardware::TextureHandle slot = driver().createPublishTexture(1280, 720);
  skgpu::graphite::Recorder *recorder = graphite().recorder();
  for ([[maybe_unused]] auto _ : state)
    benchmark::DoNotOptimize(driver().wrapTexture(recorder, slot, 1280, 720));
  driver().releaseTexture(slot);
}
BENCHMARK(BM_Wrap);

}  // namespace

int main(int argc, char **argv) {  // NOLINT(bugprone-exception-escape): an
                                   // uncaught error ends the run
  if (!device() || !MetalDriver::create(*device(), graphite())) {
    std::fprintf(stderr, "scry_gpu_bench: no Metal device or driver on this machine\n");
    return 1;
  }
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
