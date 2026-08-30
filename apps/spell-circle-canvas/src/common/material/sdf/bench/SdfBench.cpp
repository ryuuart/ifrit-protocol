/** @file
 * The signed-distance surfaces under load: the shader built per kind, and
 * a box shaded per side.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/sdf/Sdf.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

using namespace sigil::material;

namespace {

Material styled(sdf::Kind kind) {
  sdf::Style style;
  style.fill = {0.2f, 0.4f, 0.9f, 1};
  style.borderWidth = 2;
  style.glowRadius = 6;
  const sdf::Shape shape = kind == sdf::Kind::Circle     ? sdf::circle()
                           : kind == sdf::Kind::RoundBox ? sdf::roundBox(8)
                                                         : sdf::star(5, 3);
  return sdf::material(shape, style);
}

void SdfShader(benchmark::State& state) {
  skia::install();
  const Material m = styled((sdf::Kind)state.range(0));
  for ([[maybe_unused]] auto iteration : state) {
    sk_sp<SkShader> s = skia::shader(m, {.resolution = {128, 128}});
    benchmark::DoNotOptimize(s);
  }
}
BENCHMARK(SdfShader)->Arg(0)->Arg(1)->Arg(2);

void SdfPaint(benchmark::State& state) {
  skia::install();
  const int side = (int)state.range(0);
  const Material m = styled(sdf::Kind::Star);
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(side, side));
  SkPaint paint;
  paint.setShader(skia::shader(m, {.resolution = {(float)side, (float)side}}));
  for ([[maybe_unused]] auto iteration : state)
    surface->getCanvas()->drawPaint(paint);
  state.SetItemsProcessed(state.iterations() * side * side);
}
BENCHMARK(SdfPaint)->Arg(64)->Arg(256);

}  // namespace

BENCHMARK_MAIN();
