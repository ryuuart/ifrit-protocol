/** @file The kit's retained shader-source cache, shared by its builders. */

#include "ShaderSources.h"

#include <sigilio/hub/Hub.h>

#include <utility>

namespace sigil::material::kit {

namespace {

constexpr char kShaderPrefix[] = "shader://material/kit/";

struct ShaderResources {
  ShaderResources() {
    hub.mount(kShaderPrefix, SIGIL_MATERIAL_KIT_SHADER_DIR);
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

size_t preloadShaderSources() {
  ShaderResources& resources = shaderResources();
  resources.retained.refresh();
  return resources.retained.preload();
}

}  // namespace sigil::material::kit
