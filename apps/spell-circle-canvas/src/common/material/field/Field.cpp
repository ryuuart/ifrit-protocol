/** @file
 * The field bodies: the staggered dot grid under a vertical swell, the
 * pass-through over Skia's Perlin generator, the value-noise fBm unrolled
 * per octave count, and the sine displacement.
 */

#include "sigilmaterial/field/Field.h"
#include <sigilmaterial/field/Crt.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkPerlinNoiseShader.h>
#include <sigilmaterial/texture/ShaderLeaf.h>
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

}  // namespace

const std::shared_ptr<const Recipe>& halftoneRampRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<HalftoneRampParameters>("field.halftoneRamp")
          .frame(FrameInput::Resolution)
          .body(Target::SkSL, std::string(shaderSource("HalftoneRamp.sksl"))));
  return recipe;
}

Material halftoneRamp(float spacing, float rMin, float rMax, Color color,
                      float angleDeg, float rampFrom, float rampTo) {
  return Material(halftoneRampRecipe(),
                  HalftoneRampParameters{std::max(spacing, 1.0f), rMin, rMax,
                                         angleDeg * 0.017453293f, 0.0f, 0.0f,
                                         rampFrom, rampTo, color});
}

const std::shared_ptr<const Recipe>& crtOverlayRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<CrtOverlayParameters>("field.crtOverlay")
          .frame(FrameInput::Resolution)
          .body(Target::SkSL, std::string(shaderSource("CrtOverlay.sksl"))));
  return recipe;
}

Material crtOverlay(float scanPitch, float scanStrength, float vigInner,
                    float vigOuter, float vigStrength, float squeeze) {
  return crtOverlay(CrtOverlayParameters{.uScanPitch = scanPitch,
                                         .uScanStrength = scanStrength,
                                         .uVigInner = vigInner,
                                         .uVigOuter = vigOuter,
                                         .uVigStrength = vigStrength,
                                         .uSqueeze = squeeze});
}

Material crtOverlay(const CrtOverlayParameters& parameters) {
  return Material(crtOverlayRecipe(), parameters);
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

struct NoParameters {
  float uUnused;
};

const std::shared_ptr<const Recipe>& passThroughRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<NoParameters>("field.noise")
          .slot("uSource")
          .body(Target::SkSL, std::string(shaderSource("Noise.sksl"))));
  return recipe;
}

}  // namespace

Material noise(float frequency, int octaves, float seed, bool turbulence) {
  Material m(passThroughRecipe(), NoParameters{0});
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
  std::string src(shaderSource("Grain.sksl"));
  replace(src, "const int kOctaves = 1;",
          "const int kOctaves = " + std::to_string(n) + ";");
  cache[(size_t)n] = std::make_shared<const Recipe>(
      Recipe::of<GrainParameters>("field.grain." + std::to_string(n))
          .body(Target::SkSL, src));
  return cache[(size_t)n];
}

Material grain(float frequency, int octaves, float seed, float contrast,
               float stretch) {
  const float k = stretch > 0.01f ? stretch : 1.0f;
  return Material(
      grainRecipe(octaves),
      GrainParameters{{frequency / k, frequency * k}, seed, contrast});
}

const std::shared_ptr<const Recipe>& rippleRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<RippleParameters>("field.ripple")
          .slot("content")
          .body(Target::SkSL, std::string(shaderSource("Ripple.sksl"))));
  return recipe;
}

Material ripple(float amplitudePx, float wavelengthPx, float phase,
                bool vertical) {
  return Material(
      rippleRecipe(),
      RippleParameters{amplitudePx, 6.2831853f / std::max(wavelengthPx, 1.0f),
                       phase, vertical ? 1.0f : 0.0f});
}

std::vector<Material> everyRecipe() {
  std::vector<Material> all;
  all.push_back(halftoneRamp(8, 1, 3, {1, 1, 1, 1}, 15.0f, 0.1f, 0.9f));
  all.push_back(noise(0.03f));
  for (int octaves = 1; octaves <= 4; ++octaves)
    all.push_back(grain(0.05f, octaves));
  all.push_back(crtOverlay());
  sk_sp<SkSurface> content =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(4, 4));
  Material warp = ripple(4, 32);
  if (content) {
    content->getCanvas()->clear(SK_ColorMAGENTA);
    warp.slot("content", Texture::of(content->makeImageSnapshot()));
  }
  // The whole screen and each of the three subjects it composes, since
  // every one of them is a program a backend can be asked for.
  Material screen = crt({.uBounds = {0, 0, 4, 4}});
  Material beam = crtBeam({.uBounds = {0, 0, 4, 4}});
  Material light = crtBloom({.uBounds = {0, 0, 4, 4}});
  Material glass = crtGlass({.uBounds = {0, 0, 4, 4}});
  if (content) {
    // Both slots wherever a recipe declares both, because a slot nothing
    // fills generates a different program from the one a backend will
    // really run — and the bloom slot is the executor's where there is a
    // layer, which a catalogue entry has not got.
    const auto stand = Texture::of(content->makeImageSnapshot());
    screen.slot("content", stand);
    screen.slot("bloom", stand);
    beam.slot("content", stand);
    light.slot("bloom", stand);
    glass.slot("content", stand);
    glass.slot("bloom", stand);
  }
  all.push_back(std::move(screen));
  all.push_back(std::move(beam));
  all.push_back(std::move(light));
  all.push_back(std::move(glass));
  all.push_back(std::move(warp));
  return all;
}

}  // namespace sigil::material::field
