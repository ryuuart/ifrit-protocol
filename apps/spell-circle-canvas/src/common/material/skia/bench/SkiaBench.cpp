/** @file
 * material_skia_bench — the shader a material becomes per frame: resolve
 * plus the builder upload and makeShader, by uniform count, against the
 * same SkSL filled by hand. Run a Release build; Debug numbers say
 * nothing.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkGraphics.h>
#include <include/core/SkImage.h>
#include <include/core/SkM44.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkTypes.h>
#include <include/core/SkString.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkImageFilters.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

#include <cmath>
#include <memory>

using namespace sigil::material;

namespace {

struct P2 {
  float uScale;
  Color uColor;
};
struct P16 {
  float a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p;
};

constexpr const char* kBody2 =
    "half4 main(float2 p) { return half4(uColor * uScale); }";
constexpr const char* kBody16 =
    "half4 main(float2 p) { return half4(a + b + c + d + e + f + g + h + i + "
    "j + k + l + m + n + o + p); }";

template <class P>
void BM_Shader_Live(benchmark::State& state, const char* body) {
  skia::install();
  auto recipe = std::make_shared<const Recipe>(
      Recipe::of<P>("bench").frame(FrameInput::Time).body(Target::SkSL, body));
  Material m(recipe);
  FrameData frame;
  for ([[maybe_unused]] auto iteration : state) {
    frame.seconds += 1.0 / 60.0;
    sk_sp<SkShader> s = skia::shader(m, frame);
    benchmark::DoNotOptimize(s.get());
  }
}

void BM_Shader_ByHand2(benchmark::State& state) {
  auto [effect, error] = SkRuntimeEffect::MakeForShader(SkString(
      "uniform float uScale;\nuniform float4 uColor;\nuniform float uTime;\n" +
      std::string(kBody2)));
  float t = 0;
  for ([[maybe_unused]] auto iteration : state) {
    t += 1.0f / 60.0f;
    SkRuntimeShaderBuilder b(effect);
    b.uniform("uScale") = 1.0f;
    b.uniform("uColor") = SkV4{1, 1, 1, 1};
    b.uniform("uTime") = t;
    sk_sp<SkShader> s = b.makeShader();
    benchmark::DoNotOptimize(s.get());
  }
}

// ---- a post-process over a finished layer ---------------------------------
//
// What a display effect costs is what it costs over a WHOLE LAYER, so the
// source is one: a dark field with bright thin figures on it, the shape a
// phosphor bloom is for, at the size a full-canvas layer is.

/** The layer a display effect runs over: a dark field with bright rules,
 *  dots and arcs on it, about a tenth of it lit. */
sk_sp<SkImage> litField(int width, int height) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SkColors::kBlack);
  SkPaint ink;
  ink.setAntiAlias(true);
  ink.setStyle(SkPaint::kStroke_Style);
  for (int i = 0; i < 60; ++i) {
    const float t = (float)i / 60.0f;
    ink.setColor4f({0.2f + 0.8f * t, 0.9f - 0.5f * t, 0.15f, 1.0f});
    ink.setStrokeWidth(1.0f + 2.0f * t);
    canvas->drawLine(0.0f, (float)height * t, (float)width,
                     (float)height * (1.0f - t), ink);
    canvas->drawCircle((float)width * t, (float)height * 0.5f,
                       10.0f + 30.0f * t, ink);
  }
  return surface->makeImageSnapshot();
}

/** One frame: the layer drawn through @p filter, which is the whole of
 *  what an effect over a finished layer costs. */
void through(benchmark::State& state, const skia::Effect& effect) {
  constexpr int kWidth = 1200, kHeight = 800;
  const sk_sp<SkImage> layer = litField(kWidth, kHeight);
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kWidth, kHeight));
  SkPaint paint;
  paint.setImageFilter(effect.resolvedImageFilter(nullptr));
  // EVERY FRAME FILTERS. Skia keys a filtered result on the filter, the
  // source and the matrix it was made under, so a loop that draws the same
  // layer through the same filter at the same place measures a cache
  // lookup and a blit from the second iteration on — and the cheaper the
  // intermediates an effect keeps, the more of them survive the budget,
  // which reads as the wide-reach arm being the fastest of all. The cache
  // is emptied between frames, off the clock, so every arm reports the
  // work and not the lookup.
  for ([[maybe_unused]] auto iteration : state) {
    state.PauseTiming();
    SkGraphics::PurgeResourceCache();
    state.ResumeTiming();
    surface->getCanvas()->clear(SkColors::kBlack);
    surface->getCanvas()->drawImage(layer.get(), 0, 0, SkSamplingOptions(),
                                    &paint);
    SkPixmap pixels;
    surface->peekPixels(&pixels);
    benchmark::DoNotOptimize(pixels.addr());
  }
  state.counters["megapixels"] = (double)(kWidth * kHeight) / 1e6;
}

/** The bright pass a bloom is usually hand-built from: one tap and a
 *  small Gaussian, which is the price the phosphor recipe is judged
 *  against. */
sk_sp<SkRuntimeEffect> brightPass() {
  static const sk_sp<SkRuntimeEffect> effect = [] {
    auto [program, error] = SkRuntimeEffect::MakeForShader(SkString(R"(
uniform shader content;
half4 main(float2 p) {
  half4 c = content.eval(p);
  half peak = max(c.r, max(c.g, c.b));
  return c * smoothstep(half(0.52), half(0.82), peak);
})"));
    return program;
  }();
  return effect;
}

void BM_Layer_BrightPass(benchmark::State& state) {
  through(state, skia::Effect::shader(brightPass())
                     .then(skia::Effect::filter(
                         SkImageFilters::Blur(4, 4, nullptr))));
}
BENCHMARK(BM_Layer_BrightPass);

void BM_Layer_PhosphorBloom(benchmark::State& state) {
  through(state, skia::Effect::phosphorBloom());
}
BENCHMARK(BM_Layer_PhosphorBloom);

void BM_Layer_PhosphorBloom_Drifted(benchmark::State& state) {
  through(state, skia::Effect::phosphorBloom(9.0f, 0.52f, 0.46f, 0.80f, -40.0f,
                                             0.5f));
}
BENCHMARK(BM_Layer_PhosphorBloom_Drifted);

/** A WIDE reach, where the halo is gathered coarsest: the same
 *  twenty-four taps, spread far enough that the layer they are gathered
 *  over can be reduced further than the defaults' is. */
void BM_Layer_PhosphorBloom_Wide(benchmark::State& state) {
  through(state, skia::Effect::phosphorBloom(24.0f));
}
BENCHMARK(BM_Layer_PhosphorBloom_Wide);


}  // namespace

BENCHMARK_CAPTURE(BM_Shader_Live<P2>, 2, kBody2)->Name("BM_Shader_Live/2");
BENCHMARK_CAPTURE(BM_Shader_Live<P16>, 16, kBody16)->Name("BM_Shader_Live/16");
BENCHMARK(BM_Shader_ByHand2);
