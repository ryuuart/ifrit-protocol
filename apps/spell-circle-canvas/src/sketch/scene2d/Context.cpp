/** @file
 * What a scene2d sketch's context hands it beyond its declarations: a
 * compose scene kept as a texture, a page a baked scene lights, and a lit
 * scene baked to an image.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/advanced/Skia.h>
#include <sigilworld/frame/Frame.h>
#include <sigilworld/scene/Scene.h>

#include "../scene3d/KeptPage.h"

namespace sigil::sketch {

std::shared_ptr<compose::TextureScene> Scene2DContext::textureScene(
    SkISize size, sigil::material::Color background) {
  if (!fonts) return nullptr;
  std::shared_ptr<compose::TextureScene> scene =
      compose::TextureScene::make(size, *fonts, background);
  // Kept by the session, not by this context: the context is a per-frame
  // value and the scene outlives every one of them.
  if (scenes) scenes->push_back(scene);
  return scene;
}

Page Scene2DContext::page(glm::ivec2 size, compose::SurfaceRange range) {
  if (!fonts) return Page();
  return detail::keptPage(size, range, *fonts, pages);
}

sk_sp<SkImage> Scene2DContext::bakeScene3D(const world::Frame& frame,
                                           SkISize size,
                                           BakeScene3DOptions options) {
  // A scene3d sketch is drawn on a device and nowhere else.
  if (!runtimeAvailable()) return nullptr;
  sk_sp<SkSurface> surface = SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul(size.width(), size.height()));
  if (!surface) return nullptr;
  SkCanvas& canvas = *surface->getCanvas();
  canvas.clear(material::skia::toSkColor(options.background));
  // The caller's lens is written onto the frame rather than handed to the
  // draw, which is what lets a tree carrying its own camera win: forming
  // and presenting then read the ONE camera, and a tree holding none is
  // seen through the caller's.
  world::Frame framed = frame;
  framed.extent(glm::ivec2(size.width(), size.height()))
      .defaultCamera(options.camera)
      .runtime(runtime());
  // A frame whose passes paint no bodies — none at all, texture passes
  // alone, or a bloom alone — is drawn through the stock geometry pass,
  // put first so a post pass reads what it painted.
  throughPasses(framed, options.background);
  // A scene for this call, on a CLOCK OF ITS OWN: nothing here is
  // retained between bakes, so the picture is a function of the frame
  // and the moment and not of how many times the sketch has baked one —
  // and the moment is reached without touching the sketch's engine,
  // which a document baking a 3D frame into one of its panels must not move.
  motion::Engine engine;
  world::Scene scene(engine);
  // MOUNT, THEN MOVE. A node's entrance begins when it mounts, so the
  // motions the moment is measured against do not exist until the first
  // render; the second is what samples the lanes where the clock has
  // taken them.
  scene.render(framed);
  if (options.at > motion::Duration::zero()) {
    engine.advance(options.at);
    scene.render(framed);
  }
  if (!scene.error().empty()) return nullptr;
  world::draw(scene, canvas);
  return surface->makeImageSnapshot();
}

}  // namespace sigil::sketch
