/** @file The Skia backend's retained shader-source cache. */

#include "ShaderSources.h"

#include <sigilio/hub/Hub.h>

namespace sigil::material::skia {

namespace {

constexpr char kShaderPrefix[] = "shader://material/skia/";

struct ShaderResources {
  ShaderResources() {
    hub.mount(kShaderPrefix, SIGIL_MATERIAL_SKIA_SHADER_DIR);
    retained = hub.retain(kShaderPrefix);
  }

  io::Hub hub;
  io::ResourceLease retained;
};

ShaderResources& shaderResources() {
  static ShaderResources resources;
  return resources;
}

}  // namespace

std::string shaderSource(std::string_view name) {
  return shaderResources()
      .hub.text(std::string(kShaderPrefix) + std::string(name))
      .value_or("");
}

}  // namespace sigil::material::skia
