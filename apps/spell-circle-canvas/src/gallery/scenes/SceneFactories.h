#pragma once

// One factory per scene file in src/gallery/scenes/. GalleryScenes.cpp's
// makeScenes() calls these in display order; nothing else needs the
// concrete Scene subclasses, so each stays private to its own .cpp.

#include "../include/GalleryScenes.h"

#include <memory>

namespace gallery {

std::unique_ptr<Scene> makeExclusionsScene();
std::unique_ptr<Scene> makeKnuthPlassScene();
std::unique_ptr<Scene> makeLoopScene();
std::unique_ptr<Scene> makeRainScene();
std::unique_ptr<Scene> makeRippleScene();
std::unique_ptr<Scene> makeVerticalScene();
std::unique_ptr<Scene> makeMarkersScene();
std::unique_ptr<Scene> makeSlotsScene();
std::unique_ptr<Scene> makeOverflowScene();

} // namespace gallery
