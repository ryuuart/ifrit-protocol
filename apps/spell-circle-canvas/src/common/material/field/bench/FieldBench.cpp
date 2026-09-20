/** @file
 * The fields under load: grain per octave count, the halftone ramp and
 * the CRT overlay, each shaded over a box per side; and the CRT screen
 * over a rendered layer at a rising bloom radius, which is where the
 * cost of a light that is blurred rather than gathered shows.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/field/Crt.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

using namespace sigil::material;

namespace {

void paint(benchmark::State& state, const Material& m, int side) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(side, side));
  SkPaint p;
  p.setShader(skia::shader(m, {.resolution = {(float)side, (float)side}}));
  for ([[maybe_unused]] auto iteration : state)
    surface->getCanvas()->drawPaint(p);
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

void CrtOverlayPaint(benchmark::State& state) {
  paint(state, field::crtOverlay(), (int)state.range(0));
}
BENCHMARK(CrtOverlayPaint)->Arg(64)->Arg(256);

/** THE SCREEN OVER A LAYER, at the bloom radius the argument names. The
 *  light is a slot the executor fills with the layer blurred, so what
 *  this measures across its arguments is how the cost of a tube's glow
 *  follows its reach. */
void CrtScreenLayer(benchmark::State& state) {
  constexpr int kSide = 512;
  const field::CrtParameters parameters{
      .uBounds = {0, 0, kSide, kSide},
      .uCurvature = 0.055f,
      .uRgbShift = 0.8f,
      .uRaster = 0.45f,
      .uBloomRadius = (float)state.range(0),
      .uBloom = 0.38f,
      .uNoise = 0.012f,
      .uVignette = 0.22f,
  };
  SkPaint layer;
  layer.setImageFilter(
      skia::Effect::recipe(field::crt(parameters),
                           field::crtSampleRadius(parameters))
          .resolvedImageFilter(nullptr));
  SkPaint ink;
  ink.setColor(SK_ColorGREEN);
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kSide, kSide));
  SkCanvas& canvas = *surface->getCanvas();
  for ([[maybe_unused]] auto iteration : state) {
    canvas.saveLayer(nullptr, &layer);
    canvas.clear(SK_ColorBLACK);
    canvas.drawRect(SkRect::MakeXYWH(200, 0, 8, kSide), ink);
    canvas.restore();
  }
  state.SetItemsProcessed(state.iterations() * kSide * kSide);
}
BENCHMARK(CrtScreenLayer)->Arg(2)->Arg(8)->Arg(24)->Arg(64);

}  // namespace
