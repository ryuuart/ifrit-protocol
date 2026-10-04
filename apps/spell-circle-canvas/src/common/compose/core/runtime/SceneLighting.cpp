/** @file
 * Mounted source declarations resolve after layout and before paint, so
 * a recording can never hide a source from the receivers beside it.
 */

#include <algorithm>
#include <cmath>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

namespace {

float finite(float value) { return std::isfinite(value) ? value : 0.0f; }

bool enabled(const Instance& source) {
  for (const Instance* node = &source; node; node = node->parent)
    if (node->computed.layout.display == Display::None) return false;
  return true;
}

material::Light sampled(const material::Light& source) {
  // Construct a detached frame value without copying the author's live
  // cell blocks or writing sanitized readings back into them.
  material::Light light;
  light.kind = source.kind;
  light.position = source.position;
  light.range = source.range;
  float intensity = finite(source.intensity.value());
  if (source.kind == material::LightKind::Point) {
    light.direction = motion::Animatable<float>(0.0f);
    light.elevation = motion::Animatable<float>(0.0f);
  } else {
    const float direction = source.direction.value();
    const float elevation = source.elevation.value();
    light.direction = motion::Animatable<float>(finite(direction));
    light.elevation = motion::Animatable<float>(finite(elevation));
    if (!std::isfinite(direction) || !std::isfinite(elevation)) intensity = 0;
  }
  light.intensity = motion::Animatable<float>(intensity);
  light.ambient = finite(source.ambient);
  const material::Color color = source.color.value();
  light.color = motion::Animatable<material::Color>(material::Color{
      finite(color.r), finite(color.g), finite(color.b), finite(color.a)});
  light.innerAngle = finite(source.innerAngle);
  light.outerAngle = finite(source.outerAngle);
  return light;
}

}  // namespace

void Composer::Impl::resolveSceneLighting() {
  for (Instance* scope : sceneInstances) {
    material::Lighting& resolved = sceneLightingScratch;
    resolved.frame = material::LightingFrame::Scene;
    resolved.lights.clear();
    resolved.environment.reset();
    if (scope->description->sceneData)
      resolved.environment = scope->description->sceneData->environment;
    resolved.lights.reserve(scope->lightSources.size());
    for (Instance* source : scope->lightSources) {
      const SceneData* data = source->description->sceneData
                                  ? &*source->description->sceneData
                                  : nullptr;
      if (!data || !data->source || !enabled(*source)) continue;
      material::Light light = sampled(*data->source);
      const SkMatrix world = worldMatrixOf(*source);
      float z = light.position.z;
      float zScale = 1.0f;
      bool planar =
          !world.hasPerspective() && world.isFinite() && world.invert(nullptr);
      for (Instance* node = source; node; node = node->parent) {
        const NodeTransform tf = transformOf(*node);
        z = tf.tz + tf.pivot.depth * (1.0f - tf.sz) + tf.sz * z;
        zScale *= tf.sz;
        planar &= tf.rx == 0 && tf.ry == 0;
        if (node->description->depthData)
          planar &= node->resolveFloat(
                        Instance::kPerspective,
                        node->description->depthData->perspective) <= 0;
      }
      if (light.kind != material::LightKind::Point &&
          (world.getScaleX() != 1 || world.getScaleY() != 1 ||
           world.getSkewX() != 0 || world.getSkewY() != 0 || zScale != 1)) {
        constexpr float radians = 3.14159265f / 180.0f;
        const float azimuth = light.direction.value() * radians;
        const float elevation = light.elevation.value() * radians;
        const float x = std::cos(elevation) * std::cos(azimuth);
        const float y = -std::cos(elevation) * std::sin(azimuth);
        const float axisX = world.getScaleX() * x + world.getSkewX() * y;
        const float axisY = world.getSkewY() * x + world.getScaleY() * y;
        const float axisZ = zScale * std::sin(elevation);
        light.direction = finite(std::atan2(-axisY, axisX) / radians);
        light.elevation =
            finite(std::atan2(axisZ, std::hypot(axisX, axisY)) / radians);
        planar &= std::isfinite(axisX) && std::isfinite(axisY) &&
                  std::isfinite(axisZ) &&
                  (axisX != 0 || axisY != 0 || axisZ != 0);
      }
      if (!planar) light.intensity = 0.0f;
      if (light.kind != material::LightKind::Directional) {
        const SkPoint xy = world.mapPoint({light.position.x, light.position.y});
        light.position = {xy.x(), xy.y(), z};
        if (!planar || !std::isfinite(xy.x()) || !std::isfinite(xy.y()) ||
            !std::isfinite(z) || !std::isfinite(light.range)) {
          light.position = {0, 0, 0};
          light.range = 0;
        }
      }
      resolved.lights.push_back(std::move(light));
    }
    if (scope->sceneLighting && *scope->sceneLighting == resolved) continue;
    scope->sceneLighting = std::make_shared<const material::Lighting>(resolved);
    cascadeDirty = true;
    volatileDirty = true;
    contentDirty = true;
  }
}

}  // namespace sigil::compose
