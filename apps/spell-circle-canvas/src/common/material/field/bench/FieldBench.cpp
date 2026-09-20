/** @file
 * The fields under load: grain per octave count, the halftone ramp and
 * the CRT overlay, each shaded over a box per side; the CRT screen over
 * a rendered layer at a rising bloom radius, which is where the cost of
 * a light that is blurred rather than gathered shows; and the screen's
 * three subjects each over the same layer on their own, which is what
 * says what a surface pays for taking one of them.
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

constexpr int kScreenSide = 512;

/** A thin bright line drawn into a layer that carries @p effect, which
 *  is the picture every screen arm below is measured over. */
void overLayer(benchmark::State& state, const skia::Effect& effect) {
  SkPaint layer;
  layer.setImageFilter(effect.resolvedImageFilter(nullptr));
  SkPaint ink;
  ink.setColor(SK_ColorGREEN);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul(kScreenSide, kScreenSide));
  SkCanvas& canvas = *surface->getCanvas();
  for ([[maybe_unused]] auto iteration : state) {
    canvas.saveLayer(nullptr, &layer);
    canvas.clear(SK_ColorBLACK);
    canvas.drawRect(SkRect::MakeXYWH(200, 0, 8, kScreenSide), ink);
    canvas.restore();
  }
  state.SetItemsProcessed(state.iterations() * kScreenSide * kScreenSide);
}

/** THE WHOLE SCREEN OVER A LAYER, at the bloom radius the argument
 *  names. The light is a slot the executor fills with the layer
 *  blurred, so what this measures across its arguments is how the cost
 *  of a tube's glow follows its reach. */
void CrtScreenLayer(benchmark::State& state) {
  const field::CrtParameters parameters{
      .uBounds = {0, 0, kScreenSide, kScreenSide},
      .uCurvature = 0.055f,
      .uRgbShift = 0.8f,
      .uRaster = 0.45f,
      .uBloomRadius = (float)state.range(0),
      .uBloom = 0.38f,
      .uNoise = 0.012f,
      .uVignette = 0.22f,
  };
  overLayer(state, skia::Effect::recipe(field::crt(parameters),
                                        field::crtSampleRadius(parameters)));
}
BENCHMARK(CrtScreenLayer)->Arg(2)->Arg(8)->Arg(24)->Arg(64);

/** EACH SUBJECT ON ITS OWN, at the numbers the whole screen is measured
 *  at: what a surface pays for raster lines, for a tube's light, or for
 *  the bend alone, against what it pays for all three. */
void CrtBeamLayer(benchmark::State& state) {
  const field::CrtBeamParameters parameters{
      .uBounds = {0, 0, kScreenSide, kScreenSide},
      .uRgbShift = 0.8f,
      .uRaster = 0.45f,
      .uNoise = 0.012f,
  };
  overLayer(state,
            skia::Effect::recipe(field::crtBeam(parameters),
                                 field::crtBeamSampleRadius(parameters)));
}
BENCHMARK(CrtBeamLayer);

void CrtBloomLayer(benchmark::State& state) {
  const field::CrtBloomParameters parameters{
      .uBounds = {0, 0, kScreenSide, kScreenSide},
      .uBloomRadius = (float)state.range(0),
  };
  overLayer(state, skia::Effect::recipe(field::crtBloom(parameters)));
}
BENCHMARK(CrtBloomLayer)->Arg(2)->Arg(64);

void CrtGlassLayer(benchmark::State& state) {
  const field::CrtGlassParameters parameters{
      .uBounds = {0, 0, kScreenSide, kScreenSide},
      .uCurvature = 0.055f,
      .uBloomRadius = 2.4f,
      .uBloom = 0.38f,
      .uVignette = 0.22f,
  };
  overLayer(state,
            skia::Effect::recipe(field::crtGlass(parameters),
                                 field::crtGlassSampleRadius(parameters)));
}
BENCHMARK(CrtGlassLayer);

}  // namespace
