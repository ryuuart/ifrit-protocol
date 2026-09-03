/** @file
 * What a canvas sketch's context hands it beyond its declarations: a
 * compose scene kept as a texture, and a lit set baked to an image.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilworld/frame/Frame.h>
#include <sigilworld/scene/Scene.h>

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

sk_sp<SkImage> SketchContext::bakeSet(
    const world::Frame& frame, const geometry::mesh::camera::Camera& camera,
    SkISize size, SkColor4f background) {
  sk_sp<SkSurface> surface = SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul(size.width(), size.height()));
  if (!surface) return nullptr;
  SkCanvas& canvas = *surface->getCanvas();
  canvas.clear(background);
  // The caller's viewpoint is written onto the frame rather than handed
  // to the draw, which is what lets a tree carrying its own camera win:
  // forming and presenting then read the ONE viewpoint, and a frame that
  // named none is seen from the caller's.
  world::Frame framed = frame;
  framed.extent(size).camera(camera);
  // A scene for this call: nothing here is retained between bakes, so
  // the picture is a function of the frame and not of how many times the
  // sketch has baked one.
  world::Scene scene(ticker);
  scene.render(framed);
  scene.draw(canvas);
  return surface->makeImageSnapshot();
}

}  // namespace sigil::sketch
