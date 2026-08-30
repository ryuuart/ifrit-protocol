/** @file
 * The field bodies: the staggered dot grid under a vertical swell, the
 * pass-through over Skia's Perlin generator, the value-noise fBm unrolled
 * per octave count, and the sine displacement.
 */

#include "sigilmaterial/field/Field.h"

#include <include/effects/SkPerlinNoiseShader.h>
#include <sigilmaterial/texture/ShaderLeaf.h>

#include <algorithm>
#include <array>
#include <string>

namespace sigil::material::field {

const std::shared_ptr<const Recipe>& halftoneRampRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<HalftoneRampParams>("field.halftoneRamp")
          .frame(FrameInput::Resolution)
          .body(Target::SkSL, R"(
      half4 main(float2 xy) {
        float ty = xy.y / max(uResolution.y, 1.0);
        float t = clamp((ty - uRamp0) / max(uRamp1 - uRamp0, 0.001), 0.0, 1.0);
        float2 p = xy + float2(uDriftX, uDriftY);
        float cs = cos(uAngle); float sn = sin(uAngle);
        p = float2(p.x * cs - p.y * sn, p.x * sn + p.y * cs);
        float row = floor(p.y / uSpacing);
        p.x += mod(row, 2.0) * uSpacing * 0.5; // staggered rows
        float2 cell = p - uSpacing * (floor(p / uSpacing) + 0.5);
        float r = mix(uRMin, uRMax, t);
        float d = length(cell) - r;
        float cov = 1.0 - smoothstep(-0.75, 0.75, d);
        float a = uColor.a * cov;
        return half4(half3(uColor.rgb) * a, a);
      }
    )"));
  return recipe;
}

Material halftoneRamp(float spacing, float rMin, float rMax, Color color,
                      float angleDeg, float rampFrom, float rampTo) {
  return Material(halftoneRampRecipe(),
                  HalftoneRampParams{std::max(spacing, 1.0f), rMin, rMax,
                                     angleDeg * 0.017453293f, 0.0f, 0.0f,
                                     rampFrom, rampTo, color});
}

namespace {

/** Skia's Perlin generator as a leaf: equal when its parameters are. */
class PerlinLeaf final : public ShaderLeaf {
 public:
  PerlinLeaf(float frequency, int octaves, float seed, bool turbulence)
      : m_frequency(frequency),
        m_octaves(octaves),
        m_seed(seed),
        m_turbulence(turbulence) {}
  sk_sp<SkShader> shader() const override {
    return m_turbulence
               ? SkShaders::MakeTurbulence(m_frequency, m_frequency, m_octaves,
                                           m_seed, nullptr)
               : SkShaders::MakeFractalNoise(m_frequency, m_frequency,
                                             m_octaves, m_seed, nullptr);
  }

 protected:
  bool equals(const Leaf& other) const override {
    const auto& o = static_cast<const PerlinLeaf&>(other);
    return m_frequency == o.m_frequency && m_octaves == o.m_octaves &&
           m_seed == o.m_seed && m_turbulence == o.m_turbulence;
  }

 private:
  float m_frequency;
  int m_octaves;
  float m_seed;
  bool m_turbulence;
};

struct NoParams {
  float uUnused;
};

const std::shared_ptr<const Recipe>& passThroughRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<NoParams>("field.noise")
          .child("uSource")
          .body(Target::SkSL,
                "half4 main(float2 p) { return uSource.eval(p); }"));
  return recipe;
}

}  // namespace

Material noise(float frequency, int octaves, float seed, bool turbulence) {
  Material m(passThroughRecipe(), NoParams{0});
  m.child("uSource", std::shared_ptr<const Leaf>(std::make_shared<PerlinLeaf>(
                         frequency, octaves, seed, turbulence)));
  return m;
}

const std::shared_ptr<const Recipe>& grainRecipe(int octaves) {
  const int n = std::clamp(octaves, 1, 8);
  static std::array<std::shared_ptr<const Recipe>, 9> cache{};
  if (cache[(size_t)n]) return cache[(size_t)n];
  std::string src = R"(
half4 main(float2 pos) {
  float2 q = pos * uFreq;
  float sum = 0.0;
  float total = 0.0;
)";
  float amp = 0.5f;
  for (int o = 0; o < n; ++o) {
    const std::string a = std::to_string(amp);
    src += R"(
  {
    float2 c = floor(q);
    float2 f = fract(q);
    float2 w = f * f * (3.0 - 2.0 * f);
    float2 k0 = fract((c + float2(0.0, 0.0)) * float2(123.34, 456.21) + uSeed);
    k0 += dot(k0, k0 + 45.32);
    float2 k1 = fract((c + float2(1.0, 0.0)) * float2(123.34, 456.21) + uSeed);
    k1 += dot(k1, k1 + 45.32);
    float2 k2 = fract((c + float2(0.0, 1.0)) * float2(123.34, 456.21) + uSeed);
    k2 += dot(k2, k2 + 45.32);
    float2 k3 = fract((c + float2(1.0, 1.0)) * float2(123.34, 456.21) + uSeed);
    k3 += dot(k3, k3 + 45.32);
    float n = mix(mix(fract(k0.x * k0.y), fract(k1.x * k1.y), w.x),
                  mix(fract(k2.x * k2.y), fract(k3.x * k3.y), w.x), w.y);
    sum += )" +
           a + R"( * n;
    total += )" +
           a + R"(;
    q *= 2.0;
  }
)";
    amp *= 0.5f;
  }
  src += R"(
  float v = total > 0.0 ? sum / total : 0.5;
  v = clamp(0.5 + (v - 0.5) * uContrast, 0.0, 1.0);
  return half4(half3(v), 1.0);
}
)";
  cache[(size_t)n] = std::make_shared<const Recipe>(
      Recipe::of<GrainParams>("field.grain." + std::to_string(n))
          .body(Target::SkSL, src));
  return cache[(size_t)n];
}

Material grain(float frequency, int octaves, float seed, float contrast,
               float stretch) {
  const float k = stretch > 0.01f ? stretch : 1.0f;
  return Material(grainRecipe(octaves),
                  GrainParams{{frequency / k, frequency * k}, seed, contrast});
}

const std::shared_ptr<const Recipe>& rippleRecipe() {
  static const auto recipe =
      std::make_shared<const Recipe>(Recipe::of<RippleParams>("field.ripple")
                                         .child("content")
                                         .body(Target::SkSL, R"(
      half4 main(float2 p) {
        float2 q = p;
        if (uVertical > 0.5)
          q.x += sin(p.y * uFreq + uPhase) * uAmp;
        else
          q.y += sin(p.x * uFreq + uPhase) * uAmp;
        return content.eval(q);
      }
    )"));
  return recipe;
}

Material ripple(float amplitudePx, float wavelengthPx, float phase,
                bool vertical) {
  return Material(
      rippleRecipe(),
      RippleParams{amplitudePx, 6.2831853f / std::max(wavelengthPx, 1.0f),
                   phase, vertical ? 1.0f : 0.0f});
}

}  // namespace sigil::material::field
