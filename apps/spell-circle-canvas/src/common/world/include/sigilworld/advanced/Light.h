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

/** What a light is: which of the factories made it. */
enum class LightKind : uint8_t {
  Directional,  ///< a direction only, the same everywhere: the sun
  Point,        ///< a position, falling off with the square of distance
  Spot,         ///< a position and a direction, within a cone
  Ambient,      ///< the same radiance from every direction
  Hemisphere,   ///< a sky colour from above and a ground colour from below
};

/** WHAT A LIGHT NODE DESCRIBES, in the node's own space. The world is
 *  y-up; `direction` is the way the light TRAVELS, toward what it lights,
 *  and the node's rotation turns it — or, for a hemisphere light, the way
 *  UP is. Where the light stands is the node's placement and no field
 *  here. The fields a kind does not read keep their defaults and are
 *  ignored.
 *
 *  WHAT A LIGHT DELIVERS. `color` is a colour as an author types it —
 *  encoded for display, and decoded into linear light before it shines —
 *  and `intensity` scales it, without bound:
 *  - a directional light delivers `color * intensity` to a surface square
 *    to it, everywhere;
 *  - a point or spot light delivers `color * intensity / d²` at distance
 *    `d` — the inverse square — so its intensity is what it delivers at a
 *    distance of one unit, and `intensityAt` states one by what it
 *    delivers further off;
 *  - an ambient light is radiance arriving equally from every direction,
 *    and a hemisphere light is `color` from above the horizon its up
 *    names and `groundColor` from below.
 *  A white matte surface square to a directional light of intensity one
 *  shows one over pi of it, because a matte surface scatters what it
 *  receives over the whole hemisphere; under an ambient light of one it
 *  shows all of it. */
struct LightDescription {
  LightKind kind = LightKind::Directional;
  /** The way the light travels; a directional light and a spot read it,
   *  and a hemisphere light reads it as its up. */
  glm::vec3 direction = {0, -1, 0};
  /** The colour it shines in — a hemisphere light's sky. */
  motion::Animatable<material::Color> color = material::Color{1, 1, 1, 1};
  /** How strongly it shines, as a factor on its colour. */
  motion::Animatable<float> intensity = 1.0f;
  /** A hemisphere light's colour from below the horizon. */
  motion::Animatable<material::Color> groundColor = material::Color{0, 0, 0, 1};
  /** Point and spot: past zero, the distance at which a smooth window has
   *  brought the inverse square to nothing; zero reaches everywhere. */
  float range = 0;
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

/** How much of @p light's intensity reaches @p at before any surface
 *  term: one for a directional, ambient or hemisphere light, and for a
 *  point or spot light the inverse square of the distance, windowed to
 *  zero at its range when it has one — so it is NOT bounded by one — times
 *  a spot's cone. */
[[nodiscard]] float attenuation(const PlacedLight& light, const glm::vec3& at);

/** The light's colour, decoded, scaled by its intensity — what a renderer
 *  uploads as one value. */
[[nodiscard]] glm::vec3 radiance(const PlacedLight& light);

/** @p color decoded from the display encoding an author types it in into
 *  the linear light it shines with. Above one it carries on along the same
 *  curve. */
[[nodiscard]] glm::vec3 linearColor(const material::Color& color);

/** The inverse of `linearColor`: linear light encoded as an author would
 *  type it, opaque — what a light read from a format that stores linear
 *  colour is described with. */
[[nodiscard]] material::Color encodedColor(const glm::vec3& linear);

}  // namespace sigil::world
