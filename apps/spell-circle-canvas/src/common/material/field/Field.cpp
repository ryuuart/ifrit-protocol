/** @file
 * The field bodies: the staggered dot grid under a vertical swell, the
 * pass-through over Skia's Perlin generator, the value-noise fBm unrolled
 * per octave count, and the sine displacement.
 */

#include "sigilmaterial/field/Field.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkPerlinNoiseShader.h>
#include <sigilio/hub/TextCatalog.h>
#include <sigilmaterial/texture/ShaderLeaf.h>
#include <sigilmaterial/texture/Texture.h>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace sigil::material::field {

namespace {

constexpr char kShaderPrefix[] = "shader://material/field/";

io::TextCatalog& shaders() {
  static io::TextCatalog catalog(kShaderPrefix, SIGIL_MATERIAL_FIELD_SHADER_DIR);
  return catalog;
}

std::string shaderSource(std::string_view name) {
  return shaders().text(name).value_or("");
}

void replace(std::string& text, std::string_view token,
             std::string_view value) {
  const size_t at = text.find(token);
  if (at != std::string::npos) text.replace(at, token.size(), value);
}

}  // namespace

const std::shared_ptr<const Recipe>& halftoneRampRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<HalftoneRampParams>("field.halftoneRamp")
          .frame(FrameInput::Resolution)
          .body(Target::SkSL, shaderSource("HalftoneRamp.sksl")));
  return recipe;
}

Material halftoneRamp(float spacing, float rMin, float rMax, Color color,
                      float angleDeg, float rampFrom, float rampTo) {
  return Material(halftoneRampRecipe(),
                  HalftoneRampParams{std::max(spacing, 1.0f), rMin, rMax,
                                     angleDeg * 0.017453293f, 0.0f, 0.0f,
                                     rampFrom, rampTo, color});
}

const std::shared_ptr<const Recipe>& crtOverlayRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<CrtOverlayParams>("field.crtOverlay")
          .frame(FrameInput::Resolution)
          .body(Target::SkSL, shaderSource("CrtOverlay.sksl")));
  return recipe;
}

Material crtOverlay(float scanPitch, float scanStrength, float vigInner,
                    float vigOuter, float vigStrength, float squeeze) {
  return Material(crtOverlayRecipe(),
                  CrtOverlayParams{scanPitch, scanStrength, vigInner, vigOuter,
                                   vigStrength, squeeze});
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
          .body(Target::SkSL, shaderSource("Noise.sksl")));
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
  std::string src = shaderSource("Grain.sksl");
  replace(src, "const int kOctaves = 1;",
          "const int kOctaves = " + std::to_string(n) + ";");
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
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<RippleParams>("field.ripple")
          .child("content")
          .body(Target::SkSL, shaderSource("Ripple.sksl")));
  return recipe;
}

Material ripple(float amplitudePx, float wavelengthPx, float phase,
                bool vertical) {
  return Material(
      rippleRecipe(),
      RippleParams{amplitudePx, 6.2831853f / std::max(wavelengthPx, 1.0f),
                   phase, vertical ? 1.0f : 0.0f});
}

std::vector<Material> everyRecipe() {
  shaders().preload();
  std::vector<Material> all;
  all.push_back(halftoneRamp(8, 1, 3, {1, 1, 1, 1}, 15.0f, 0.1f, 0.9f));
  all.push_back(noise(0.03f));
  for (int octaves = 1; octaves <= 4; ++octaves)
    all.push_back(grain(0.05f, octaves));
  all.push_back(crtOverlay());
  sk_sp<SkSurface> content =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(4, 4));
  content->getCanvas()->clear(SK_ColorMAGENTA);
  Material warp = ripple(4, 32);
  warp.child("content", Texture::of(content->makeImageSnapshot()));
  all.push_back(std::move(warp));
  return all;
}

}  // namespace sigil::material::field
