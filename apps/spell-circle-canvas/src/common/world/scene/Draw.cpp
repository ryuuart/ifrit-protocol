/** @file
 * The draw: what the last extract left in the registry, sorted back to
 * front and handed to the mesh runtime. It reads components and nothing
 * else — the Element tree is not reachable from here.
 */

#include <include/core/SkCanvas.h>
#include <sigilgeometry/mesh/render/Painter.h>

#include <algorithm>
#include <utility>
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

  // Back to front by view depth, so a nearer body covers a farther one
  // without a depth buffer. Stable, because two bodies at one depth must
  // land in tree order rather than in whichever order a sort happened to
  // leave them — a byte-identity gate reads the difference.
  const glm::mat4 view = camera.view();
  std::vector<std::pair<float, entt::entity>> sorted;
  sorted.reserve(impl.order.size());
  for (entt::entity entity : impl.order) {
    const glm::mat4& world =
        impl.registry.get<component::Placement>(entity).world;
    const glm::vec4 centre = view * world * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    sorted.emplace_back(centre.z, entity);
  }
  std::stable_sort(sorted.begin(), sorted.end(),
                   [](const std::pair<float, entt::entity>& a,
                      const std::pair<float, entt::entity>& b) {
                     return a.first < b.first;
                   });

  for (const auto& [depth, entity] : sorted) {
    const component::Body& body = impl.registry.get<component::Body>(entity);
    if (!body.mesh) continue;
    const glm::vec4& colour =
        impl.registry.get<component::Surface>(entity).baseColor;
    style.baseColor = SkColor4f{colour.r, colour.g, colour.b, colour.a};
    render::drawMesh(canvas, *body.mesh,
                     impl.registry.get<component::Placement>(entity).world,
                     camera, viewport, style);
  }
}

void Scene::draw(SkCanvas& canvas, const render::Runtime& runtime) {
  const std::optional<Camera> declared = camera();
  draw(canvas, declared ? *declared : Camera{}, runtime);
}

}  // namespace sigil::world
