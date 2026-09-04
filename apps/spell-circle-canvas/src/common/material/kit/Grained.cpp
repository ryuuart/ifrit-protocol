/** @file
 * The four grained bodies in each language a renderer speaks — the
 * shared prelude of hash, value noise, the luminance fold and the
 * lattice fleck, then the stone's bed, the timber's arrises and grain
 * lines, the latten's ladder and sheen, and the board's tooth and wear.
 */

#include "sigilmaterial/kit/Grained.h"

#include <sigilio/hub/TextLibrary.h>

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

template <class P>
std::shared_ptr<const Recipe> define(const char* name,
                                     std::string_view shaderName) {
  return std::make_shared<const Recipe>(
      Recipe::of<P>(name)
          .body(Target::SkSL,
                shaderSource("GrainedPrelude.sksl") +
                    shaderSource(std::string(shaderName) + ".sksl"))
          .body(Target::Slang,
                shaderSource("GrainedPrelude.slang") +
                    shaderSource(std::string(shaderName) + ".slang")));
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

}  // namespace sigil::material::kit
