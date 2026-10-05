/** @file
 * A light held still, its falloffs, and the three factories that make a
 * light node.
 */

#include <sigilworld/advanced/Light.h>
#include <sigilworld/node/Light.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <utility>

namespace sigil::world {

namespace {

/** A unit vector, or +Y when the argument has no direction to speak
 *  of — a zero axis would otherwise put a NaN through every term
 *  downstream. */
glm::vec3 unit(const glm::vec3& v) {
  const float length = glm::length(v);
  return length < 1e-5f ? glm::vec3(0, 1, 0) : v / length;
}

/** A light node of @p kind carrying @p light. */
Light lightNode(LightDescription light) {
  Light node;
  node.edit().light = std::move(light);
  return node;
}

}  // namespace

LightDescription held(const LightDescription& light) {
  LightDescription out = light;
  // Each animatable REPLACED by a constant rather than assigned a number:
  // assigning to a live value writes the cell every copy of it shares,
  // the source's included.
  out.color = motion::Animatable<material::Color>(light.color.value());
  out.intensity = motion::Animatable<float>(light.intensity.value());
  return out;
}

float attenuation(const PlacedLight& placed, const glm::vec3& at) {
  const LightDescription& light = placed.light;
  if (light.kind == LightKind::Directional) return 1.0f;
  const glm::vec3 toLight = placed.position - at;
  const float distance = std::max(glm::length(toLight), 1e-4f);
  const float reach =
      std::clamp(distance / std::max(light.range, 1e-3f), 0.0f, 1.0f);
  const float window = 1.0f - reach * reach;
  float falloff = window * window;
  if (light.kind == LightKind::Spot) {
    const float cosAngle = glm::dot(unit(light.direction), unit(-toLight));
    const float outer =
        std::cos(glm::radians(std::max(light.outerAngle, 0.0f)));
    const float inner = std::cos(glm::radians(
        std::clamp(light.innerAngle, 0.0f, std::max(light.outerAngle, 0.0f))));
    const float span = std::max(inner - outer, 1e-4f);
    falloff *= std::clamp((cosAngle - outer) / span, 0.0f, 1.0f);
  }
  return falloff;
}

glm::vec3 radiance(const PlacedLight& placed) {
  const material::Color color = placed.light.color.value();
  return glm::vec3(color.r, color.g, color.b) * placed.light.intensity.value();
}

Light directionalLight(glm::vec3 direction, DirectionalLightOptions options) {
  return lightNode({.kind = LightKind::Directional,
                    .direction = direction,
                    .color = std::move(options.color),
                    .intensity = std::move(options.intensity)});
}

Light pointLight(PointLightOptions options) {
  return lightNode({.kind = LightKind::Point,
                    .color = std::move(options.color),
                    .intensity = std::move(options.intensity),
                    .range = options.range});
}

Light spotLight(glm::vec3 direction, SpotLightOptions options) {
  return lightNode({.kind = LightKind::Spot,
                    .direction = direction,
                    .color = std::move(options.color),
                    .intensity = std::move(options.intensity),
                    .range = options.range,
                    .innerAngle = options.innerAngle,
                    .outerAngle = options.outerAngle});
}

Light& Light::lightLink(Collection bodies) {
  if (auto& light = edit().light) light->lightLink = std::move(bodies);
  return *this;
}

Light& Light::lightLink(std::string name) {
  return lightLink(Collection{.include = {std::move(name)}});
}

Light& Light::shadowLink(Collection blockers) {
  if (auto& light = edit().light) light->shadowLink = std::move(blockers);
  return *this;
}

Light& Light::shadowLink(std::string name) {
  return shadowLink(Collection{.include = {std::move(name)}});
}

const LightDescription& Light::description() const {
  static const LightDescription none;
  const NodeDescription* described = node().get();
  return described && described->light ? *described->light : none;
}

}  // namespace sigil::world
