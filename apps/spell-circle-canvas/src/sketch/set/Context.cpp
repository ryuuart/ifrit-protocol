/** @file
 * What a set's context hands it beyond its declarations: a compose scene
 * kept as a texture for a body to wear as its own light, and a page the
 * set lights.
 */

#include <sigilcompose/texture/SurfaceScene.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/set/Set.h>

#include <string>
#include <utility>

namespace sigil::sketch {

std::shared_ptr<compose::TextureScene> SetContext::textureScene(
    SkISize size, sigil::material::Color background) {
  std::shared_ptr<compose::TextureScene> scene =
      compose::TextureScene::make(size, fonts, background);
  // A CAPTURE THAT WILL BE DIFFED pins the scene's promoter off, as every
  // composer a host holds is pinned: the scene is the same runtime making
  // the same measured decision on the same capture.
  if (deterministic)
    scene->setAutoTexturePromotion(compose::PromotionPolicy::Off);
  // Kept by the session, not by this context: the context is handed over
  // at setup and gone by the first frame, and the scene has to stand for
  // as long as a body wears it.
  if (scenes) scenes->push_back(scene);
  return scene;
}

Page SetContext::page(glm::ivec2 size) {
  // A page painted as maps takes no pixel bake, so a capture that will be
  // diffed has no promotion decision to pin. Kept by the session, as a
  // texture scene is.
  std::shared_ptr<compose::SurfaceScene> scene =
      compose::SurfaceScene::make(SkISize::Make(size.x, size.y), fonts);
  if (!scene) return Page();
  // The first page is "page"; each one after it is numbered, so two pages
  // on one frame are two passes an error can tell apart.
  const size_t made = pages ? pages->size() : 0;
  if (pages) pages->push_back(scene);
  return Page(std::move(scene),
              made == 0 ? "page" : "page " + std::to_string(made + 1));
}

}  // namespace sigil::sketch
