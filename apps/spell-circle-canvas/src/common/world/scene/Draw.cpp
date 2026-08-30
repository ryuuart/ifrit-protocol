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

/** An emitter in the terms the CPU tier shades in, which are
 *  directional. A sun already is one. A light that stands somewhere
 *  reaches the tier as the direction from where it stands toward the
 *  origin, at the strength it has there; the full falloff is
 *  `light::attenuation`, which a device tier evaluates per pixel. */
render::Light directional(const Light& light) {
  render::Light out;
  out.color = SkColor4f{light.color.r, light.color.g, light.color.b, 1.0f};
  out.intensity = light.intensity;
  if (light.kind == light::Kind::Sun) {
    out.direction = light.direction;
    return out;
  }
  const glm::vec3 toward = -light.position;
  out.direction = glm::dot(toward, toward) > 0.0f ? glm::normalize(toward)
                                                  : light.direction;
  out.intensity = light.intensity * light::attenuation(light, glm::vec3(0.0f));
  return out;
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

  const SkISize layer = canvas.getBaseLayerSize();
  const SkSize viewport =
      SkSize::Make((float)layer.width(), (float)layer.height());

  render::MeshStyle style;
  style.runtime = runtime;
  if (!impl.lights.empty()) {
    style.lights.clear();
    for (const Light& light : impl.lights)
      style.lights.push_back(directional(light));
  }

  std::vector<Draw> bodies;
  impl.collectBodies(camera, bodies);
  for (const Draw& body : bodies) {
    style.baseColor = SkColor4f{body.baseColor.r, body.baseColor.g,
                                body.baseColor.b, body.baseColor.a};
    render::drawMesh(canvas, *body.mesh, body.world, camera, viewport, style);
  }
}

void Scene::draw(SkCanvas& canvas, const render::Runtime& runtime) {
  const std::optional<Camera> declared = camera();
  draw(canvas, declared ? *declared : m_impl->frame.camera(), runtime);
}

}  // namespace sigil::world
