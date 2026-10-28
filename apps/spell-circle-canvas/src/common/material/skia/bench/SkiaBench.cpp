/** @file
 * material_skia_bench — the shader a material becomes per frame: resolve
 * plus the builder upload and makeShader, by uniform count, against the
 * same SkSL filled by hand. Run a Release build; Debug numbers say
 * nothing.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkGraphics.h>
#include <include/core/SkImage.h>
#include <include/core/SkM44.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkString.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTypes.h>
#include <include/effects/SkImageFilters.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/skia/Filter.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

#include <cmath>
#include <memory>
#include <vector>

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
void through(benchmark::State& state, const Filter& effect) {
  constexpr int kWidth = 1200, kHeight = 800;
  const sk_sp<SkImage> layer = litField(kWidth, kHeight);
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kWidth, kHeight));
  SkPaint paint;
  paint.setImageFilter(skia::resolvedImageFilter(effect, nullptr));
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

/** The cheap bloom: one tap and a small Gaussian, which is the price the
 *  gathered phosphor recipe is judged against. */
void BM_Layer_BrightPass(benchmark::State& state) {
  through(state, Filter::brightPass(0.52f, 0.30f)
                     .then(skia::filter(SkImageFilters::Blur(4, 4, nullptr))));
}
BENCHMARK(BM_Layer_BrightPass);

void BM_Layer_PhosphorBloom(benchmark::State& state) {
  through(state, Filter::phosphorBloom());
}
BENCHMARK(BM_Layer_PhosphorBloom);

void BM_Layer_PhosphorBloom_Drifted(benchmark::State& state) {
  through(state,
          Filter::phosphorBloom(9.0f, 0.52f, 0.46f, 0.80f, -40.0f, 0.5f));
}
BENCHMARK(BM_Layer_PhosphorBloom_Drifted);

/** A WIDE reach, where the halo is gathered coarsest: the same
 *  twenty-four taps, spread far enough that the layer they are gathered
 *  over can be reduced further than the defaults' is. */
void BM_Layer_PhosphorBloom_Wide(benchmark::State& state) {
  through(state, Filter::phosphorBloom(24.0f));
}
BENCHMARK(BM_Layer_PhosphorBloom_Wide);

/** A LIVE LAYER smaller than the canvas it stands on: the layer is
 *  opened, its content drawn and the bloom run as it closes, on a canvas
 *  the size of a screen. What the bloom costs should follow the layer —
 *  a caption line and a caption box — and not the canvas around it. */
void throughLiveLayer(benchmark::State& state, const Filter& effect, int width,
                      int height) {
  constexpr int kCanvasWidth = 1920, kCanvasHeight = 1080;
  const sk_sp<SkImage> content = litField(width, height);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul(kCanvasWidth, kCanvasHeight));
  // The layer stands at an offset on the canvas and knows its own box,
  // as a node's layer does.
  FrameData frame;
  frame.resolution = {(float)width, (float)height};
  SkPaint paint;
  paint.setImageFilter(skia::resolvedImageFilter(effect, &frame));
  const SkRect layer = SkRect::MakeWH((float)width, (float)height);
  for ([[maybe_unused]] auto iteration : state) {
    state.PauseTiming();
    SkGraphics::PurgeResourceCache();
    state.ResumeTiming();
    SkCanvas* canvas = surface->getCanvas();
    canvas->clear(SkColors::kBlack);
    canvas->save();
    canvas->translate(200, 400);
    canvas->saveLayer(&layer, &paint);
    canvas->drawImage(content.get(), 0, 0);
    canvas->restore();
    canvas->restore();
    SkPixmap pixels;
    surface->peekPixels(&pixels);
    benchmark::DoNotOptimize(pixels.addr());
  }
  state.counters["megapixels"] = (double)(width * height) / 1e6;
}

void BM_LiveLayer_PhosphorBloom_CaptionLine(benchmark::State& state) {
  throughLiveLayer(state, Filter::phosphorBloom(), 800, 60);
}
BENCHMARK(BM_LiveLayer_PhosphorBloom_CaptionLine);

void BM_LiveLayer_PhosphorBloom_CaptionBox(benchmark::State& state) {
  throughLiveLayer(state, Filter::phosphorBloom(), 900, 300);
}
BENCHMARK(BM_LiveLayer_PhosphorBloom_CaptionBox);

// ---- a mask read from a ramp of many stops --------------------------------
//
// A page masked by a ramp of two hundred and fifty-six stops against the
// same page masked by that ramp painted once into a strip. The ramp is
// read from a lookup made once per paint, so its cost per pixel stays
// near the strip's however many stops it carries.

constexpr int kMaskedPage = 512;

Material rampOfManyStops() {
  std::vector<ColorStop> stops;
  for (int index = 0; index < 256; ++index) {
    const float along = (float)index / 255.0f;
    const float grey = 0.5f + 0.5f * std::sin(along * 37.0f);
    stops.push_back({along * along, Color{grey, grey, grey, 1}});
  }
  return linearGradient({0, 0}, {1, 0}, stops);
}

Material stripOf(const Material& ramp) {
  SkBitmap strip;
  strip.allocPixels(SkImageInfo::MakeN32Premul(256, 1));
  {
    SkCanvas canvas(strip);
    SkPaint paint;
    paint.setShader(
        skia::shader(skia::paint(ramp), FrameData{.resolution = {256, 1}}));
    canvas.drawPaint(paint);
  }
  strip.setImmutable();
  return skia::base(skia::image(
      strip.asImage(), Repeat::Pad, Repeat::Pad,
      SkMatrix::Scale((float)kMaskedPage / 256.0f, (float)kMaskedPage),
      SkSamplingOptions(SkFilterMode::kLinear)));
}

void paintMaskedPage(benchmark::State& state, const Material& mask) {
  const Material page = Material(Color{0.9f, 0.2f, 0.1f, 1})
                            .layer(Color{0.1f, 0.3f, 0.9f, 1},
                                   {.mask = {mask, ColorChannel::Luminance}});
  const FrameData frame{.resolution = {(float)kMaskedPage, (float)kMaskedPage}};
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(kMaskedPage, kMaskedPage));
  SkCanvas canvas(bitmap);
  SkPaint paint;
  paint.setShader(skia::shader(skia::paint(page), frame));
  for ([[maybe_unused]] auto iteration : state) {
    canvas.drawPaint(paint);
    benchmark::DoNotOptimize(bitmap.getColor(kMaskedPage / 2, kMaskedPage / 2));
  }
}

void BM_RampMask_ManyStops(benchmark::State& state) {
  paintMaskedPage(state, rampOfManyStops());
}
BENCHMARK(BM_RampMask_ManyStops);

void BM_RampMask_Strip(benchmark::State& state) {
  paintMaskedPage(state, stripOf(rampOfManyStops()));
}
BENCHMARK(BM_RampMask_Strip);

}  // namespace

BENCHMARK_CAPTURE(BM_Shader_Live<P2>, 2, kBody2)->Name("BM_Shader_Live/2");
BENCHMARK_CAPTURE(BM_Shader_Live<P16>, 16, kBody16)->Name("BM_Shader_Live/16");
BENCHMARK(BM_Shader_ByHand2);
