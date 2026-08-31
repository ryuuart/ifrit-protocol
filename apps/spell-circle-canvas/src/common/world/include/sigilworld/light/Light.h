#pragma once

/** @file
 * Emitters as plain values: a sun, a point light and a spot, each a
 * comparable struct of where it is, which way it faces, what colour it
 * is and how far it reaches. Nothing here renders, uploads or holds a
 * device — a light is a value a renderer reads, a scene writes to a
 * stage, and a test compares.
 *
 * Colours are LINEAR. A SUN has a direction and no position: it is
 * infinitely far away and shines the same everywhere. A POINT light has
 * a position and a range. A SPOT is a point light narrowed to a cone
 * about its direction, full strength inside the inner angle and dark
 * outside the outer one.
 */

#include <cstdint>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace sigil::world::light {

/** What an emitter IS, which decides which of its fields mean anything. */
enum class Kind : uint8_t {
  Sun,    ///< direction only
  Point,  ///< position and range
  Spot,   ///< position, direction, range and a cone
};

/** One emitter. The fields a kind does not use keep their defaults and
 *  are ignored. */
struct Light {
  Kind kind = Kind::Point;
  /** Linear colour; alpha is unused. */
  glm::vec4 color = {1, 1, 1, 1};
  float intensity = 1;
  /** Sun and spot: the direction the light travels, TOWARD the scene. */
  glm::vec3 direction = {0, -1, 0};
  /** Point and spot: where the emitter sits, in world space. */
  glm::vec3 position = {0, 0, 0};
  /** Point and spot: the distance at which the light has fallen to
   *  nothing. */
  float range = 600;
  /** Spot: full strength within this half-angle of the direction, dark
   *  beyond `outerDeg`, and smoothly between. */
  float innerDeg = 0;
  float outerDeg = 45;

  bool operator==(const Light&) const = default;
};

/** A sun shining along @p direction (toward the scene). */
Light sun(glm::vec3 direction, glm::vec4 color = {1, 1, 1, 1},
          float intensity = 1);
/** A point light at @p position reaching @p range. */
Light point(glm::vec3 position, glm::vec4 color = {1, 1, 1, 1},
            float intensity = 1, float range = 600);
/** A spot at @p position aimed along @p direction, opening to
 *  @p outerDeg and full within @p innerDeg. */
Light spot(glm::vec3 position, glm::vec3 direction, float outerDeg = 45,
           float innerDeg = 0, glm::vec4 color = {1, 1, 1, 1},
           float intensity = 1, float range = 600);

/** How many emitters a renderer is expected to carry in one frame. A
 *  count, not a limit on how many a scene may hold. */
inline constexpr int kBudget = 8;

/** How much of @p light reaches @p at, in [0, 1], before any surface
 *  term. A sun reaches everything equally. A point light falls off on a
 *  window — (1 - (d/range)^2)^2 — rather than an inverse square, which
 *  keeps an authored intensity in the same small range as a sun's
 *  instead of running to thousands, and reaches exactly zero at its
 *  range rather than trailing off forever. A spot multiplies that by its
 *  cone. */
float attenuation(const Light& light, const glm::vec3& at);

/** The light's colour scaled by its intensity — what a renderer uploads
 *  as one value. */
glm::vec3 radiance(const Light& light);

/** AN EMITTER IN DIRECTIONAL TERMS: one direction, one colour and one
 *  strength, which is what a shading model with no per-pixel position
 *  can answer to. */
struct Directional {
  /** The direction the light travels, toward the scene. */
  glm::vec3 direction = {0, -1, 0};
  glm::vec4 color = {1, 1, 1, 1};
  float intensity = 1;
};

/** @p light as a direction. A sun already is one. A light that STANDS
 *  somewhere reaches this reading as the direction from where it stands
 *  toward the origin, at the strength it has there — so a renderer that
 *  shades per vertex and one that shades per pixel disagree about where
 *  a lamp falls off, and agree about where it is. The full falloff is
 *  `attenuation`. */
Directional directional(const Light& light);

}  // namespace sigil::world::light
