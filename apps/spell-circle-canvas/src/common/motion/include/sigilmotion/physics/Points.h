#pragma once

/** @file
 * @ingroup motion-physics
 *
 * THE POINT SET A SIMULATION IS: one value holding a lane per property,
 * written in `glm::vec2` positions.
 *
 * Lanes rather than a vector of particles because everything that reads
 * a simulation reads one property of all of it — a stepper walks the
 * positions, a stamping leaf walks the positions and the colours, a
 * draw walks the velocities. A structure of arrays is the shape all of
 * those want, and it is the shape an instanced draw takes its lanes in.
 */

#include <sigilmotion/time/Duration.h>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

/** A POINT SET THAT IS STEPPED RATHER THAN READ. Everywhere else in this
 *  library a value answers where a property is at a time; here a set of
 *  points carries its own state and is advanced one frame at a time by
 *  forces, constraints and a stepper, so where it ends up is the history
 *  of the steps and not a function of the clock. The set is lanes of
 *  plain numbers, the grid answers what is near what, and particles are
 *  a set whose members are born, age and die. */
namespace sigil::motion::physics {

/* A POSITION OR A DISPLACEMENT is a `glm::vec2`, in whatever units the
 * caller is simulating in. This library states no unit: a simulation is in
 * pixels for a screen, in metres for a model of something, in whatever a
 * sketch is drawing in. Every number below — a gravity, a stiffness, a
 * radius — is in those units and the seconds the stepper is given, and
 * nothing here converts between two of them. `glm::length` is a vector's
 * length; its member `length()` is the component count, never a distance. */

/** THE POINT SET: one lane per property, all the same length.
 *
 *  The lanes are public because a simulation is a value a caller reads
 *  and writes, not a machine that owns its state: a describe walks
 *  `position` to place its instances, an emitter writes `velocity` into
 *  the rows it just added, and a picker writes `pinned` where a cursor
 *  grabbed something. `add` and `remove` are here so the lanes cannot be
 *  left at different lengths, and nothing else about the set is hidden.
 *
 *  READING IT INTO AN INSTANCED DRAW is one loop: the stamping leaf's
 *  pool holds its own lanes of two-float positions, so a frame copies
 *  `position[i]` into the pool's position lane and whatever else it is
 *  drawing from — a rotation off the velocity's angle, a scale off the
 *  mass, an alpha off an age the caller keeps beside these lanes. This
 *  library holds no colour and no age: what a point IS on screen is the
 *  drawing's business, and a simulation that carried it would have to
 *  name a renderer. */
struct Points {
  /** Where each point is now. */
  std::vector<glm::vec2> position;
  /** Where it was when the current step began — the position the
   *  constraint passes moved it away from, which is what turns a
   *  position correction back into a velocity. */
  std::vector<glm::vec2> previous;
  /** How fast it is going, in units per second. It is the lane the
   *  stepper carries motion in and the one a drag, a wind and a flock
   *  read; the stepper rewrites it from the movement a step actually
   *  achieved, so a point stopped by a constraint loses the speed the
   *  constraint took. */
  std::vector<glm::vec2> velocity;
  /** What is pushing on it this step, in mass times units per second
   *  squared. The lane is the CALLER'S to pre-load: a step accumulates
   *  its forces onto whatever the lane already holds and clears it once
   *  it has integrated, so a push written here between two steps is
   *  spent exactly once and one written before a step that is never
   *  taken is still there for the next one. */
  std::vector<glm::vec2> force;
  /** How much of it there is. Zero or less is IMMOVABLE — infinitely
   *  heavy — which is the same answer `pinned` gives and is reached by a
   *  different road: a mass of zero is a wall, a pin is a point held
   *  where something put it. */
  std::vector<float> mass;
  /** Held still: the stepper does not move it and the constraints do not
   *  push it. One byte rather than a bit, because these lanes are read
   *  in loops and a bit vector cannot hand out a reference. */
  std::vector<uint8_t> pinned;

  [[nodiscard]] size_t size() const { return position.size(); }
  [[nodiscard]] bool empty() const { return position.empty(); }

  /** A point at @p at, moving at @p velocity, and its index. */
  size_t add(glm::vec2 at, glm::vec2 startingVelocity = {}, float startingMass = 1.0f,
             bool held = false);

  /** Drop the point at @p index by moving the LAST one into its place —
   *  which is what a field of particles wants, since the order of a
   *  cloud means nothing and shuffling every lane down would cost the
   *  whole set per death.
   *
   *  It renumbers, so anything holding an index INTO the set — a
   *  constraint, a pin, a caller's own table — is holding the wrong
   *  point afterwards. A set with constraints over it is grown and
   *  cleared, not thinned. */
  void remove(size_t index);

  /** Every lane emptied. */
  void clear();

  /** Whether the stepper and the constraints may move this point.
   *
   *  @p index must name a point this set holds. These two read the lanes
   *  the way `position[i]` does — unchecked, because they are called once
   *  per point per force and a bounds test there would be paid by every
   *  point of every step. Whoever holds an index that may be stale
   *  answers for it: a constraint carries indices a `remove` renumbers,
   *  so `Constraint::project` tests them against `size()` before it asks
   *  anything here. */
  [[nodiscard]] bool movable(size_t index) const {
    return !pinned[index] && mass[index] > 0.0f;
  }
  /** One over the mass, or zero for anything immovable — the weight a
   *  constraint shares its correction by, where an immovable point takes
   *  none of it and its partner takes all. @p index must name a point
   *  this set holds, as above. */
  [[nodiscard]] float inverseMass(size_t index) const {
    return movable(index) ? 1.0f / mass[index] : 0.0f;
  }
};

}  // namespace sigil::motion::physics
