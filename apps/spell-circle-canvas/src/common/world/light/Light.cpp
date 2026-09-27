/** @file
 * A light's angles as a direction in the world and back, the three
 * emitter factories, and the distance and cone falloffs a renderer
 * applies before any surface term.
 */

#include "sigilworld/light/Light.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>

namespace sigil::world::light {

namespace {

/** A unit vector, or +Y when the argument has no direction to speak
 *  of — a zero axis would otherwise put a NaN through every term
 *  downstream. */
glm::vec3 unit(const glm::vec3& v) {
  const float length = glm::length(v);
  return length < 1e-5f ? glm::vec3(0, 1, 0) : v / length;
}

/** The cosine and sine of @p degrees, exact on the quarter turns, so a
 *  light aimed straight down reads as straight down. */
glm::vec2 turn(float degrees) {
  const float wrapped = std::fmod(degrees, 360.0f);
  const float whole = wrapped < 0 ? wrapped + 360.0f : wrapped;
  if (whole == 0.0f) return {1, 0};
  if (whole == 90.0f) return {0, 1};
  if (whole == 180.0f) return {-1, 0};
  if (whole == 270.0f) return {0, -1};
  const float radians = glm::radians(degrees);
  return {std::cos(radians), std::sin(radians)};
}

}  // namespace

glm::vec3 travel(const material::Light& light) {
  const glm::vec2 bearing = turn(light.direction.value());
  const glm::vec2 height = turn(light.elevation.value());
  // Where the light comes FROM; it travels the other way.
  const glm::vec3 source{height.x * bearing.x, height.y, -height.x * bearing.y};
  return -source;
}

void aim(material::Light& light, glm::vec3 direction) {
  const glm::vec3 source = -unit(direction);
  // Straight up or down reads as exactly that, with no bearing to speak of.
  if (std::abs(source.x) < 1e-7f && std::abs(source.z) < 1e-7f) {
    light.elevation = source.y > 0 ? 90.0f : -90.0f;
    light.direction = 0.0f;
    return;
  }
  light.elevation =
      glm::degrees(std::asin(std::clamp(source.y, -1.0f, 1.0f)));
  light.direction = glm::degrees(std::atan2(-source.z, source.x));
}

material::Light sun(glm::vec3 direction, material::Color color,
                    float intensity) {
  material::Light light;
  light.kind = material::LightKind::Directional;
  aim(light, direction);
  light.color = color;
  light.intensity = intensity;
  return light;
}

material::Light point(glm::vec3 position, material::Color color,
                      float intensity, float range) {
  material::Light light;
  light.kind = material::LightKind::Point;
  aim(light, {0, -1, 0});
  light.position = position;
  light.color = color;
  light.intensity = intensity;
  light.range = range;
  return light;
}

material::Light spot(glm::vec3 position, glm::vec3 direction, float outerAngle,
                     float innerAngle, material::Color color, float intensity,
                     float range) {
  material::Light light = point(position, color, intensity, range);
  light.kind = material::LightKind::Spot;
  aim(light, direction);
  light.outerAngle = outerAngle;
  light.innerAngle = innerAngle;
  return light;
}

float attenuation(const material::Light& light, const glm::vec3& at) {
  if (light.kind == material::LightKind::Directional) return 1.0f;
  const glm::vec3 toLight = light.position - at;
  const float distance = std::max(glm::length(toLight), 1e-4f);
  const float reach =
      std::clamp(distance / std::max(light.range, 1e-3f), 0.0f, 1.0f);
  const float window = 1.0f - reach * reach;
  float falloff = window * window;
  if (light.kind == material::LightKind::Spot) {
    const float cosAngle = glm::dot(unit(travel(light)), unit(-toLight));
    const float outer =
        std::cos(glm::radians(std::max(light.outerAngle, 0.0f)));
    const float inner = std::cos(glm::radians(
        std::clamp(light.innerAngle, 0.0f, std::max(light.outerAngle, 0.0f))));
    const float span = std::max(inner - outer, 1e-4f);
    falloff *= std::clamp((cosAngle - outer) / span, 0.0f, 1.0f);
  }
  return falloff;
}

glm::vec3 radiance(const material::Light& light) {
  return glm::vec3(light.color.r, light.color.g, light.color.b) *
         light.intensity.value();
}

Directional directional(const material::Light& light) {
  Directional out;
  out.color = {light.color.r, light.color.g, light.color.b, 1.0f};
  out.intensity = light.intensity.value();
  if (light.kind == material::LightKind::Directional) {
    out.direction = travel(light);
    return out;
  }
  const glm::vec3 toward = -light.position;
  out.direction = glm::dot(toward, toward) > 0.0f ? glm::normalize(toward)
                                                  : travel(light);
  out.intensity = light.intensity.value() * attenuation(light, glm::vec3(0.0f));
  return out;
}

}  // namespace sigil::world::light
