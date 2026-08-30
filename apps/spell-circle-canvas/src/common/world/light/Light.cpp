/** @file
 * The three emitter factories and the distance and cone falloffs a
 * renderer applies before any surface term.
 */

#include "sigilworld/light/Light.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

namespace sigil::world::light {

namespace {

/** A unit vector, or +Y when the argument has no direction to speak
 *  of — a zero axis would otherwise put a NaN through every term
 *  downstream. */
glm::vec3 unit(const glm::vec3& v) {
  const float length = glm::length(v);
  return length < 1e-5f ? glm::vec3(0, 1, 0) : v / length;
}

}  // namespace

Light sun(glm::vec3 direction, glm::vec4 color, float intensity) {
  Light light;
  light.kind = Kind::Sun;
  light.direction = direction;
  light.color = color;
  light.intensity = intensity;
  return light;
}

Light point(glm::vec3 position, glm::vec4 color, float intensity, float range) {
  Light light;
  light.kind = Kind::Point;
  light.position = position;
  light.color = color;
  light.intensity = intensity;
  light.range = range;
  return light;
}

Light spot(glm::vec3 position, glm::vec3 direction, float outerDeg,
           float innerDeg, glm::vec4 color, float intensity, float range) {
  Light light = point(position, color, intensity, range);
  light.kind = Kind::Spot;
  light.direction = direction;
  light.outerDeg = outerDeg;
  light.innerDeg = innerDeg;
  return light;
}

float attenuation(const Light& light, const glm::vec3& at) {
  if (light.kind == Kind::Sun) return 1.0f;
  const glm::vec3 toLight = light.position - at;
  const float distance = std::max(glm::length(toLight), 1e-4f);
  const float x =
      std::clamp(distance / std::max(light.range, 1e-3f), 0.0f, 1.0f);
  const float window = 1.0f - x * x;
  float atten = window * window;
  if (light.kind == Kind::Spot) {
    const float cosAngle = glm::dot(unit(light.direction), unit(-toLight));
    const float outer = std::cos(glm::radians(std::max(light.outerDeg, 0.0f)));
    const float inner = std::cos(glm::radians(
        std::clamp(light.innerDeg, 0.0f, std::max(light.outerDeg, 0.0f))));
    const float span = std::max(inner - outer, 1e-4f);
    atten *= std::clamp((cosAngle - outer) / span, 0.0f, 1.0f);
  }
  return atten;
}

glm::vec3 radiance(const Light& light) {
  return glm::vec3(light.color) * light.intensity;
}

}  // namespace sigil::world::light
