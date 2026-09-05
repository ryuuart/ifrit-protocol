/** @file
 * The ocio feature under load: the exponent baked per LUT size, and the
 * two lowerings of one baked view applied to a whole canvas — the
 * program over every pixel against the per-channel table an eight-bit
 * surface admits.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <sigilmaterial/ocio/Ocio.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

using namespace sigil::material;

namespace {

void ExponentLutBake(benchmark::State& state) {
  const int n = (int)state.range(0);
  for ([[maybe_unused]] auto iteration : state) {
    Material m = ocio::exponent(2.2f, n);
    benchmark::DoNotOptimize(m);
  }
  state.SetItemsProcessed(state.iterations() * (int64_t)n * n * n);
}
BENCHMARK(ExponentLutBake)->Arg(17)->Arg(33);

/** The view as a composer applies it: the whole output into one layer,
 *  restored through the effect. @p graded says whether there is a view
 *  at all — the ungraded arm is the layer and the fill alone, which both
 *  graded arms also pay — and @p surface is the colour type the view is
 *  lowered for: an eight-bit one admits the table, and the unknown one
 *  is how a caller asks for the program over the same transform, so the
 *  two graded arms differ in nothing but the lowering. */
void ExponentOverACanvas(benchmark::State& state, bool graded,
                         SkColorType surface) {
  skia::install();
  constexpr int kWidth = 900, kHeight = 640;
  SkBitmap output;
  output.allocPixels(SkImageInfo::MakeN32Premul(kWidth, kHeight));
  SkCanvas canvas(output);
  const skia::Effect view =
      graded ? skia::Effect::recipe(ocio::exponent(1.08f), surface)
             : skia::Effect{};
  SkPaint viewPaint;
  viewPaint.setImageFilter(view.imageFilter());
  viewPaint.setColorFilter(view.colorFilter());
  for ([[maybe_unused]] auto iteration : state) {
    canvas.saveLayer(nullptr, &viewPaint);
    canvas.drawColor(SkColorSetARGB(255, 128, 140, 150));
    canvas.restore();
  }
  state.SetItemsProcessed(state.iterations() * (int64_t)kWidth * kHeight);
}
BENCHMARK_CAPTURE(ExponentOverACanvas, none, false, kUnknown_SkColorType);
BENCHMARK_CAPTURE(ExponentOverACanvas, program, true, kUnknown_SkColorType);
BENCHMARK_CAPTURE(ExponentOverACanvas, table, true, kN32_SkColorType);

}  // namespace
