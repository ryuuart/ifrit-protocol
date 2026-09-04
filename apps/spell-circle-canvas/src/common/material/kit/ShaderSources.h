#pragma once

/** @file The kit's retained shader-source cache, shared by its builders. */

#include <cstddef>
#include <string>
#include <string_view>

namespace sigil::material::kit {

std::string shaderSource(std::string_view name);
size_t preloadShaderSources();

}  // namespace sigil::material::kit
