/** @file
 * The stock surfaces under load: a material built per recipe, the shader
 * made from it, and a badge filled per bevel size.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/kit/Surfaces.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Surface.h>

using namespace sigil::material;

namespace {

const Environment& studio() {
  static const Environment env = Environment::studio(256);
  return env;
}

void SurfaceBuild(benchmark::State& state) {
  skia::install();
  const Texture normals = bevelNormals(SkPath::Circle(40, 40, 30), 6);
  for (auto _ : state) {
    Material m = state.range(0) == 0 ? kit::gold(normals, studio())
                                     : kit::chrome(normals, studio());
    benchmark::DoNotOptimize(m);
  }
}
BENCHMARK(SurfaceBuild)->Arg(0)->Arg(1);

void SurfaceShader(benchmark::State& state) {
  skia::install();
  const Texture normals = bevelNormals(SkPath::Circle(40, 40, 30), 6);
  const Material m = kit::gold(normals, studio());
  for (auto _ : state) {
    sk_sp<SkShader> s = skia::shader(m, {});
    benchmark::DoNotOptimize(s);
  }
}
BENCHMARK(SurfaceShader);

void BadgeFill(benchmark::State& state) {
  skia::install();
  const float radius = (float)state.range(0);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul((int)radius * 2 + 40, (int)radius * 2 + 40));
  const SkPath shape = SkPath::Circle(radius + 20, radius + 20, radius);
  const Material m = kit::chrome(bevelNormals(shape, 8), studio());
  for (auto _ : state) skia::fill(*surface->getCanvas(), shape, m);
  state.SetItemsProcessed(state.iterations() *
                          (int64_t)(radius * radius * 3.14159f));
}
BENCHMARK(BadgeFill)->Arg(32)->Arg(128);

}  // namespace

BENCHMARK_MAIN();
