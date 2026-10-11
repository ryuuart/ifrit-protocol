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
#include <vector>

#include "KeptPage.h"

namespace sigil::sketch {

namespace {

/** A SKETCH'S 2D SCENES ARE PAINTED ON THE DEVICE THE PROCESS DRAWS ON.
 *  There the material programs a page or a screen paints with run on the
 *  device, and the body that wears the result samples the very texture
 *  they painted, with no upload between. On the CPU every one of those
 *  programs is interpreted pixel by pixel, once per map, which for a page
 *  of material is a third of a second a frame.
 *
 *  A PAGE stands there in a capture too: the capture that holds a device
 *  paints its pages where the window paints them, so the two are the same
 *  pixels, and a capture with no device paints them on the CPU. A screen
 *  in a capture that will be diffed keeps the CPU's pixels, because its
 *  composer bakes them and a bake is the CPU's. A scene the device refuses
 *  stays on the CPU and paints there as it would have. */
template <class Scene>
void standOnTheDevice(Scene& scene) {
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
  if (!deterministic) standOnTheDevice(*scene);
  // Kept by the session, not by this context: the context is handed over
  // at setup and gone by the first frame, and the scene has to stand for
  // as long as a body wears it.
  if (scenes) scenes->push_back(scene);
  return scene;
}

namespace detail {

Page keptPage(glm::ivec2 size, compose::SurfaceRange range,
              weave::FontContext& fonts,
              std::vector<std::shared_ptr<compose::SurfaceScene>>* pages) {
  // A page painted as maps takes no pixel bake, so a capture that will be
  // diffed has no promotion decision to pin, and it stands on the device
  // whether or not the capture will be diffed. Kept by the session, as a
  // texture scene is.
  std::shared_ptr<compose::SurfaceScene> scene =
      compose::SurfaceScene::make(SkISize::Make(size.x, size.y), fonts, range);
  if (!scene) return Page();
  standOnTheDevice(*scene);
  const size_t made = pages ? pages->size() : 0;
  if (pages) pages->push_back(scene);
  return Page(std::move(scene),
              made == 0 ? "page" : "page " + std::to_string(made + 1));
}

}  // namespace detail

Page Scene3DContext::page(glm::ivec2 size, compose::SurfaceRange range) {
  return detail::keptPage(size, range, fonts, pages);
}

}  // namespace sigil::sketch
