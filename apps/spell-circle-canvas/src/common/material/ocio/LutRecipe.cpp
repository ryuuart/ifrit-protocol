/** @file
 * The trilinear 3D-LUT recipe, which needs nothing of OpenColorIO and so
 * is compiled whether or not the build found it.
 */

#include <sigilio/hub/Hub.h>

#include "sigilmaterial/ocio/Ocio.h"

namespace sigil::material::ocio {

namespace {

constexpr char kShaderPrefix[] = "shader://material/ocio/";

struct ShaderResources {
  ShaderResources() {
    hub.mount(kShaderPrefix, SIGIL_MATERIAL_OCIO_SHADER_DIR);
    retained = hub.retain(kShaderPrefix);
  }

  io::Hub hub;
  io::ResourceLease retained;
};

ShaderResources& shaders() {
  static ShaderResources resources;
  return resources;
}

}  // namespace

const std::shared_ptr<const Recipe>& lutRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<LutParams>("color.lut3d")
          .child("content")
          .child("lut")
          .body(Target::SkSL,
                shaders()
                    .hub.text(std::string(kShaderPrefix) + "Lut3d.sksl")
                    .value_or("")));
  return recipe;
}

}  // namespace sigil::material::ocio
