/** @file
 * The trilinear 3D-LUT recipe, which needs nothing of OpenColorIO and so
 * is compiled whether or not the build found it.
 */

#include <sigilio/hub/TextLibrary.h>

#include "sigilmaterial/ocio/Ocio.h"

namespace sigil::material::ocio {

const std::shared_ptr<const Recipe>& lutRecipe() {
  static io::TextLibrary shaders("shader://material/ocio/",
                                 SIGIL_MATERIAL_OCIO_SHADER_DIR);
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<LutParams>("color.lut3d")
          .child("content")
          .child("lut")
          .body(Target::SkSL, shaders
                                  .text("shader://material/ocio/"
                                        "Lut3d.sksl")
                                  .value_or("")));
  return recipe;
}

}  // namespace sigil::material::ocio
