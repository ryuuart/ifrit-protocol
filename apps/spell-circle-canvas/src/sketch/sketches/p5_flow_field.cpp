/** @file
 * p5_flow_field — deterministic streamlines through an animated noise field.
 *
 * The paths are generated with p5's noise and beginShape vocabulary. One
 * canvas-space material shades every bold stroke, combining a procedural
 * field child with an automatically injected clock. Geometry and pixels can
 * therefore move at different rates while the frame remains reproducible.
 */

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

constexpr int kAcross = 11;
constexpr int kDown = 9;
constexpr int kSteps = 40;
constexpr float kStepLength = 6.2f;

float hash01(int value) {
  const float wave = std::sin(value * 127.1f + 311.7f) * 43758.5453f;
  return wave - std::floor(wave);
}

sk_sp<SkRuntimeEffect> flowEffect() {
  static const sk_sp<SkRuntimeEffect> effect = [] {
    auto [built, error] = SkRuntimeEffect::MakeForShader(SkString(R"(
      uniform shader uField;
      uniform float2 uResolution;
      uniform float uTime;
      half4 main(float2 xy) {
        float2 uv = xy / max(uResolution, float2(1.0));
        float field = uField.eval(xy * 0.55 + float2(uTime * 9.0, 0.0)).r;
        float ribbon = 0.5 + 0.5 * sin(8.0 * uv.x + 11.0 * uv.y - uTime * 1.7);
        float3 cyan = float3(0.18, 0.90, 0.94);
        float3 violet = float3(0.72, 0.28, 1.00);
        float3 amber = float3(1.00, 0.68, 0.20);
        float3 colour = mix(cyan, violet, ribbon);
        colour = mix(colour, amber, smoothstep(0.62, 0.92, field) * 0.62);
        return half4(half3(colour), 0.72 + 0.22 * field);
      }
    )"));
    return built;
  }();
  return effect;
}

mskia::Paint currentInk() {
  return mskia::Paint::sksl(flowEffect())
      .child("uField", mskia::Paint::recipe(field::noise(0.025f, 4, 23.0f)))
      .quantizeTime(30.0f);
}

mskia::Paint particleLight() {
  return mskia::Paint::glowUnit({0.34f, 0.30f}, 0.92f,
                                {{0.0f, {1.0f, 1.0f, 0.88f, 1.0f}},
                                 {0.32f, {0.30f, 0.94f, 1.0f, 0.96f}},
                                 {1.0f, {0.18f, 0.08f, 0.42f, 0.0f}}});
}

struct P5FlowField final : sketch::DrawSketch {
  const mskia::Paint ink = currentInk();
  const mskia::Paint sparks = particleLight();

  void setup(sketch::DrawContext& context) override {
    context.canvas(960, 720);
    context.background(4, 7, 17);
    context.captureAt(0.05);  // the field is a direct function of the clock
    context.pen.noiseSeed(809u);
    context.pen.noiseDetail(5, 0.54f);
    context.pen.strokeCap(ROUND);
    context.pen.strokeJoin(ROUND);
    context.pen.noFill();
  }

  void draw(sketch::DrawContext& context) override {
    Pen& pen = context.pen;
    const float clock = static_cast<float>(pen.millis() * 0.001);
    pen.background(4, 7, 17);
    pen.blendMode(ADD);
    pen.stroke(ink, CANVAS);
    pen.strokeWeight(3.8f);
    pen.noFill();

    for (int row = 0; row < kDown; ++row) {
      for (int column = 0; column < kAcross; ++column) {
        const int id = row * kAcross + column;
        float x =
            (column + 0.5f + (hash01(id) - 0.5f) * 0.62f) * pen.width / kAcross;
        float y = (row + 0.5f + (hash01(id + 701) - 0.5f) * 0.62f) *
                  pen.height / kDown;
        x += 13.0f * std::sin(clock * 0.31f + id * 0.73f);
        y += 10.0f * std::cos(clock * 0.27f + id * 0.51f);

        pen.beginShape();
        for (int step = 0; step < kSteps; ++step) {
          if (x < -20.0f || x > pen.width + 20.0f || y < -20.0f ||
              y > pen.height + 20.0f)
            break;
          pen.vertex(x, y);
          const float sample =
              pen.noise(x * 0.0029f, y * 0.0029f, clock * 0.055f + id * 0.001f);
          const float angle = sample * TAU * 2.35f +
                              0.22f * std::sin(clock * 0.42f + row * 0.8f);
          x += std::cos(angle) * kStepLength;
          y += std::sin(angle) * kStepLength;
        }
        pen.endShape();

        if (id % 11 == 0) {
          pen.push();
          pen.noStroke();
          pen.fill(sparks, SHAPE);
          pen.circle(x, y, 15.0f + 4.0f * std::sin(clock * 1.6f + id));
          pen.pop();
          pen.stroke(ink, CANVAS);
          pen.noFill();
        }
      }
    }
    pen.blendMode(BLEND);
  }
};

}  // namespace

SIGIL_SKETCH_AS(P5FlowField, "p5_flow_field", "Draw · Generative",
                "Bold deterministic streamlines shaded by a live nested "
                "procedural material.")
