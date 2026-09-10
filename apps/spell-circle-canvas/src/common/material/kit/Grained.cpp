/** @file
 * The four grained bodies in each language a renderer speaks — the one
 * prelude of hash, value noise, the luminance fold and the lattice
 * fleck, then the stone's bed, the timber's arrises and grain lines, the
 * latten's ladder and sheen, and the board's tooth and wear.
 */

#include "sigilmaterial/kit/Grained.h"

#include <sigilmaterial/core/Terms.h>
#include <sigilshaders/MaterialKit.h>

#include <algorithm>
#include <string>
#include <string_view>

#include "NoisePrelude.h"

namespace sigil::material::kit {

const std::string& noisePrelude(Target target) {
  static const std::string kSlang = std::string(shaderSource("Noise.slang"));
  static const std::string kSkSL = skSLFromSlang(kSlang);
  return target == Target::Slang ? kSlang : kSkSL;
}

namespace {

template <class P>
std::shared_ptr<const Recipe> define(const char* name,
                                     std::string_view shaderName) {
  return std::make_shared<const Recipe>(
      Recipe::of<P>(name)
          .body(Target::SkSL,
                std::string(noisePrelude(Target::SkSL))
                    .append(shaderSource(std::string(shaderName) + ".sksl")))
          .body(Target::Slang,
                std::string(noisePrelude(Target::Slang))
                    .append(shaderSource(std::string(shaderName) + ".slang"))));
}

}  // namespace

const std::shared_ptr<const Recipe>& stoneRecipe() {
  static const std::shared_ptr<const Recipe> recipe =
      define<StoneParams>("kit.stone", "Stone");
  return recipe;
}

const std::shared_ptr<const Recipe>& timberRecipe() {
  static const std::shared_ptr<const Recipe> recipe =
      define<TimberParams>("kit.timber", "Timber");
  return recipe;
}

const std::shared_ptr<const Recipe>& lattenRecipe() {
  static const std::shared_ptr<const Recipe> recipe =
      define<LattenParams>("kit.latten", "Latten");
  return recipe;
}

const std::shared_ptr<const Recipe>& boardRecipe() {
  static const std::shared_ptr<const Recipe> recipe =
      define<BoardParams>("kit.board", "Board");
  return recipe;
}

Material stone(const StoneParams& params) {
  return Material(stoneRecipe(), params);
}

Material timber(const TimberParams& params) {
  return Material(timberRecipe(), params);
}

Material latten(const LattenParams& params) {
  return Material(lattenRecipe(), params);
}

Material board(const BoardParams& params) {
  return Material(boardRecipe(), params);
}

Color lattenTone(const LattenParams& params, float along) {
  const float u =
      std::clamp(params.level + (along - 0.5f) * params.sheen, 0.0f, 1.0f);
  const Color& lo = u < 0.5f ? params.shadow : params.body;
  const Color& hi = u < 0.5f ? params.body : params.light;
  const float f = u < 0.5f ? u * 2.0f : (u - 0.5f) * 2.0f;
  return {lo.r + (hi.r - lo.r) * f, lo.g + (hi.g - lo.g) * f,
          lo.b + (hi.b - lo.b) * f, 1};
}

}  // namespace sigil::material::kit
