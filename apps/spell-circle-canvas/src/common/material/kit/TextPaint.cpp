/** @file
 * The text paint recipes: six animated fields over one ABI of origin,
 * extent, time and motion, and the two chrome-type ramps.
 */

#include "sigilmaterial/kit/TextPaint.h"

#include <sigilio/hub/TextLibrary.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

namespace sigil::material::kit {

namespace {

std::string shaderSource(std::string_view name) {
  static io::TextLibrary library("shader://material/kit/",
                                 SIGIL_MATERIAL_KIT_SHADER_DIR);
  return library.text("shader://material/kit/" + std::string(name))
      .value_or("");
}

std::shared_ptr<const Recipe> make(const char* name,
                                   std::string_view shaderName) {
  return std::make_shared<const Recipe>(Recipe::of<TextPaintParams>(name).body(
      Target::SkSL, shaderSource(shaderName)));
}

}  // namespace

TextPaintParams textPaintParams(const SkRect& bounds, float timeSeconds) {
  return {{bounds.left(), bounds.top()},
          {std::max(1.0f, bounds.width()), std::max(1.0f, bounds.height())},
          timeSeconds,
          {std::sin(timeSeconds * 0.83f), std::cos(timeSeconds * 0.61f)}};
}

const std::shared_ptr<const Recipe>& waterRecipe() {
  static const auto recipe = make("kit.water", "Water.sksl");
  return recipe;
}

Material water(const SkRect& bounds, float timeSeconds) {
  return Material(waterRecipe(), textPaintParams(bounds, timeSeconds));
}

const std::shared_ptr<const Recipe>& meshGradientRecipe() {
  static const auto recipe = make("kit.meshGradient", "MeshGradient.sksl");
  return recipe;
}

Material meshGradient(const SkRect& bounds, float timeSeconds) {
  return Material(meshGradientRecipe(), textPaintParams(bounds, timeSeconds));
}

const std::shared_ptr<const Recipe>& sparkleRecipe() {
  static const auto recipe = make("kit.sparkle", "Sparkle.sksl");
  return recipe;
}

Material sparkle(const SkRect& bounds, float timeSeconds) {
  return Material(sparkleRecipe(), textPaintParams(bounds, timeSeconds));
}

const std::shared_ptr<const Recipe>& starNestRecipe() {
  static const auto recipe = make("kit.starNest", "StarNest.sksl");
  return recipe;
}

Material starNest(const SkRect& bounds, float timeSeconds) {
  return Material(starNestRecipe(), textPaintParams(bounds, timeSeconds));
}

const std::shared_ptr<const Recipe>& cloudsRecipe() {
  static const auto recipe = make("kit.clouds", "Clouds.sksl");
  return recipe;
}

Material clouds(const SkRect& bounds, float timeSeconds) {
  return Material(cloudsRecipe(), textPaintParams(bounds, timeSeconds));
}

const std::shared_ptr<const Recipe>& tunnelRecipe() {
  static const auto recipe = make("kit.tunnel", "Tunnel.sksl");
  return recipe;
}

Material tunnel(const SkRect& bounds, float timeSeconds) {
  return Material(tunnelRecipe(), textPaintParams(bounds, timeSeconds));
}

std::vector<RampStop> sunsetChromeText() {
  return {{0.0f, rgb(0xEAF6FF)},   {0.12f, rgb(0x9CCFF3)},
          {0.35f, rgb(0x3C7FC0)},  {0.495f, rgb(0x0B2A52)},
          {0.505f, rgb(0x7A4A1A)}, {0.62f, rgb(0xB98A46)},
          {0.82f, rgb(0xE8CE9A)},  {1.0f, rgb(0xFDF6E3)}};
}

std::vector<RampStop> silverChromeText() {
  return chromeRamp(ChromePalette::Silver);
}

}  // namespace sigil::material::kit
