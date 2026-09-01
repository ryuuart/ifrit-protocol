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

#include <optional>
#include <vector>

#include "SceneImpl.h"

namespace sigil::world {

namespace {

/** An emitter as the mesh painter takes it: the one directional reading
 *  every tier that shades without a per-pixel position works from. */
render::Light painterLight(const Light& light) {
  const light::Directional value = light::directional(light);
  render::Light out;
  out.direction = value.direction;
  out.color = SkColor4f{value.color.r, value.color.g, value.color.b, 1.0f};
  out.intensity = value.intensity;
  return out;
}

/** The map a body is dressed with and whether the emitters reach it, put
 *  on the style — and taken off it again for a body carrying neither,
 *  since one style is reused across the whole list. */
void dress(render::MeshStyle& style, const Draw& body) {
  const Sampling sampling =
      body.texture ? samplingOf(*body.texture) : Sampling{};
  style.texture = sampling.image;
  style.uvTransform = sampling.uv;
  style.tileTexture = sampling.tile;
  style.filter = sampling.filter;
  style.lit = body.lit;
}

}  // namespace

void Scene::draw(SkCanvas& canvas, const Camera& camera,
                 const render::Runtime& runtime) {
  Impl& impl = *m_impl;
  // A frame with passes has already been performed, from the viewpoint
  // the tree or the frame declared; presenting it is the whole of the
  // draw, and the arguments here do not enter into it.
  if (!impl.frame.passes().empty()) {
    if (impl.plan.present().empty()) return;
    const sk_sp<SkImage> picture = impl.targets.image(impl.plan.present());
    if (picture) canvas.drawImage(picture, 0, 0);
    return;
  }

  // The viewport is where the projection LANDS, in the canvas's OWN
  // coordinates — so it is the extent the frame was formed at, and not
  // the surface standing behind the canvas. A caller that fitted the
  // picture into something larger left a transform on the canvas, and
  // reading the projection off the surface instead would magnify the
  // scene by that fit and carry most of it off its own edge. A frame
  // that declared no extent has said nothing, and the surface is then
  // the only size there is.
  const SkISize declared = impl.frame.extent();
  const SkISize layer =
      declared.isEmpty() ? canvas.getBaseLayerSize() : declared;
  const SkSize viewport =
      SkSize::Make((float)layer.width(), (float)layer.height());

  render::MeshStyle style;
  style.runtime = runtime;
  if (!impl.lights.empty()) {
    style.lights.clear();
    for (const Light& light : impl.lights)
      style.lights.push_back(painterLight(light));
  }

  std::vector<Draw> bodies;
  impl.collectBodies(camera, bodies);
  for (const Draw& body : bodies) {
    style.baseColor = SkColor4f{body.baseColor.r, body.baseColor.g,
                                body.baseColor.b, body.baseColor.a};
    dress(style, body);
    render::drawMesh(canvas, *body.mesh, body.world, camera, viewport, style);
  }
}

void Scene::draw(SkCanvas& canvas, const render::Runtime& runtime) {
  const std::optional<Camera> declared = camera();
  draw(canvas, declared ? *declared : m_impl->frame.camera(), runtime);
}

}  // namespace sigil::world
