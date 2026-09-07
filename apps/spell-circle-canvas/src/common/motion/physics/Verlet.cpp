/** @file
 * One step: the forces, the move, the constraint passes, and the
 * velocity recovered from where the points ended up.
 */

#include "sigilmotion/physics/Verlet.h"

#include <cmath>

namespace sigil::motion::physics {

void Verlet::step(Points& points, std::span<const Force> forces,
                  std::span<const Constraint> constraints) const {
  const size_t count = points.size();
  if (count == 0 || !(dt > 0.0f)) return;

  for (size_t i = 0; i < count; ++i) points.force[i] = {};
  for (const Force& force : forces) force.apply(points, dt);

  // Exponential rather than a fraction taken per step: a loss stated per
  // second is the same loss whatever the step is, so re-timing a
  // simulation does not re-tune how it settles.
  const float kept = damping > 0.0f ? std::exp(-damping * dt) : 1.0f;

  for (size_t i = 0; i < count; ++i) {
    points.previous[i] = points.position[i];
    if (!points.movable(i)) {
      // An immovable point carries no motion into the next step: it is
      // where something else put it, and a velocity left on it would be
      // read by a drag and a flock as a speed it does not have.
      points.velocity[i] = {};
      continue;
    }
    points.velocity[i] *= kept;
    points.velocity[i] += points.force[i] * (dt / points.mass[i]);
    points.position[i] += points.velocity[i] * dt;
  }

  const int passes = iterations > 0 ? iterations : 1;
  if (!constraints.empty())
    for (int pass = 0; pass < passes; ++pass)
      for (const Constraint& constraint : constraints)
        constraint.project(points);

  // The velocity is what the step ACHIEVED, not what it intended. A
  // point a stick stopped ends where the stick allows and comes out of
  // the step at the speed that movement was, which is how a constraint
  // takes speed away without any force having said so.
  const float perSecond = 1.0f / dt;
  for (size_t i = 0; i < count; ++i) {
    if (!points.movable(i)) continue;
    points.velocity[i] = (points.position[i] - points.previous[i]) * perSecond;
  }
}

}  // namespace sigil::motion::physics
