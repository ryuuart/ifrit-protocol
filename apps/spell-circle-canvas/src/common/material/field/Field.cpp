/** @file
 * The field bodies: the staggered dot grid under a vertical swell, the
 * pass-through over Skia's Perlin generator, the value-noise fBm with its
 * octave count written into both bodies, the nearest two seeds of a
 * jittered lattice, and the sine displacement.
 */

#include "sigilmaterial/field/Field.h"

#include <include/effects/SkPerlinNoiseShader.h>
#include <sigilmaterial/skia/ShaderLeaf.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilshaders/MaterialField.h>

#include <algorithm>
#include <array>
#include <mutex>
#include <string>
#include <string_view>

namespace sigil::material::field {

namespace {

void replace(std::string& text, std::string_view token,
             std::string_view value) {
  const size_t at = text.find(token);
  if (at != std::string::npos) text.replace(at, token.size(), value);
}

const std::shared_ptr<const Recipe>& halftoneRampRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<HalftoneRampParameters>("field.halftoneRamp")
          .frame(FrameInput::Resolution)
          .body(Target::SkSL, std::string(shaderSource("HalftoneRamp.sksl")))
          .body(Target::Slang,
                std::string(shaderSource("HalftoneRamp.slang"))));
  return recipe;
}

}  // namespace

Material halftoneRamp(float spacing, float minimumRadius, float maximumRadius,
                      Color color, float angleDegrees, float rampFrom,
                      float rampTo) {
  return Material(
      halftoneRampRecipe(),
      HalftoneRampParameters{std::max(spacing, 1.0f), minimumRadius,
                             maximumRadius, angleDegrees * 0.017453293f, 0.0f,
                             0.0f, rampFrom, rampTo, color});
}

namespace {

/** Skia's Perlin generator as a leaf: equal when its parameters are. */
class PerlinLeaf final : public skia::ShaderLeaf {
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

/** What the noise is, read by the device body, which sums the field
 *  itself; the page's body passes Skia's generator through, which holds
 *  the same numbers. */
struct NoiseParameters {
  float uFrequency;
  float uOctaves;
  float uSeed;
  float uTurbulence;
};

const std::shared_ptr<const Recipe>& passThroughRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<NoiseParameters>("field.noise")
          .slot("uSource")
          .frame(FrameInput::Resolution)
          .body(Target::SkSL, std::string(shaderSource("Noise.sksl")))
          .body(Target::Slang, std::string(shaderSource("Noise.slang"))));
  return recipe;
}

}  // namespace

Material noise(float frequency, int octaves, float seed, bool turbulence) {
  Material m(passThroughRecipe(),
             NoiseParameters{frequency, (float)octaves, seed,
                             turbulence ? 1.0f : 0.0f});
  m.slot("uSource", std::shared_ptr<const Leaf>(std::make_shared<PerlinLeaf>(
                        frequency, octaves, seed, turbulence)));
  return m;
}

const std::shared_ptr<const Recipe>& grainRecipe(int octaves) {
  const int n = std::clamp(octaves, 1, 8);
  static std::array<std::shared_ptr<const Recipe>, 9> cache{};
  // Held under a lock: two threads describing grain at once would
  // otherwise write the same slot while the other reads it, and the
  // reference handed back has to name a recipe that is fully built.
  static std::mutex mutex;
  const std::lock_guard lock(mutex);
  if (cache[(size_t)n]) return cache[(size_t)n];
  // Both bodies carry the count as the same constant, so a page and a
  // body sum the same octaves.
  const std::string octaveCount =
      "const int kOctaves = " + std::to_string(n) + ";";
  std::string page(shaderSource("Grain.sksl"));
  replace(page, "const int kOctaves = 1;", octaveCount);
  std::string body(shaderSource("Grain.slang"));
  replace(body, "const int kOctaves = 1;", octaveCount);
  cache[(size_t)n] = std::make_shared<const Recipe>(
      Recipe::of<GrainParameters>("field.grain." + std::to_string(n))
          .body(Target::SkSL, page)
          .body(Target::Slang, body));
  return cache[(size_t)n];
}

Material grain(float frequency, int octaves, float seed, float contrast,
               float stretch) {
  const float k = stretch > 0.01f ? stretch : 1.0f;
  return Material(
      grainRecipe(octaves),
      GrainParameters{{frequency / k, frequency * k}, seed, contrast});
}

const std::shared_ptr<const Recipe>& cellularRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<CellularParameters>("field.cellular")
          .frame(FrameInput::Resolution)
          .body(Target::SkSL, std::string(shaderSource("Cellular.sksl")))
          .body(Target::Slang, std::string(shaderSource("Cellular.slang"))));
  return recipe;
}

Material cellular(float frequency, float jitter, float seed) {
  return Material(cellularRecipe(),
                  CellularParameters{std::max(frequency, 0.0f),
                                     std::clamp(jitter, 0.0f, 1.0f), seed});
}

const std::shared_ptr<const Recipe>& rippleRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<RippleParameters>("field.ripple")
          .slot("content")
          .frame(FrameInput::Resolution)
          .body(Target::SkSL, std::string(shaderSource("Ripple.sksl")))
          .body(Target::Slang, std::string(shaderSource("Ripple.slang"))));
  return recipe;
}

Material ripple(float amplitudePx, float wavelengthPx, float phase,
                bool vertical) {
  return Material(
      rippleRecipe(),
      RippleParameters{amplitudePx, 6.2831853f / std::max(wavelengthPx, 1.0f),
                       phase, vertical ? 1.0f : 0.0f});
}

}  // namespace sigil::material::field

namespace sigil::material {

Material noise(float frequency, NoiseOptions options) {
  if (options.cellular)
    return field::cellular(frequency, options.jitter, options.seed);
  if (options.grain)
    return field::grain(frequency, options.octaves, options.seed,
                        options.contrast, options.stretch);
  return field::noise(frequency, options.octaves, options.seed,
                      options.turbulence);
}

Material grained(Color ground, float amount, float frequency) {
  amount = std::clamp(amount, 0.0f, 1.0f);
  if (amount <= 0.0f) return from(ground);
  // The ground a quarter of the amount darker and lighter, and the noise
  // choosing between them: mid-grey noise is the ground itself, and the
  // grain's reach is the same on every ground rather than a fraction of
  // its value.
  const float reach = amount * 0.25f;
  const Color darker{std::max(0.0f, ground.r - reach),
                     std::max(0.0f, ground.g - reach),
                     std::max(0.0f, ground.b - reach), ground.a};
  const Color lighter{std::min(1.0f, ground.r + reach),
                      std::min(1.0f, ground.g + reach),
                      std::min(1.0f, ground.b + reach), ground.a};
  return from(darker).layer(
      from(lighter),
      {.mask = Mask{.source = noise(frequency, {.octaves = 2, .grain = true}),
                    .channel = ColorChannel::Luminance}});
}

}  // namespace sigil::material
