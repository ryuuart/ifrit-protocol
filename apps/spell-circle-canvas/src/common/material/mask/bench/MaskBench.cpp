/** @file
 * The mask under load: the constant built, and the sampled one's shader
 * made once its source is dressed.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/mask/Mask.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

using namespace sigil::material;

namespace {

void MaskBuild(benchmark::State& state) {
  skia::install();
  for ([[maybe_unused]] auto iteration : state) {
    Material m = maskConstant(0.5f);
    benchmark::DoNotOptimize(m);
  }
}
BENCHMARK(MaskBuild);

void SampledMaskShader(benchmark::State& state) {
  skia::install();
  sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(2, 2));
  s->getCanvas()->clear(SK_ColorWHITE);
  const Material m = maskMap(Texture::of(s->makeImageSnapshot()));
  for ([[maybe_unused]] auto iteration : state) {
    sk_sp<SkShader> shader = skia::shader(m, {});
    benchmark::DoNotOptimize(shader);
  }
}
BENCHMARK(SampledMaskShader);

}  // namespace

BENCHMARK_MAIN();
