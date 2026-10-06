/** @file
 * What a scene3d sketch's context hands it beyond its declarations: a compose
 * scene kept as a texture for a body to wear as its own light, and a page the
 * scene lights.
 */

#include <sigilcompose/texture/SurfaceScene.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/device/Device.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/core/Device.h>
#include <sigilsketch/scene3d/Scene3D.h>

#include <string>
#include <utility>

namespace sigil::sketch {

namespace {

/** A LIVE SCENE3D SKETCH'S 2D SCENES ARE PAINTED ON THE DEVICE THE SKETCH IS
 *  DRAWN ON. There the material programs a page or a screen paints with run on
 *  the device, and the body that wears the result samples the very texture
 *  they painted, with no upload between. On the CPU every one of those
 *  programs is interpreted pixel by pixel, once per map, which for a page
 *  of material is seconds a frame.
 *
 *  A capture that will be diffed keeps the CPU's pixels, which are the
 *  same pixels on every machine; a scene the device refuses stays on the
 *  CPU and paints there as it would have. */
template <class Scene>
void standOnTheDevice(Scene& scene, bool deterministic) {
  if (deterministic) return;
  geometry::device::Device* on = device();
  if (!on || !on->gpu() || !on->graphite()) return;
  (void)scene.useDevice(*on->gpu(), *on->graphite());
}

}  // namespace

std::shared_ptr<compose::TextureScene> Scene3DContext::textureScene(
    SkISize size, sigil::material::Color background) {
  std::shared_ptr<compose::TextureScene> scene =
      compose::TextureScene::make(size, fonts, background);
  // A CAPTURE THAT WILL BE DIFFED pins the scene's promoter off, as every
  // composer a host holds is pinned: the scene is the same runtime making
  // the same measured decision on the same capture.
  if (deterministic)
    scene->setAutoTexturePromotion(compose::PromotionPolicy::Off);
  standOnTheDevice(*scene, deterministic);
  // Kept by the session, not by this context: the context is handed over
  // at setup and gone by the first frame, and the scene has to stand for
  // as long as a body wears it.
  if (scenes) scenes->push_back(scene);
  return scene;
}

Page Scene3DContext::page(glm::ivec2 size) {
  // A page painted as maps takes no pixel bake, so a capture that will be
  // diffed has no promotion decision to pin. Kept by the session, as a
  // texture scene is.
  std::shared_ptr<compose::SurfaceScene> scene =
      compose::SurfaceScene::make(SkISize::Make(size.x, size.y), fonts);
  if (!scene) return Page();
  standOnTheDevice(*scene, deterministic);
  // The first page is "page"; each one after it is numbered, so two pages
  // on one frame are two passes an error can tell apart.
  const size_t made = pages ? pages->size() : 0;
  if (pages) pages->push_back(scene);
  return Page(std::move(scene),
              made == 0 ? "page" : "page " + std::to_string(made + 1));
}

}  // namespace sigil::sketch
