/** @file
 * The surface program under load: what a dressed program costs to build,
 * what a stated response costs to lower, and what a stack of programs
 * costs to shade.
 */

#include <benchmark/benchmark.h>
#include <sigilmaterial/advanced/Combine.h>
#include <sigilmaterial/mask/Mask.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/surface/Surface.h>

#include <utility>

using namespace sigil::material;

namespace {

void ProgramBuild(benchmark::State& state) {
  const surface::SurfaceParameters parameters;
  for ([[maybe_unused]] auto iteration : state) {
    Material m = state.range(0) == 0 ? surface::program(parameters)
                                     : surface::unlit(parameters);
    benchmark::DoNotOptimize(m);
  }
}
BENCHMARK(ProgramBuild)->Arg(0)->Arg(1);

void Lower(benchmark::State& state) {
  const Material stated =
      from(Color{0.6f, 0.5f, 0.4f, 1})
          .surface(SurfaceOptions{.metallic = 1.0f, .roughness = 0.3f});
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(surface::lower(stated));
}
BENCHMARK(Lower);

void StackShader(benchmark::State& state) {
  Material m = surface::program();
  for (int i = 0; i < (int)state.range(0); ++i)
    m = over(std::move(m), surface::unlit(), maskConstant(0.5f));
  for ([[maybe_unused]] auto iteration : state) {
    sk_sp<SkShader> s = skia::shader(m, {});
    benchmark::DoNotOptimize(s);
  }
}
BENCHMARK(StackShader)->Arg(0)->Arg(2);

}  // namespace
