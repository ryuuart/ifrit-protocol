#pragma once

/** @file
 * THE STEPPER: one value carrying the step it takes, what it damps and
 * how many times it goes round the constraints, and one call that
 * advances a point set through its forces and its constraints.
 */

#include <sigilmotion/physics/Constraints.h>
#include <sigilmotion/physics/Forces.h>
#include <sigilmotion/physics/Points.h>

#include <span>

namespace sigil::motion::physics {

/** THE VERLET STEP, as a value.
 *
 *  `dt` is a PROP and not an argument, and that is the whole determinism
 *  claim: a simulation stepped by a frame's delta is a different
 *  simulation on every machine and on every frame it stutters, while one
 *  stepped by a fixed number is the same run everywhere and can be
 *  replayed, seeded and compared. A host with a varying clock runs this
 *  from the fixed-rate lane of its ticker, which is exactly the shape
 *  this asks for: step as many times as the elapsed time holds, and draw
 *  whatever the last step left.
 *
 *  The order inside one step is forces, integrate, constrain, recover.
 *  The velocity is recovered from the movement the step actually
 *  achieved, which is what makes a constraint take speed away without
 *  anything having to say that it does. */
struct Verlet {
  /** How much time one step covers, in seconds. */
  float dt = 1.0f / 60.0f;
  /** The fraction of its speed a point loses per second, applied
   *  exponentially so that the loss is the same whatever the step is —
   *  0 keeps everything, 1 leaves about a third of the speed after a
   *  second. It is here rather than as a `Drag` force because it is the
   *  simulation's own settling and not something in the scene pushing:
   *  a drag that weighs nothing and comes from nowhere. */
  float damping = 0.0f;
  /** How many times the constraint list is walked per step. One pass
   *  satisfies each constraint alone and pulls the ones before it out
   *  again; a few passes converge a chain. More is stiffer, not more
   *  correct. */
  int iterations = 4;

  bool operator==(const Verlet&) const = default;

  /** ONE STEP. The forces fill the force lane, the points move, the
   *  constraints project them, and the velocity comes back out of where
   *  they ended up. */
  void step(Points& points, std::span<const Force> forces,
            std::span<const Constraint> constraints) const;

  /** The same step with nothing tying the points together — a field of
   *  particles, which is the case with no constraint list at all. */
  void step(Points& points, std::span<const Force> forces) const {
    step(points, forces, {});
  }
};

}  // namespace sigil::motion::physics
