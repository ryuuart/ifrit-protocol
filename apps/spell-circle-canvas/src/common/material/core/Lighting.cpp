/** @file
 * The light a lit surface is shaded under: what moves, what compares
 * equal, what depends on placement, and an environment made from any
 * material.
 */

#include "sigilmaterial/core/Lighting.h"

#include <sigilmaterial/core/Material.h>

#include <algorithm>
#include <utility>

namespace sigil::material {

bool Light::isRunning() const {
  return color.isRunning() || intensity.isRunning() ||
         (kind != LightKind::Point &&
          (direction.isRunning() || elevation.isRunning()));
}

bool Environment::isRunning() const {
  return options.rotation.isRunning() || (image && image->isRunning());
}

bool Environment::operator==(const Environment& other) const {
  if (!(options == other.options)) return false;
  if (image == other.image) return true;
  return image && other.image && *image == *other.image;
}

Environment environment(Material image, EnvironmentOptions options) {
  return {std::make_shared<const Material>(std::move(image)),
          std::move(options)};
}

bool Lighting::isRunning() const {
  return std::ranges::any_of(lights, &Light::isRunning) ||
         (environment && environment->isRunning());
}

bool Lighting::dependsOnPlacement() const {
  return frame == LightingFrame::Scene ||
         std::ranges::any_of(lights, [](const Light& light) {
           return light.kind != LightKind::Directional;
         });
}

}  // namespace sigil::material
