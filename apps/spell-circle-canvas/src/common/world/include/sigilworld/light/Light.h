#pragma once

/** @file
 * @ingroup world-light
 * Emitters in a set: Material's `material::Light` — the one light value a
 * surface in the plane and a body in a set are both shaded under — placed
 * in three dimensions. What is added here is only what a set in space
 * needs and a plane does not: a light's angles read as a direction in
 * the world, the sun, point and spot spelled by where they stand and aim,
 * and the distance and cone falloffs. Nothing here renders, uploads or
 * holds a device.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Lighting.h>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/** A LIGHT'S ANGLES IN A SET: the world is y-up, and a light's
 *  `elevation` is its height above the ground plane (90 overhead), its
 *  `direction` the bearing it comes from on that plane, in degrees
 *  counter-clockwise from +x seen from above — so 90 comes from −z. A
 *  light's colour is read as linear radiance. Where an emitter STANDS is
 *  the element's transform and no part of this catalogue. */
namespace sigil::world::light {

/** The unit direction @p light travels, TOWARD the scene. */
glm::vec3 travel(const material::Light& light);
/** Turns @p light to travel along @p direction (toward the scene). A
 *  direction of no length travels straight up. */
void aim(material::Light& light, glm::vec3 direction);
/** @p light with each animatable read once and held as that constant,
 *  and every other field carried as it is: what a frame keeps of a light,
 *  so nothing written to a live value afterwards reaches the copy. */
material::Light held(const material::Light& light);

/** A sun shining along @p direction (toward the scene). */
material::Light sun(glm::vec3 direction, material::Color color = {1, 1, 1, 1},
                    float intensity = 1);
/** A point light at @p position reaching @p range. */
material::Light point(glm::vec3 position, material::Color color = {1, 1, 1, 1},
                      float intensity = 1, float range = 600);
/** A spot at @p position aimed along @p direction, opening to
 *  @p outerAngle degrees and full within @p innerAngle. */
material::Light spot(glm::vec3 position, glm::vec3 direction,
                     float outerAngle = 45, float innerAngle = 0,
                     material::Color color = {1, 1, 1, 1}, float intensity = 1,
                     float range = 600);

/** How much of @p light reaches @p at, in [0, 1], before any surface
 *  term. A directional light reaches everything equally; a point light
 *  falls off on a window rather than an inverse square, reaching exactly
 *  zero at its range; a spot multiplies that by its cone. */
float attenuation(const material::Light& light, const glm::vec3& at);

/** The light's colour scaled by its intensity — what a renderer uploads
 *  as one value. */
glm::vec3 radiance(const material::Light& light);

/** AN EMITTER IN DIRECTIONAL TERMS: one direction, one colour and one
 *  strength, which is what a shading model with no per-pixel position
 *  can answer to. */
struct Directional {
  /** The direction the light travels, toward the scene. */
  glm::vec3 direction = {0, -1, 0};
  glm::vec4 color = {1, 1, 1, 1};
  float intensity = 1;
};

/** @p light as a direction. A directional light already is one. A light
 *  that STANDS somewhere reaches this reading as the direction from where
 *  it stands toward the origin, at the strength it has there — so a
 *  renderer that shades per vertex and one that shades per pixel disagree
 *  about where a lamp falls off, and agree about where it is. The full
 *  falloff is `attenuation`. */
Directional directional(const material::Light& light);

}  // namespace sigil::world::light
