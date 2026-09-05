/** @file
 * p5_fractal_garden — a recursive radial tree whose geometry and material
 * share the sketch clock.
 *
 * The branches are generated with p5-style coordinates by one recursive
 * function, then batched by depth through the canvas the Pen lends. Their ink
 * is not an RGB stroke: it is one live material with a procedural grain child,
 * resolved against the canvas and time on every frame. The centre uses a
 * second material fitted to its circle, so the glow needs no raster asset.
 */

#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/draw/Draw.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace sketch = sigil::sketch;
namespace field = sigil::material::field;
namespace mskia = sigil::material::skia;
using namespace sigil::draw;

namespace {

constexpr int kTrunks = 6;
constexpr int kDepth = 7;
constexpr float kFirstLength = 108.0f;

sk_sp<SkRuntimeEffect> branchEffect() {
  static const sk_sp<SkRuntimeEffect> effect = [] {
    auto [built, error] = SkRuntimeEffect::MakeForShader(SkString(R"(
      uniform shader uGrain;
      uniform float2 uResolution;
      uniform float uTime;
      half4 main(float2 xy) {
        float2 uv = xy / max(uResolution, float2(1.0));
        float tooth = uGrain.eval(xy * 0.72).r;
        float current = 0.5 + 0.5 * sin(13.0 * uv.x - 9.0 * uv.y - uTime * 1.4);
        float3 lo = float3(0.15, 0.52, 0.72);
        float3 hi = float3(1.00, 0.48, 0.30);
        float3 colour = mix(lo, hi, current);
        colour *= 0.76 + 0.34 * tooth;
        return half4(half3(colour), 0.94);
      }
    )"));
    return built;
  }();
  return effect;
}

mskia::Paint branchInk() {
  return mskia::Paint::sksl(branchEffect())
      .child("uGrain", mskia::Paint::recipe(field::grain(0.08f, 3, 17.0f)))
      .quantizeTime(30.0f);
}

mskia::Paint ground() {
  return mskia::Paint::blend(
      {{mskia::Paint::solid({0.025f, 0.032f, 0.065f, 1.0f}),
        SkBlendMode::kSrcOver},
       {mskia::Paint::recipe(field::grain(0.012f, 4, 31.0f, 0.6f, 2.2f))
            .amount(0.22f),
        SkBlendMode::kSoftLight}});
}

mskia::Paint budLight() {
  return mskia::Paint::glowUnit({0.36f, 0.30f}, 0.92f,
                                {{0.00f, {1.00f, 0.98f, 0.82f, 1.0f}},
                                 {0.30f, {1.00f, 0.62f, 0.30f, 1.0f}},
                                 {0.72f, {0.42f, 0.30f, 0.90f, 0.92f}},
                                 {1.00f, {0.03f, 0.05f, 0.16f, 0.12f}}});
}

struct P5FractalGarden final : sketch::DrawSketch {
  struct Segment {
    SkPoint from;
    SkPoint to;
  };

  const mskia::Paint branches = branchInk();
  const mskia::Paint background = ground();
  const mskia::Paint buds = budLight();

  void setup(sketch::DrawContext& context) override {
    context.canvas(900, 900);
    context.background(6, 8, 16);
    context.captureAt(0.05);  // the tree is a direct function of the clock
    context.pen.angleMode(RADIANS);
    context.pen.strokeCap(ROUND);
    context.pen.noFill();
  }

  void branch(std::array<std::vector<Segment>, kDepth + 1>& levels,
              std::vector<SkPoint>& tips, float x, float y, float length,
              float angle, int depth, float clock, int lineage) {
    const float x2 = x + std::cos(angle) * length;
    const float y2 = y + std::sin(angle) * length;
    levels[depth].push_back({{x, y}, {x2, y2}});

    if (depth == 0) {
      tips.push_back({x2, y2});
      return;
    }

    const float phase = clock * 0.55f + lineage * 0.71f;
    const float spread = 0.40f + 0.055f * std::sin(phase);
    const float sway = 0.035f * std::sin(clock * 0.8f + depth * 0.9f + lineage);
    branch(levels, tips, x2, y2, length * 0.725f, angle - spread + sway,
           depth - 1, clock, lineage * 2 + 1);
    branch(levels, tips, x2, y2, length * 0.725f, angle + spread + sway,
           depth - 1, clock, lineage * 2 + 2);
  }

  void draw(sketch::DrawContext& context) override {
    Pen& pen = context.pen;
    const float clock = static_cast<float>(pen.millis() * 0.001);
    pen.background(background);

    std::array<std::vector<Segment>, kDepth + 1> levels;
    std::vector<SkPoint> tips;
    tips.reserve(kTrunks * (1 << kDepth));
    const float turn = clock * 0.075f;
    for (int trunk = 0; trunk < kTrunks; ++trunk) {
      const float angle = -HALF_PI + trunk * TAU / kTrunks + turn;
      branch(levels, tips, pen.width * 0.5f, pen.height * 0.5f, kFirstLength,
             angle, kDepth, clock, trunk + 1);
    }

    pen.stroke(branches, CANVAS);
    pen.noFill();
    for (int depth = kDepth; depth >= 0; --depth) {
      pen.strokeWeight(1.35f + 0.52f * depth);
      SkPathBuilder path;
      for (const Segment& segment : levels[depth]) {
        path.moveTo(segment.from);
        path.lineTo(segment.to);
      }
      if (const SkPaint* stroke = pen.strokePaint())
        pen.canvas()->drawPath(path.detach(), *stroke);
    }

    pen.blendMode(ADD);
    pen.stroke(branches, CANVAS);
    pen.strokeWeight(7.0f + 2.0f * (0.5f + 0.5f * std::sin(clock * 2.1f)));
    if (const SkPaint* stroke = pen.strokePaint())
      pen.canvas()->drawPoints(SkCanvas::kPoints_PointMode,
                               SkSpan<const SkPoint>(tips.data(), tips.size()),
                               *stroke);

    pen.push();
    pen.blendMode(BLEND);
    pen.noStroke();
    pen.fill(buds, SHAPE);
    pen.circle(pen.width * 0.5f, pen.height * 0.5f,
               42.0f + 5.0f * std::sin(clock * 1.3f));
    pen.pop();
    pen.blendMode(BLEND);
  }
};

}  // namespace

SIGIL_SKETCH_AS(P5FractalGarden, "p5_fractal_garden", "Draw · Generative",
                "A breathing radial fractal in live grained ink and "
                "shape-fitted material light.")
