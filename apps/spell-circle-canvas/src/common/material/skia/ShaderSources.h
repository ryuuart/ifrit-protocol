#pragma once

/** @file The Skia backend's retained shader-source cache. */

#include <string>
#include <string_view>

namespace sigil::material::skia {

std::string shaderSource(std::string_view name);

}  // namespace sigil::material::skia
