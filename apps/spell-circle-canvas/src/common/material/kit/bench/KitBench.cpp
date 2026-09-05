/** @file
 * The stock surfaces under load: a material built per recipe, the shader
 * made from it, a badge filled per bevel size, and the metallic-roughness
 * surface built, masked and stacked.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/core/Combine.h>
#include <sigilmaterial/kit/Environments.h>
#include <sigilmaterial/kit/Mask.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilmaterial/kit/Surfaces.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Surface.h>

#include <utility>

using namespace sigil::material;

namespace {

const EnvironmentMap& studio() {
  static const EnvironmentMap env = kit::studioEnvironment(256);
  return env;
}

void SurfaceBuild(benchmark::State& state) {
  skia::install();
  const Texture normals = bevelNormals(SkPath::Circle(40, 40, 30), 6);
  for ([[maybe_unused]] auto iteration : state) {
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
  for ([[maybe_unused]] auto iteration : state) {
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
  for ([[maybe_unused]] auto iteration : state)
    skia::fill(*surface->getCanvas(), shape, m);
  state.SetItemsProcessed(state.iterations() *
                          (int64_t)(radius * radius * 3.14159f));
}
BENCHMARK(BadgeFill)->Arg(32)->Arg(128);

/** The metallic-roughness surface: what a dressed material costs to
 *  build, and what a stack of them costs to shade. */
void PbrBuild(benchmark::State& state) {
  skia::install();
  const kit::SurfaceParams params;
  for ([[maybe_unused]] auto iteration : state) {
    Material m =
        state.range(0) == 0 ? kit::surface(params) : kit::unlit(params);
    benchmark::DoNotOptimize(m);
  }
}
BENCHMARK(PbrBuild)->Arg(0)->Arg(1);

void MaskBuild(benchmark::State& state) {
  skia::install();
  for ([[maybe_unused]] auto iteration : state) {
    Material m = kit::maskConstant(0.5f);
    benchmark::DoNotOptimize(m);
  }
}
BENCHMARK(MaskBuild);

void StackShader(benchmark::State& state) {
  skia::install();
  Material m = kit::surface();
  for (int i = 0; i < (int)state.range(0); ++i)
    m = over(std::move(m), kit::unlit(), kit::maskConstant(0.5f));
  for ([[maybe_unused]] auto iteration : state) {
    sk_sp<SkShader> s = skia::shader(m, {});
    benchmark::DoNotOptimize(s);
  }
}
BENCHMARK(StackShader)->Arg(0)->Arg(2);

}  // namespace

BENCHMARK_MAIN();
