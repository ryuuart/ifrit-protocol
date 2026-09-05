/** @file The Skia backend's retained shader-source cache. */

#include "ShaderSources.h"

#include <sigilio/hub/TextCatalog.h>

namespace sigil::material::skia {

namespace {

constexpr char kShaderPrefix[] = "shader://material/skia/";

io::TextCatalog& shaderResources() {
  static io::TextCatalog catalog(kShaderPrefix, SIGIL_MATERIAL_SKIA_SHADER_DIR);
  return catalog;
}

}  // namespace

std::string shaderSource(std::string_view name) {
  return shaderResources().text(name).value_or("");
}

}  // namespace sigil::material::skia
