/** @file
 * A light held still, its falloffs, the factories that make a light node,
 * and the intensity that delivers a strength at a distance.
 */

#include <sigilmaterial/advanced/Program.h>  // reportOnce
#include <sigilmotion/bind/Binding.h>
#include <sigilworld/advanced/Light.h>
#include <sigilworld/node/Light.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <string>
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
  out.groundColor =
      motion::Animatable<material::Color>(light.groundColor.value());
  return out;
}

std::vector<size_t> shadowCasters(std::span<const PlacedLight> lights) {
  std::vector<size_t> out;
  for (size_t i = 0; i < lights.size(); ++i) {
    const LightDescription& light = lights[i].light;
    if (!light.castShadow) continue;
    if (light.kind == LightKind::Ambient || light.kind == LightKind::Hemisphere)
      continue;
    if (out.size() >= kShadowCasters) {
      material::reportOnce(
          "world.shadow.budget",
          "more than " + std::to_string(kShadowCasters) +
              " lights ask to cast shadows in one frame; the ones past that "
              "many, in the order the frame placed them, cast nothing");
      continue;
    }
    out.push_back(i);
  }
  return out;
}

/** The device's `rangeWindow`: one minus the fourth power of @p distance
 *  over @p range, squared; one where there is no range. */
static float rangeWindow(float distance, float range) {
  if (range <= 0.0f) return 1.0f;
  const float reach = std::max(distance, 1e-2f) / range;
  const float window =
      std::clamp(1.0f - reach * reach * reach * reach, 0.0f, 1.0f);
  return window * window;
}

float attenuation(const PlacedLight& placed, const glm::vec3& at) {
  const LightDescription& light = placed.light;
  const glm::vec3 toLight = placed.position - at;
  if (light.kind == LightKind::Rect || light.kind == LightKind::Disk) {
    const float cosFace = glm::dot(unit(light.direction), unit(-toLight));
    if (cosFace <= 0.0f) return 0.0f;
    const float squared = std::max(glm::dot(toLight, toLight), 1e-4f);
    const float window = rangeWindow(std::sqrt(squared), light.range);
    if (light.kind == LightKind::Disk)
      return window * cosFace / (squared + light.radius * light.radius);
    return window * light.width * light.height * cosFace / squared;
  }
  if (light.kind != LightKind::Point && light.kind != LightKind::Spot &&
      light.kind != LightKind::Sphere)
    return 1.0f;
  // The same arithmetic the device's `distanceFalloff` and `coneFalloff`
  // do, so a host reading of a light and the shaded picture agree.
  const float distance = std::max(glm::length(toLight), 1e-2f);
  float falloff = 1.0f / (distance * distance);
  if (light.range > 0.0f) {
    const float reach = distance / light.range;
    const float window =
        std::clamp(1.0f - reach * reach * reach * reach, 0.0f, 1.0f);
    falloff *= window * window;
  }
  if (light.kind == LightKind::Spot) {
    const float cosAngle = glm::dot(unit(light.direction), unit(-toLight));
    const float outer = std::max(light.outerAngle, 0.0f);
    const float cosOuter = std::cos(glm::radians(outer));
    const float cosInner =
        std::cos(glm::radians(std::clamp(light.innerAngle, 0.0f, outer)));
    const float span = std::max(cosInner - cosOuter, 1e-4f);
    const float t = std::clamp((cosAngle - cosOuter) / span, 0.0f, 1.0f);
    falloff *= t * t;
  }
  return falloff;
}

glm::vec3 linearColor(const material::Color& color) {
  const auto decode = [](float c) {
    const float x = std::max(c, 0.0f);
    return x <= 0.04045f ? x / 12.92f : std::pow((x + 0.055f) / 1.055f, 2.4f);
  };
  return {decode(color.r), decode(color.g), decode(color.b)};
}

material::Color encodedColor(const glm::vec3& linear) {
  const auto encode = [](float l) {
    const float x = std::max(l, 0.0f);
    return x <= 0.0031308f ? x * 12.92f
                           : 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
  };
  return {encode(linear.r), encode(linear.g), encode(linear.b), 1.0f};
}

glm::vec3 radiance(const PlacedLight& placed) {
  return linearColor(placed.light.color.value()) *
         placed.light.intensity.value();
}

float intensityAt(float strength, float distance) {
  return strength * distance * distance;
}

motion::Animatable<float> intensityAt(motion::Animatable<float> strength,
                                      float distance) {
  const float scale = distance * distance;
  if (const float* constant = strength.constant()) return *constant * scale;
  // A moving strength is followed through a linear stage that takes
  // [0, 1] to [0, d²] — the scale, applied to every sample it reads.
  return motion::bind(strength, motion::Binding{.to = {0.0f, scale}});
}

Light directionalLight(glm::vec3 direction, DirectionalLightOptions options) {
  return lightNode({.kind = LightKind::Directional,
                    .direction = direction,
                    .color = std::move(options.color),
                    .intensity = std::move(options.intensity),
                    .castShadow = options.castShadow,
                    .shadow = options.shadow});
}

Light pointLight(PointLightOptions options) {
  return lightNode({.kind = LightKind::Point,
                    .color = std::move(options.color),
                    .intensity = std::move(options.intensity),
                    .range = options.range,
                    .castShadow = options.castShadow,
                    .shadow = options.shadow});
}

Light spotLight(glm::vec3 direction, SpotLightOptions options) {
  return lightNode({.kind = LightKind::Spot,
                    .direction = direction,
                    .color = std::move(options.color),
                    .intensity = std::move(options.intensity),
                    .range = options.range,
                    .innerAngle = options.innerAngle,
                    .outerAngle = options.outerAngle,
                    .castShadow = options.castShadow,
                    .shadow = options.shadow});
}

Light rectLight(glm::vec3 direction, RectLightOptions options) {
  return lightNode({.kind = LightKind::Rect,
                    .direction = direction,
                    .color = std::move(options.color),
                    .intensity = std::move(options.intensity),
                    .range = options.range,
                    .width = std::max(options.width, 0.0f),
                    .height = std::max(options.height, 0.0f),
                    .castShadow = options.castShadow,
                    .shadow = options.shadow});
}

Light sphereLight(SphereLightOptions options) {
  return lightNode({.kind = LightKind::Sphere,
                    .color = std::move(options.color),
                    .intensity = std::move(options.intensity),
                    .range = options.range,
                    .radius = std::max(options.radius, 0.0f),
                    .castShadow = options.castShadow,
                    .shadow = options.shadow});
}

Light diskLight(glm::vec3 direction, DiskLightOptions options) {
  return lightNode({.kind = LightKind::Disk,
                    .direction = direction,
                    .color = std::move(options.color),
                    .intensity = std::move(options.intensity),
                    .range = options.range,
                    .radius = std::max(options.radius, 0.0f),
                    .castShadow = options.castShadow,
                    .shadow = options.shadow});
}

Light ambientLight(AmbientLightOptions options) {
  return lightNode({.kind = LightKind::Ambient,
                    .color = std::move(options.color),
                    .intensity = std::move(options.intensity)});
}

Light hemisphereLight(HemisphereLightOptions options) {
  return lightNode({.kind = LightKind::Hemisphere,
                    .direction = {0, 1, 0},
                    .color = std::move(options.color),
                    .intensity = std::move(options.intensity),
                    .groundColor = std::move(options.groundColor)});
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
