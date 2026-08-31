#pragma once

/** @file
 * Presets: elements composed out of the verbs a tree is already written
 * in — a three-point rig, a turntable, and a set with both over a
 * ground plane.
 *
 * NOTHING HERE DECIDES A LOOK. Each returns an ordinary `Element` whose
 * every field the caller can read, replace or ignore, and the only
 * constants in it are the geometry of the arrangement — where a key
 * light stands relative to a subject, how a rail circles it — plus one
 * neutral grey for a ground plane that was given no surface. There is
 * no shading model here, no catalog of surfaces and no material of this
 * library's own: a preset is a shorthand for a tree someone could have
 * written by hand, and it is worth having only for as long as that
 * stays true.
 */

#include <sigilmaterial/core/Material.h>
#include <sigilworld/element/Element.h>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <optional>

namespace sigil::world::kit {

/** WHERE THREE LIGHTS STAND ROUND A SUBJECT.
 *
 *  The arrangement is stated relative to the subject, so one rig serves
 *  a thumbnail and a room: `extent` is how far across the subject is and
 *  every distance is a multiple of it. `bearing` turns the whole rig
 *  about the subject's up axis, so a set is re-lit by turning one
 *  number. */
struct Rig {
  /** Where the subject stands, and how far across it is. */
  glm::vec3 at{0.0f, 0.0f, 0.0f};
  float extent = 100.0f;
  /** How far the key stands from the subject, in extents. */
  float distance = 2.2f;
  /** The key's bearing round the subject and its height above it, in
   *  degrees. The fill stands a quarter turn the other way and lower;
   *  the back light stands opposite and higher. */
  float bearing = -35.0f;
  float elevation = 28.0f;
  /** The fill and the back light as fractions of the key's strength.
   *  The key is what a subject is read by; the other two are what keeps
   *  it from being read by the key alone. */
  float fill = 0.35f;
  float back = 0.55f;
  /** The key's own strength and colour; the other two take the same
   *  colour, scaled. */
  float intensity = 1.0f;
  glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
};

/** THE RIG, as one element with three keyed children — `key`, `fill`
 *  and `back` — each standing where the arrangement puts it and
 *  carrying one emitter. */
Element threePoint(const Rig& rig = {});

/** A CAMERA CIRCLING A SUBJECT: how far out it stands, how high, and
 *  how long one turn takes. */
struct Turntable {
  glm::vec3 at{0.0f, 0.0f, 0.0f};
  float radius = 420.0f;
  float height = 170.0f;
  /** Seconds for one full turn. Zero or less parks the camera at its
   *  first station. */
  float period = 8.0f;
  float fovYDeg = 42.0f;
  /** How many points the rail is drawn through. More is rounder; the
   *  count is here because it decides the curve and therefore the
   *  pixels, not because it is a dial worth turning. */
  int stations = 12;
};

/** THE RAIL a turntable rides: a closed loop of `stations` points round
 *  `at`, at the table's radius and height. */
Spline3 rail(const Turntable& table);

/** THE CAMERA at scene time @p seconds, as one element riding that rail
 *  and looking at the subject from wherever it has reached.
 *
 *  The viewpoint is written in the rail's own moving frame, which is
 *  what makes the aim independent of how far the camera has travelled:
 *  the frame's first axis points inward at every station, so the subject
 *  is one radius along it and one height down, always. */
Element turntable(const Turntable& table, float seconds);

/** A LIT SET: a ground plane, a rig over it, a turntable round it, and
 *  whatever is being looked at. */
struct Set {
  Rig rig;
  Turntable table;
  /** How wide the ground plane is, in the rig's extents. Zero lays
   *  none. */
  float ground = 7.0f;
  /** How far below the subject the ground lies, in extents. */
  float drop = 0.6f;
  /** What the ground is made of. Left empty it is one neutral grey,
   *  which is the only colour this library states and is stated so that
   *  a set has a floor rather than because grey is the right answer. */
  std::optional<material::Material> surface;
};

/** The set, with @p subject standing on it. The tree is `set` with the
 *  children `ground`, `rig`, `camera` and `subject`, in that order — a
 *  caller who wants one of them alone builds it alone. */
Element litSet(Element subject, const Set& set = {}, float seconds = 0.0f);

}  // namespace sigil::world::kit
