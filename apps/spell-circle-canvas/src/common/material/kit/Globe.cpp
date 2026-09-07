/** @file
 * The globe recipe: one body over the params, reading the node's
 * resolution because the disc is inscribed in the node.
 */

#include "sigilmaterial/kit/Globe.h"

#include <sigilshaders/MaterialKit.h>

#include <string>

namespace sigil::material::kit {

const std::shared_ptr<const Recipe>& globeRecipe() {
  static const std::shared_ptr<const Recipe> recipe =
      std::make_shared<const Recipe>(
          Recipe::of<GlobeParams>("kit.globe")
              .body(Target::SkSL, std::string(shaderSource("Globe.sksl")))
              .frame(FrameInput::Resolution));
  return recipe;
}

Material globe(const GlobeParams& params) {
  return Material(globeRecipe(), params);
}

}  // namespace sigil::material::kit
