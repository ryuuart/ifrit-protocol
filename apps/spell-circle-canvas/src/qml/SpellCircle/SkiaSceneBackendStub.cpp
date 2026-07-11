#include "SkiaSceneBackend.h"

// Built in place of SkiaSceneBackend.mm when SPELLCIRCLE_ENABLE_SKIA_CANVAS
// is OFF (see CMakeLists.txt) — keeps createSkiaSceneBackend() linkable so
// SpellCircleRenderer never needs to know which build it's in.
std::unique_ptr<CanvasSceneBackend> createSkiaSceneBackend(QRhi *) {
  return nullptr;
}
