#pragma once

/** @file
 * The environment a set stands in: a panorama placed in the scene, what
 * of it reaches a surface, and how much of it is shown behind one.
 *
 * The map itself is the material library's value — one equirect
 * panorama, prefiltered by roughness, with a cosine-convolved diffuse
 * side. What is added here is where it stands and how far it is
 * believed: the node's transform ORIENTS it, the way a dome light is
 * placed in every authoring tool, and the dials below are the lanes a
 * tree binds.
 */

#include <sigilmaterial/texture/EnvironmentMap.h>

#include <glm/vec3.hpp>

namespace sigil::world {

/** THE MAP SHOWN AS THE SET'S SKY, behind everything else in it.
 *
 *  A backdrop is a separate question from what the map lights: a set can
 *  be lit by a sunset it does not show, or show one it barely takes any
 *  light from. `intensity` is both the dial and the switch — at zero
 *  nothing is drawn, which is one number rather than a flag and a
 *  number that can disagree. */
struct Backdrop {
  float intensity = 0;
  /** How much the sky is blurred where it is SHOWN, in roughness units
   *  over the same prefiltered chain a reflection reads. Racking a
   *  backdrop out of focus costs nothing this way: the levels are
   *  already there. */
  float blur = 0;
  /** GROUND PROJECTION: past zero, the sky is treated as a sphere of
   *  this radius standing on the ground rather than as a panorama at
   *  infinity, so a body moving through the set sees the horizon shift
   *  the way it would outdoors. Zero leaves the sky at infinity, which
   *  is what a panorama means on its own. */
  float groundRadius = 0;
  /** Where that sphere is centred, in world units. */
  glm::vec3 projectionCenter = {0, 0, 0};

  bool operator==(const Backdrop&) const = default;
};

/** AN ENVIRONMENT MAP PLACED IN A SET. Copyable, comparable, and cheap
 *  to pass: the panorama behind it is a shared handle. */
struct Environment {
  material::EnvironmentMap map;
  /** Overall scale on everything the map contributes. */
  float intensity = 1;
  /** Multiplied into every sample, so a sky can be warmed or cooled
   *  without rebaking it. */
  glm::vec3 tint = {1, 1, 1};
  /** The diffuse side alone — what the panorama's cosine convolution
   *  puts on a surface facing away from every emitter. */
  float diffuse = 1;
  /** The specular side alone — what a surface mirrors. Pushing one and
   *  not the other is how a set gets a bright reflection over a dim
   *  bounce, which is a look and not a physical claim. */
  float specular = 1;
  /** Added to a surface's roughness before it picks a prefiltered level,
   *  so a whole set can be softened without editing a material. */
  float roughnessBias = 0;
  /** THE EXPOSURE THE SET IS READ AT: what every radiance is multiplied
   *  by before the tone curve compresses it onto what a display can
   *  hold. Doubling it is one stop, and it is the dial that decides
   *  which part of the range the curve's shoulder falls on — a set lit
   *  by a panorama with a sun in it wants a smaller one than a set lit
   *  by a lamp.
   *
   *  It is the one dial here that stands where no panorama does: a lit
   *  sum ends at the curve whether or not the set carries a sky. */
  float exposure = 1;

  /** A SECOND MAP, crossfaded over the first: at 0 only `map` is read,
   *  at 1 only this one. Both sides are sampled and mixed rather than
   *  one being rebuilt, which is what makes a sky able to change while
   *  the frame is running. */
  material::EnvironmentMap next;
  float crossfade = 0;

  Backdrop backdrop;

  /** Whether this environment has a panorama to sample at all. */
  bool valid() const { return map.valid(); }
  bool operator==(const Environment&) const = default;
};

}  // namespace sigil::world
