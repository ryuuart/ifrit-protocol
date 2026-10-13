#pragma once

/** @file
 * @ingroup world-frame
 * THE ENVIRONMENT A SET STANDS IN: a panorama every lit body samples for
 * what reaches it from all around, stated once on the frame — never a
 * node, because it surrounds everything rather than standing anywhere.
 * The panorama itself is an `EnvironmentMap`; what is added here is what
 * of it reaches a surface, how it is turned, and how much of it is shown
 * behind the scene.
 */

#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmotion/values/Animatable.h>

#include <glm/mat3x3.hpp>

namespace sigil::world {

/** THE PANORAMA: an equirectangular radiance image with its prefiltered
 *  levels and its irradiance, baked once. SigilMaterial holds the type,
 *  because SigilGeometry's device residency uploads it and Geometry
 *  stands beneath this library; this is the name a scene spells it by. */
using EnvironmentMap = material::EnvironmentMap;

/** What of the environment reaches a surface, how it is turned and how
 *  much of it is shown — every dial animatable, read once a frame. */
struct EnvironmentOptions {
  /** Overall scale on everything the panorama contributes. */
  motion::Animatable<float> intensity{1.0f};
  /** The diffuse side alone — what the panorama's cosine convolution
   *  puts on a surface facing away from every light. */
  motion::Animatable<float> diffuse{1.0f};
  /** The specular side alone — what a surface mirrors. Pushing one and
   *  not the other is a look, not a physical claim. */
  motion::Animatable<float> specular{1.0f};
  /** Added to every surface's roughness before it picks a prefiltered
   *  level, so a whole scene softens without a material being edited. */
  motion::Animatable<float> roughnessBias{0.0f};
  /** A SECOND PANORAMA, crossfaded over the first: at 0 only the first is
   *  read, at 1 only this one. Both are sampled and mixed, which is what
   *  lets a sky change while the frame is running. */
  EnvironmentMap next;
  motion::Animatable<float> crossfade{0.0f};
  /** The turn of the panorama about the up axis, in degrees. */
  motion::Animatable<float> rotation{0.0f};
  /** THE PANORAMA SHOWN behind the scene, at this strength — zero shows
   *  none of it, so the dial is also the switch. What a scene is lit by and
   *  what it shows are separate questions. */
  motion::Animatable<float> shown{0.0f};
  /** How blurred the shown panorama is, in the roughness units a
   *  reflection reads. */
  motion::Animatable<float> blur{0.0f};
  /** GROUND PROJECTION: past zero, the shown panorama is a sphere of this
   *  radius standing on the ground at the world origin rather than a
   *  picture at infinity, so a body moving through the scene sees the
   *  horizon shift the way it would outdoors. */
  float groundRadius = 0;
};

/** THE ENVIRONMENT AS ONE FRAME READ IT: the panorama, its second, and
 *  every dial read once and held, with the turn as the matrix that
 *  carries a world direction into the panorama's own frame. What an
 *  executor reads. */
struct Environment {
  EnvironmentMap map;
  EnvironmentMap next;
  float intensity = 1;
  float diffuse = 1;
  float specular = 1;
  float roughnessBias = 0;
  float crossfade = 0;
  float shown = 0;
  float blur = 0;
  float groundRadius = 0;
  /** A world direction into the panorama's frame. */
  glm::mat3 orientation{1.0f};

  /** Whether there is a panorama to sample at all. */
  [[nodiscard]] bool valid() const { return map.valid(); }
  bool operator==(const Environment&) const = default;
};

/** @p options read once, over @p map. */
[[nodiscard]] Environment environmentOf(const EnvironmentMap& map,
                                        const EnvironmentOptions& options);

}  // namespace sigil::world
