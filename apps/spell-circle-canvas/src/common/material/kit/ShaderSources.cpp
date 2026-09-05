/** @file The kit's retained shader-source cache, shared by its builders. */

#include "ShaderSources.h"

#include <sigilio/hub/TextCatalog.h>

#include <utility>

namespace sigil::material::kit {

namespace {

constexpr char kShaderPrefix[] = "shader://material/kit/";

io::TextCatalog& shaderResources() {
  static io::TextCatalog catalog(kShaderPrefix, SIGIL_MATERIAL_KIT_SHADER_DIR);
  return catalog;
}

}  // namespace

std::string shaderSource(std::string_view name) {
  return shaderResources().text(name).value_or("");
}

size_t preloadShaderSources() {
  return shaderResources().preload();
}

}  // namespace sigil::material::kit
