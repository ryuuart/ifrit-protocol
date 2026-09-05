/** @file
 * The two recipes a baked transform is applied through — the trilinear
 * 3D LUT and the per-channel response row — neither of which needs
 * anything of OpenColorIO, so both are compiled whether or not the build
 * found it.
 */

#include <sigilshaders/MaterialOcio.h>

#include "sigilmaterial/ocio/Ocio.h"

namespace sigil::material::ocio {

const std::shared_ptr<const Recipe>& lutRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<LutParams>("color.lut3d")
          .child("content")
          .child("lut")
          .body(Target::SkSL,
                std::string(shaderSource("Lut3d.sksl"))));
  return recipe;
}

const std::shared_ptr<const Recipe>& responseRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<LutParams>("color.response1d")
          .child("content")
          .child("lut")
          .channelwise("lut")
          .body(Target::SkSL,
                std::string(shaderSource("Response1d.sksl"))));
  return recipe;
}

}  // namespace sigil::material::ocio
