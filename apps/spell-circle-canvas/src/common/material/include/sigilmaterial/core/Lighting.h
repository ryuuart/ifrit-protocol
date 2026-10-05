#pragma once

/** @file
 * @ingroup material-core
 *
 * ONE EMITTER, as a renderer that shades a set reads it: its kind, where
 * it comes from, its colour and strength, and for a point or a spot where
 * it stands and how far it reaches. Colour, angles and strength take
 * animatables, so a bound light changes the shading frame by frame.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmotion/values/Animatable.h>

#include <cstdint>
#include <glm/vec3.hpp>

namespace sigil::material {

/** What an emitter is. */
enum class LightKind : uint8_t {
  Directional,  ///< a direction only: a key light, the sun
  Point,        ///< a position, falling off to nothing at a range
  Spot,         ///< a position and a direction, within a cone
};

/** ONE LIGHT, as a studio key light is set: where it comes from, how high
 *  above the surface, its colour and strength, and the ambient share
 *  every point receives whatever way it faces. A point or a spot also
 *  stands somewhere and reaches so far; the fields a kind does not read
 *  keep their defaults and are ignored. */
struct Light {
  /** Where the light comes FROM on the page, in degrees counter-clockwise
   *  from three o'clock: 120 is the upper left a bevel is lit from.
   *  For a spot this is the source-facing axis; its light travels oppositely.
   */
  motion::Animatable<float> direction = 120.0f;
  /** How far above the page it stands, in degrees: 90 is straight on,
   *  0 grazes the surface. */
  motion::Animatable<float> elevation = 45.0f;
  /** The illumination's RGB colour, including values above one. */
  motion::Animatable<Color> color = Color{1, 1, 1, 1};
  motion::Animatable<float> intensity = 1.0f;
  /** The light every point receives whichever way it faces, as a share of
   *  the surface's own colour. The set renderers take their ambient light
   *  from the set's surroundings and do not read it. */
  float ambient = 0.3f;
  /** Directional by default: every point is lit from one direction. */
  LightKind kind = LightKind::Directional;
  /** Point and spot: where the emitter stands, in the scene's units. */
  glm::vec3 position = {0, 0, 0};
  /** Point and spot: the distance at which the light has fallen to
   *  nothing. */
  float range = 600;
  /** Spot: full strength within this many degrees of where it aims, dark
   *  beyond `outerAngle`, and interpolated between. */
  float innerAngle = 0;
  float outerAngle = 45;

  /** Whether the colour, strength or an angle read by this kind is moving. */
  bool isRunning() const;
  bool operator==(const Light&) const = default;
};

}  // namespace sigil::material
