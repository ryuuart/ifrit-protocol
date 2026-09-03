/** @file
 * What a canvas sketch's context hands it beyond its declarations: a
 * compose scene kept as a texture.
 */

#include <sigilcompose/texture/Texture.h>
#include <sigilsketch/canvas/Sketch.h>

namespace sigil::sketch {

std::shared_ptr<compose::TextureScene> SketchContext::textureScene(
    SkISize size, SkColor4f background) {
  if (!fonts) return nullptr;
  std::shared_ptr<compose::TextureScene> scene =
      compose::TextureScene::make(size, *fonts, background);
  // Kept by the session, not by this context: the context is a per-frame
  // value and the scene outlives every one of them.
  if (scenes) scenes->push_back(scene);
  return scene;
}

}  // namespace sigil::sketch
