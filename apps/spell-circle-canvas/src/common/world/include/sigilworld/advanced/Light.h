#pragma once

/** @file
 * @ingroup world-node
 * A light as values: what a light node describes, the reading a frame
 * takes of it once the node has placed it, and the distance and cone
 * falloffs an executor applies before any surface term. Nothing here
 * renders, uploads or holds a device; the node that carries a light is
 * `world::Light`, in `<sigilworld/node/Light.h>`.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilworld/node/Collection.h>

#include <cstdint>
#include <glm/vec3.hpp>

namespace sigil::world {

/** What a light is: which of the three factories made it. */
enum class LightKind : uint8_t {
  Directional,  ///< a direction only, the same everywhere: the sun
  Point,        ///< a position, falling off to nothing at its range
  Spot,         ///< a position and a direction, within a cone
};

/** WHAT A LIGHT NODE DESCRIBES, in the node's own space. The world is
 *  y-up; `direction` is the way the light TRAVELS, toward what it lights,
 *  and the node's rotation turns it. Where the light stands is the node's
 *  placement and no field here. A light's colour is read as linear
 *  radiance. The fields a kind does not read keep their defaults and are
 *  ignored. */
struct LightDescription {
  LightKind kind = LightKind::Directional;
  /** The way the light travels; a directional light and a spot read it,
   *  a point light does not. */
  glm::vec3 direction = {0, -1, 0};
  /** The colour it shines in, including values above one. */
  motion::Animatable<material::Color> color = material::Color{1, 1, 1, 1};
  /** How strongly it shines, as a factor on its colour. */
  motion::Animatable<float> intensity = 1.0f;
  /** Point and spot: the distance at which the light has fallen to
   *  nothing. */
  float range = 600;
  /** Spot: full strength within `innerAngle` degrees of where it aims,
   *  dark beyond `outerAngle`, interpolated between. */
  float innerAngle = 0;
  float outerAngle = 45;
  /** THE BODIES IT LIGHTS — USD's light link. The whole scene unless it
   *  says otherwise. */
  Collection lightLink;
  /** THE BODIES THAT BLOCK IT — USD's shadow link. Stored, compared and
   *  written out; nothing casts a shadow yet, so nothing reads it. */
  Collection shadowLink;

  bool operator==(const LightDescription&) const = default;
};

/** A LIGHT AS ONE FRAME READ IT: its description with each animatable
 *  read once and held, its direction carried into the world by the node
 *  that placed it, and the point in the world it stands at. */
struct PlacedLight {
  LightDescription light;
  /** Where the node placed it, in world units; a directional light stands
   *  nowhere and ignores it. */
  glm::vec3 position = {0, 0, 0};
  /** The key of the node that carried it. */
  std::string key;

  bool operator==(const PlacedLight&) const = default;
};

/** @p light with each animatable read once and held as that constant, so
 *  nothing written to a live value afterwards reaches the copy. */
[[nodiscard]] LightDescription held(const LightDescription& light);

/** How much of @p light reaches @p at, in [0, 1], before any surface
 *  term. A directional light reaches everything equally; a point light
 *  falls off on a window rather than an inverse square, reaching exactly
 *  zero at its range; a spot multiplies that by its cone. */
[[nodiscard]] float attenuation(const PlacedLight& light, const glm::vec3& at);

/** The light's colour scaled by its intensity — what a renderer uploads
 *  as one value. */
[[nodiscard]] glm::vec3 radiance(const PlacedLight& light);

}  // namespace sigil::world
