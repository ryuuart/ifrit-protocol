/** @file
 * p5_refractive_metaballs — merging liquid glass and drawn tendrils.
 *
 * Eight moving radial fields become one implicit surface. The surface can
 * split into islands, grow a neck, merge and separate without a tessellated
 * outline. Its material derives a normal from the same field, refracts a live
 * algorithmic backdrop through a child shader, disperses its channels and
 * lights the changing edge. Pen-drawn Bezier filaments connect the field's
 * moving centres, so the geometry and the material share one animation.
 */

#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/draw/Draw.h>

#include <array>
#include <cmath>
#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace mskia = sigil::material::skia;
using namespace sigil::draw;

namespace {

constexpr int kLobeCount = 8;
constexpr float kThreshold = 1.16f;

struct Lobe {
  SkPoint centre;
  float radius;
};

sk_sp<SkRuntimeEffect> lineFieldEffect() {
  static const sk_sp<SkRuntimeEffect> effect = [] {
    auto [built, error] = SkRuntimeEffect::MakeForShader(SkString(R"(
      uniform float2 uResolution;
      uniform float uTime;
      half4 main(float2 xy) {
        float side = max(min(uResolution.x, uResolution.y), 1.0);
        float2 uv = (xy - uResolution * 0.5) / side;
        float radius = length(uv);
        float angle = atan(uv.y, uv.x);
        float warpX = uv.x + sin(uv.y * 8.0 - uTime * 0.72) * 0.075;
        float warpY = uv.y + sin(uv.x * 7.0 + uTime * 0.58) * 0.085;
        float a = sin(warpX * 43.0 + sin(warpY * 12.0) * 3.8);
        float b = sin(warpY * 37.0 + sin(warpX * 10.0) * 4.6);
        float c = sin(radius * 70.0 - angle * 5.0 - uTime * 1.25);
        float lineA = 1.0 - smoothstep(0.030, 0.095, abs(a));
        float lineB = 1.0 - smoothstep(0.026, 0.088, abs(b));
        float lineC = 1.0 - smoothstep(0.020, 0.072, abs(c));
        float3 colour = float3(0.004, 0.010, 0.032);
        colour += float3(0.02, 0.90, 1.18) * lineA;
        colour += float3(0.92, 0.08, 1.28) * lineB;
        colour += float3(1.34, 0.42, 0.04) * lineC * 0.72;
        float vignette = 1.0 - 0.62 * smoothstep(0.26, 0.78, radius);
        return half4(half3(colour * vignette), 1.0);
      }
    )"));
    return built;
  }();
  return effect;
}

sk_sp<SkRuntimeEffect> glassEffect() {
  static const sk_sp<SkRuntimeEffect> effect = [] {
    auto [built, error] = SkRuntimeEffect::MakeForShader(SkString(R"(
      uniform shader uSource;
      uniform float4 uBall0;
      uniform float4 uBall1;
      uniform float4 uBall2;
      uniform float4 uBall3;
      uniform float4 uBall4;
      uniform float4 uBall5;
      uniform float4 uBall6;
      uniform float4 uBall7;
      uniform float uThreshold;
      uniform float uStrength;
      uniform float2 uResolution;
      uniform float uTime;

      float3 sampleBall(float2 xy, float4 ball) {
        float2 delta = xy - ball.xy;
        float squareDistance = max(dot(delta, delta), 4.0);
        float weight = ball.z * ball.z / squareDistance;
        return float3(weight, delta * weight / squareDistance);
      }

      half4 main(float2 xy) {
        float3 s0 = sampleBall(xy, uBall0);
        float3 s1 = sampleBall(xy, uBall1);
        float3 s2 = sampleBall(xy, uBall2);
        float3 s3 = sampleBall(xy, uBall3);
        float3 s4 = sampleBall(xy, uBall4);
        float3 s5 = sampleBall(xy, uBall5);
        float3 s6 = sampleBall(xy, uBall6);
        float3 s7 = sampleBall(xy, uBall7);
        float3 sum = s0 + s1 + s2 + s3 + s4 + s5 + s6 + s7;
        float field = sum.x;
        float2 outward = normalize(sum.yz + float2(0.0001));
        float alpha = smoothstep(uThreshold * 0.86, uThreshold * 1.02,
                                 field);
        float depth = smoothstep(uThreshold, uThreshold * 4.2, field);
        float shell = 1.0 - smoothstep(uThreshold * 0.98,
                                       uThreshold * 1.34, field);
        float shimmer = 0.95 + 0.07 * sin(uTime * 1.7 + field * 0.82);
        float2 bend = -outward * uStrength * (0.20 + depth * 0.80) * shimmer;
        half4 redSample = uSource.eval(xy + bend * 1.075);
        half4 greenSample = uSource.eval(xy + bend);
        half4 blueSample = uSource.eval(xy + bend * 0.925);

        float3 normal = normalize(float3(outward * (0.80 - depth * 0.24),
                                         0.60 + depth * 0.40));
        float key = pow(max(dot(normal,
                                normalize(float3(-0.52, -0.62, 0.58))), 0.0),
                        42.0);
        float rim = pow(max(dot(normal,
                                normalize(float3(0.70, 0.10, 0.70))), 0.0),
                        68.0);
        float caustic = pow(max(sin(field * 11.0 - uTime * 1.8), 0.0), 18.0);
        float3 colour = float3(redSample.r, greenSample.g, blueSample.b);
        colour *= 1.12 + depth * 0.32;
        colour += float3(0.22, 0.90, 1.36) * shell * 1.34;
        colour += float3(2.20, 1.82, 0.96) * key * 1.82;
        colour += float3(1.02, 0.46, 1.72) * rim * 1.36;
        colour += float3(0.30, 1.08, 1.34) * caustic * depth * 0.42;
        return half4(half3(colour * alpha), half(alpha));
      }
    )"));
    return built;
  }();
  return effect;
}

sk_sp<SkRuntimeEffect> tendrilEffect() {
  static const sk_sp<SkRuntimeEffect> effect = [] {
    auto [built, error] = SkRuntimeEffect::MakeForShader(SkString(R"(
      uniform float2 uResolution;
      uniform float uTime;
      half4 main(float2 xy) {
        float2 uv = xy / max(uResolution, float2(1.0));
        float pulse = 0.5 + 0.5 * sin((uv.x * 1.3 + uv.y) * 19.0 -
                                      uTime * 3.2);
        float3 cyan = float3(0.16, 1.18, 1.32);
        float3 pearl = float3(1.40, 1.32, 1.18);
        float3 magenta = float3(1.18, 0.28, 1.20);
        float3 colour = mix(cyan, magenta, uv.y * 0.72 + pulse * 0.18);
        colour = mix(colour, pearl, pow(pulse, 7.0) * 0.82);
        return half4(half3(colour), 0.86);
      }
    )"));
    return built;
  }();
  return effect;
}

mskia::Paint lineField() {
  return mskia::Paint::sksl(lineFieldEffect()).quantizeTime(30.0f);
}

mskia::Paint tendrilInk() {
  return mskia::Paint::sksl(tendrilEffect()).quantizeTime(30.0f);
}

std::array<float, 4> uniform(const Lobe& lobe) {
  return {lobe.centre.x(), lobe.centre.y(), lobe.radius, 0.0f};
}

mskia::Paint glass(const mskia::Paint& source,
                   const std::array<Lobe, kLobeCount>& lobes) {
  mskia::Paint paint =
      mskia::Paint::sksl(glassEffect(),
                         {{"uThreshold", kThreshold}, {"uStrength", 42.0f}})
          .child("uSource", source)
          .quantizeTime(30.0f);
  for (int index = 0; index < kLobeCount; ++index)
    paint =
        paint.uniform("uBall" + std::to_string(index), uniform(lobes[index]));
  return paint;
}

SkPoint mix(SkPoint a, SkPoint b, float amount) { return a + (b - a) * amount; }

void drawTendril(Pen& pen, SkPoint from, SkPoint to, int index, float clock,
                 float offset) {
  const SkPoint delta = to - from;
  const float length = std::max(delta.length(), 1.0f);
  const SkPoint normal = {-delta.y() / length, delta.x() / length};
  const float curl =
      std::sin(clock * (0.74f + index * 0.017f) + index * 1.71f) *
      (66.0f + std::fmod(index * 17.0f, 52.0f));
  const float ripple = std::cos(clock * 1.13f + index * 0.83f) * 38.0f;
  const SkPoint controlA = mix(from, to, 0.28f) + normal * (curl + offset);
  const SkPoint controlB =
      mix(from, to, 0.72f) + normal * (-curl * 0.58f + ripple + offset);
  pen.bezier(from.x(), from.y(), controlA.x(), controlA.y(), controlB.x(),
             controlB.y(), to.x(), to.y());
}

struct P5RefractiveMetaballs final : sketch::DrawSketch {
  const mskia::Paint source = lineField();
  const mskia::Paint filament = tendrilInk();

  void setup(sketch::DrawContext& context) override {
    context.canvas(720, 720);
    context.background(2, 5, 14);
    context.captureAt(0.05);  // the field is a direct function of the clock
    context.pen.strokeCap(ROUND);
    context.pen.strokeJoin(ROUND);
  }

  std::array<Lobe, kLobeCount> lobes(float clock) const {
    const float left = std::sin(clock * 0.82f);
    const float right = std::sin(clock * 0.71f + 2.0f);
    const float vertical = std::cos(clock * 0.63f + 0.8f);
    return {{{{302.0f + std::sin(clock * 0.47f) * 22.0f,
               352.0f + std::cos(clock * 0.54f) * 17.0f},
              112.0f},
             {{418.0f + std::cos(clock * 0.43f) * 24.0f,
               360.0f + std::sin(clock * 0.51f) * 20.0f},
              108.0f},
             {{132.0f + left * 88.0f, 224.0f + std::cos(clock * 0.91f) * 42.0f},
              88.0f},
             {{142.0f + std::cos(clock * 0.68f + 0.4f) * 82.0f,
               514.0f + std::sin(clock * 0.77f) * 48.0f},
              92.0f},
             {{584.0f - right * 86.0f,
               220.0f + std::sin(clock * 0.88f + 1.2f) * 48.0f},
              90.0f},
             {{578.0f - std::cos(clock * 0.73f + 1.1f) * 84.0f,
               520.0f + std::cos(clock * 0.84f) * 42.0f},
              96.0f},
             {{356.0f + std::sin(clock * 0.66f + 1.7f) * 74.0f,
               102.0f + vertical * 88.0f},
              82.0f},
             {{366.0f + std::cos(clock * 0.61f + 2.5f) * 70.0f,
               610.0f - vertical * 78.0f},
              88.0f}}};
  }

  void drawTendrils(Pen& pen, const std::array<Lobe, kLobeCount>& balls,
                    float clock) const {
    constexpr std::array<std::pair<int, int>, 13> links = {
        std::pair{0, 1}, std::pair{0, 2}, std::pair{0, 3}, std::pair{1, 4},
        std::pair{1, 5}, std::pair{0, 6}, std::pair{1, 6}, std::pair{0, 7},
        std::pair{1, 7}, std::pair{2, 6}, std::pair{4, 6}, std::pair{3, 7},
        std::pair{5, 7}};

    pen.noFill();
    pen.blendMode(ADD);
    pen.stroke(30, 205, 255, 30);
    pen.strokeWeight(5.0f);
    for (int index = 0; index < static_cast<int>(links.size()); ++index) {
      const auto [from, to] = links[index];
      drawTendril(pen, balls[from].centre, balls[to].centre, index, clock,
                  0.0f);
    }

    pen.stroke(filament, CANVAS);
    pen.strokeWeight(1.35f);
    for (int index = 0; index < static_cast<int>(links.size()); ++index) {
      const auto [from, to] = links[index];
      for (float offset : {-7.0f, 0.0f, 7.0f})
        drawTendril(pen, balls[from].centre, balls[to].centre, index, clock,
                    offset);
    }
  }

  void draw(sketch::DrawContext& context) override {
    Pen& pen = context.pen;
    const float clock = static_cast<float>(pen.millis() * 0.001);
    const std::array<Lobe, kLobeCount> balls = lobes(clock);
    pen.background(source);

    pen.blendMode(BLEND);
    pen.noStroke();
    pen.fill(glass(source, balls), CANVAS);
    pen.rect(0.0f, 0.0f, pen.width, pen.height);

    drawTendrils(pen, balls, clock);
    pen.blendMode(BLEND);
  }
};

}  // namespace

SIGIL_SKETCH_AS(P5RefractiveMetaballs, "p5_refractive_metaballs",
                "Draw · Generative",
                "Merging liquid-glass fields refract a live line shader while "
                "animated Pen tendrils weave through their moving centres.")
