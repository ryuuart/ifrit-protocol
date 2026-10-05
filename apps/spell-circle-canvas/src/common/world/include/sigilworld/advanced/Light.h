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

#include <cstddef>
#include <cstdint>
#include <glm/vec3.hpp>
#include <span>
#include <vector>

namespace sigil::world {

/** What a light is: which of the factories made it. */
enum class LightKind : uint8_t {
  Directional,  ///< a direction only, the same everywhere: the sun
  Point,        ///< a position, falling off with the square of distance
  Spot,         ///< a position and a direction, within a cone
  Ambient,      ///< the same radiance from every direction
  Hemisphere,   ///< a sky colour from above and a ground colour from below
};

/** HOW A LIGHT'S SHADOW IS MADE, once the light casts one. Every length
 *  here is counted in TEXELS of the light's shadow map — the width one
 *  texel covers where the shaded point stands — so the same numbers serve
 *  a set measured in centimetres and one measured in metres.
 *
 *  - `bias` is how far nearer the light a stored depth must stand before
 *    it shadows a point, beside a share that grows as the surface turns
 *    away from the light, so a lit face does not shadow itself;
 *  - `normalBias` is how far a point is moved out along its surface's
 *    normal before it is looked up, which is what keeps a face grazed by
 *    the light free of stripes without lifting a shadow off its caster;
 *  - `radius` is how far apart the filter's taps stand, so a larger one is
 *    a softer edge;
 *  - `mapSize` is how many texels a side the map has, at most 2048;
 *  - `distance`, for a directional light, is how far in front of the
 *    camera its map reaches; zero fits the map to every body in view;
 *  - `cascades`, for a directional light, is how many maps that reach is
 *    split into along the camera's depth, at most four: each nearer one
 *    covers less of the set at the same size, so a shadow near the camera
 *    is sharper. One is a single map for the whole reach. */
struct LightShadow {
  float bias = 1.0f;
  float normalBias = 1.0f;
  float radius = 1.0f;
  int mapSize = 2048;
  float distance = 0.0f;
  int cascades = 1;

  bool operator==(const LightShadow&) const = default;
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
  /** THE BODIES THAT BLOCK IT — USD's shadow link: of the bodies that
   *  cast, the ones that cast THIS light's shadow. The whole scene unless
   *  it says otherwise. */
  Collection shadowLink;
  /** WHETHER IT CASTS SHADOWS. Off unless stated, so a set written without
   *  shadows looks the same. A directional, a spot and a point light cast;
   *  an ambient light and a hemisphere light cast nothing. */
  bool castShadow = false;
  /** How its shadow is made, when it casts one. */
  LightShadow shadow;

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

/** HOW MANY LIGHTS CAST A SHADOW IN ONE FRAME. A light that asks past
 *  this many casts nothing, and says so once. */
constexpr size_t kShadowCasters = 4;

/** THE LIGHTS OF @p lights THAT CAST A SHADOW this frame, as their
 *  positions in @p lights: every directional, spot and point light that
 *  states `castShadow`, in the order the frame placed them, up to
 *  `kShadowCasters`. One past the budget casts nothing, and says so once. */
[[nodiscard]] std::vector<size_t> shadowCasters(
    std::span<const PlacedLight> lights);

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
