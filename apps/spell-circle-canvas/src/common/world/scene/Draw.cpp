/** @file
 * The draw: what the last render left, put on a canvas. A frame that
 * declared passes has already run them and the picture is one of its
 * resources, so the draw is a blit; a frame with no passes is its scene,
 * and the draw paints the bodies extract left. Either way it reads
 * components and images — the Element tree is not reachable from here.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkSamplingOptions.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilworld/advanced/Skia.h>

#include <optional>
#include <vector>

#include "SceneImpl.h"

namespace sigil::world {

void draw(Scene& scene, SkCanvas& canvas,
          const geometry::mesh::camera::Camera& camera,
          const geometry::mesh::render::Runtime& runtime) {
  Scene::Impl& impl = SceneAccess::of(scene);
  // A frame with passes has already been performed, from the viewpoint
  // the tree or the frame declared; presenting it is the whole of the
  // draw, and the arguments here do not enter into it.
  if (!impl.frame.passes().empty()) {
    if (impl.plan.present().empty()) return;
    const sk_sp<SkImage> picture = impl.targets.image(impl.plan.present());
    // A frame formed over a PART of its picture lands where that part
    // stands in it, so the canvas is addressed in the whole picture's
    // pixels either way.
    const ViewOffset& offset = impl.frame.viewOffset();
    const glm::ivec2 corner = isPart(offset, impl.frame.extent())
                                  ? offset.origin
                                  : glm::ivec2{0, 0};
    if (picture) canvas.drawImage(picture, (float)corner.x, (float)corner.y);
    return;
  }

  // The viewport is where the projection LANDS, in the canvas's OWN
  // coordinates — so it is the extent the frame was formed at, and not
  // the surface standing behind the canvas. A caller that fitted the
  // picture into something larger left a transform on the canvas, and
  // reading the projection off the surface instead would magnify the
  // scene by that fit and carry most of it off its own edge. A frame
  // that declared no extent has said nothing, and the surface is then
  // the only size there is. A frame with no passes forms no targets, so
  // a part of a picture is nothing to it: it is drawn over the whole one
  // and the canvas's clip leaves what the part would have.
  const glm::ivec2 declared =
      isPart(impl.frame.viewOffset(), impl.frame.extent())
          ? impl.frame.viewOffset().whole
          : impl.frame.extent();
  const SkISize layer = declared.x <= 0 || declared.y <= 0
                            ? canvas.getBaseLayerSize()
                            : toSk(declared);
  const glm::vec2 viewport{(float)layer.width(), (float)layer.height()};

  geometry::mesh::render::MeshStyle style;
  style.runtime = runtime;
  if (!impl.lights.empty()) {
    style.lights.clear();
    for (const material::Light& light : impl.lights)
      style.lights.push_back(painterLight(light));
  }
  style.environment =
      paintedEnvironment(impl.environment, impl.environmentOrientation);

  // THE SKY FIRST, where the set shows one: it stands behind every body
  // in the frame.
  geometry::mesh::render::drawBackdrop(
      canvas, style.environment,
      camera.projection(
          viewport.x > 0 ? viewport.x / viewport.y : 1.0f),
      camera.view(), viewport);

  std::vector<Draw> bodies;
  impl.collectBodies(camera, bodies);
  for (const Draw& body : bodies) {
    style.baseColor = body.baseColor;
    dress(style, body);
    geometry::mesh::render::drawMesh(canvas, *body.mesh, body.world, camera,
                                     viewport, style);
  }
}

void draw(Scene& scene, SkCanvas& canvas,
          const geometry::mesh::render::Runtime& runtime) {
  const std::optional<geometry::mesh::camera::Camera> declared =
      scene.camera();
  draw(scene, canvas,
       declared ? *declared : SceneAccess::of(scene).frame.camera(), runtime);
}

}  // namespace sigil::world
