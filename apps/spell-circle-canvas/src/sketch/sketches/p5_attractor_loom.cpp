/** @file
 * p5_attractor_loom — iterated strange-attractor traces woven into a field.
 *
 * Every thread follows a Clifford attractor. Its segments are batched through
 * the canvas the Pen lends so chaotic turns never create pathological joins.
 * Nearby seeds diverge into repeated folds, and slowly moving coefficients
 * make those folds breathe without translating the frame. A live material
 * shades the traces through a nested grain field; the material follows the
 * geometry instead of supplying the pattern.
 */

#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/draw/Draw.h>

#include <cmath>

namespace sketch = sigil::sketch;
namespace field = sigil::material::field;
namespace mskia = sigil::material::skia;
using namespace sigil::draw;

namespace {

constexpr int kThreads = 9;
constexpr int kSettlingSteps = 90;
constexpr int kTraceSteps = 720;

sk_sp<SkRuntimeEffect> threadEffect() {
  static const sk_sp<SkRuntimeEffect> effect = [] {
    auto [built, error] = SkRuntimeEffect::MakeForShader(SkString(R"(
      uniform shader uGrain;
      uniform float2 uResolution;
      uniform float uTime;
      half4 main(float2 xy) {
        float2 uv = xy / max(uResolution, float2(1.0));
        float grain = uGrain.eval(xy * 0.48).r;
        float diagonal = 0.5 + 0.5 * sin((uv.x - uv.y) * 10.0 + uTime * 0.8);
        float band = 0.5 + 0.5 * sin((uv.x + uv.y) * 17.0 - uTime * 1.15);
        float3 cyan = float3(0.10, 0.88, 0.94);
        float3 coral = float3(1.00, 0.32, 0.23);
        float3 gold = float3(1.00, 0.78, 0.24);
        float3 colour = mix(cyan, coral, diagonal);
        colour = mix(colour, gold, band * 0.32);
        colour *= 0.78 + grain * 0.34;
        return half4(half3(colour), 0.62);
      }
    )"));
    return built;
  }();
  return effect;
}

mskia::Paint threadInk() {
  return mskia::Paint::sksl(threadEffect())
      .child("uGrain", mskia::Paint::recipe(field::grain(0.07f, 3, 41.0f)))
      .quantizeTime(30.0f);
}

mskia::Paint ground() {
  return mskia::Paint::blend(
      {{mskia::Paint::solid({0.018f, 0.025f, 0.052f, 1.0f}),
        SkBlendMode::kSrcOver},
       {mskia::Paint::recipe(field::grain(0.018f, 4, 29.0f, 0.8f, 1.7f))
            .amount(0.18f),
        SkBlendMode::kSoftLight}});
}

struct P5AttractorLoom final : sketch::DrawSketch {
  const mskia::Paint threads = threadInk();
  const mskia::Paint background = ground();

  void setup(sketch::DrawContext& context) override {
    context.canvas(900, 720);
    context.background(4, 6, 14);
    context.captureAt(0.05);  // the loom is a direct function of the clock
    context.pen.noFill();
    context.pen.strokeCap(ROUND);
    context.pen.strokeJoin(ROUND);
  }

  void trace(Pen& pen, int thread, float clock) {
    const float a = -1.66f + 0.055f * std::sin(clock * 0.17f);
    const float b = 1.20f + 0.045f * std::sin(clock * 0.13f + 1.4f);
    const float c = -1.47f + 0.050f * std::cos(clock * 0.19f + 0.7f);
    const float d = -0.86f + 0.040f * std::sin(clock * 0.11f + 2.2f);
    float x = 0.014f * thread + 0.03f;
    float y = -0.011f * thread - 0.02f;

    for (int step = 0; step < kSettlingSteps; ++step) {
      const float nextX = std::sin(a * y) + c * std::cos(a * x);
      const float nextY = std::sin(b * x) + d * std::cos(b * y);
      x = nextX;
      y = nextY;
    }

    SkPathBuilder segments;
    SkPoint previous = {0.0f, 0.0f};
    for (int step = 0; step < kTraceSteps; ++step) {
      const float nextX = std::sin(a * y) + c * std::cos(a * x);
      const float nextY = std::sin(b * x) + d * std::cos(b * y);
      x = nextX;
      y = nextY;
      const float px = pen.width * (0.50f + x * 0.205f);
      const float py = pen.height * (0.50f + y * 0.225f);
      const SkPoint point = {px, py};
      if (step > 0) {
        segments.moveTo(previous);
        segments.lineTo(point);
      }
      previous = point;
    }
    if (const SkPaint* stroke = pen.strokePaint())
      pen.canvas()->drawPath(segments.detach(), *stroke);
  }

  void draw(sketch::DrawContext& context) override {
    Pen& pen = context.pen;
    const float clock = static_cast<float>(pen.millis() * 0.001);
    pen.background(background);

    pen.noFill();
    pen.blendMode(BLEND);
    pen.stroke(threads, CANVAS);
    pen.strokeWeight(1.9f);
    for (int thread = 0; thread < kThreads; ++thread)
      trace(pen, thread, clock + thread * 0.012f);

    pen.blendMode(BLEND);
  }
};

}  // namespace

SIGIL_SKETCH_AS(P5AttractorLoom, "p5_attractor_loom", "Draw · Generative",
                "Long strange-attractor traces woven in live grained ink and "
                "slowly breathing Clifford coefficients.")
