/** @file
 * What a set's context hands it beyond its declarations: a compose scene
 * kept as a texture, for a body to wear.
 */

#include <sigilcompose/texture/Texture.h>
#include <sigilsketch/set/Set.h>

namespace sigil::sketch {

std::shared_ptr<compose::TextureScene> SetContext::textureScene(
    SkISize size, SkColor4f background) {
  std::shared_ptr<compose::TextureScene> scene =
      compose::TextureScene::make(size, fonts, background);
  // Kept by the session, not by this context: the context is handed over
  // at setup and gone by the first frame, and the scene has to stand for
  // as long as a body wears it.
  if (scenes) scenes->push_back(scene);
  return scene;
}

}  // namespace sigil::sketch
