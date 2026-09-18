#include <sigilmaterial/field/Crt.h>
#include <sigilshaders/MaterialField.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace sigil::material::field {

const std::shared_ptr<const Recipe>& crtRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<CrtParameters>("field.crt")
          .slot("content")
          .body(Target::SkSL, std::string(shaderSource("Crt.sksl"))));
  return recipe;
}

Material crt(const CrtParameters& parameters) {
  return Material(crtRecipe(), parameters);
}

float crtSampleRadius(const CrtParameters& p) {
  const float extent = std::max(std::abs(p.uBounds.z), std::abs(p.uBounds.w));
  const float distortion = std::abs(p.uCurvature) * 0.5f;
  return extent * 0.5f * (1 + distortion) * distortion + std::abs(p.uRgbShift) +
         std::abs(p.uJitter) + std::abs(p.uSync) + std::abs(p.uBloomRadius) * 3;
}

}  // namespace sigil::material::field
