#pragma once

// Internal composition of the merged "Effects & shaders" scene: the three
// former effect scenes live on as sub-renderers ("parts") in their own
// translation units, and the dispatcher in EffectsScene.cpp switches
// between them with the scene's "mode" choice parameter. Only the effects
// family includes this header — it is not a general factory registry.

#include "../include/GalleryScenes.h"

#include <memory>

namespace gallery {

/// The layered-paint teaching showcase (five presets over one paragraph).
std::unique_ptr<Scene> makeLayerShowcasePart();
/// Extreme "seen from across the room" SkSL shaders as glyph fills.
std::unique_ptr<Scene> makeLoudShadersPart();
/// The fully-placed 2,000-word paragraph with toggleable paint passes.
std::unique_ptr<Scene> makeStressPart();

} // namespace gallery
