/** @file
 * The light a lit surface is shaded under: what moves, what compares
 * equal, and an environment made from any material.
 */

#include "sigilmaterial/core/Lighting.h"

#include <sigilmaterial/core/Material.h>

#include <utility>

namespace sigil::material {

bool Light::isRunning() const {
  return direction.isRunning() || elevation.isRunning() ||
         intensity.isRunning();
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
  return (light && light->isRunning()) ||
         (environment && environment->isRunning());
}

}  // namespace sigil::material
