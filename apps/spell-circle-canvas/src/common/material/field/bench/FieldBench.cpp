/** @file
 * The fields under load: grain per octave count and the halftone ramp,
 * each shaded over a box per side.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

using namespace sigil::material;

namespace {

void paint(benchmark::State& state, const Material& m, int side) {
  skia::install();
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(side, side));
  SkPaint p;
  p.setShader(skia::shader(m, {.resolution = {(float)side, (float)side}}));
  for (auto _ : state) surface->getCanvas()->drawPaint(p);
  state.SetItemsProcessed(state.iterations() * side * side);
}

void GrainPaint(benchmark::State& state) {
  paint(state, field::grain(0.1f, (int)state.range(0)), 128);
}
BENCHMARK(GrainPaint)->Arg(1)->Arg(4)->Arg(8);

void HalftoneRampPaint(benchmark::State& state) {
  paint(state, field::halftoneRamp(8, 1, 3, {0, 0, 0, 1}), (int)state.range(0));
}
BENCHMARK(HalftoneRampPaint)->Arg(64)->Arg(256);

}  // namespace

BENCHMARK_MAIN();
