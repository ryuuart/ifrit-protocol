/** @file
 * The projection: the band two points are held in, and the place one
 * point is held at.
 */

#include "sigilmotion/physics/Constraints.h"

namespace sigil::motion::physics {

void Constraint::project(Points& points) const {
  if (kind == ConstraintKind::Pin) {
    if (a >= points.size()) return;
    // A pin overrides the mass and the pinned lane both: it is the
    // caller saying where this point is this frame, and nothing in the
    // solve outranks that.
    points.position[a] = at;
    return;
  }

  if (a >= points.size() || b >= points.size() || a == b) return;
  const float weightA = points.inverseMass(a);
  const float weightB = points.inverseMass(b);
  const float total = weightA + weightB;
  // Two immovable points cannot be moved apart, and dividing by their
  // total weight would say so with a NaN instead of by doing nothing.
  if (!(total > 0.0f)) return;

  const Vec2 offset = points.position[b] - points.position[a];
  const float length = offset.length();
  const float longest = rest + (slack > 0.0f ? slack : 0.0f);
  // Inside the band there is nothing to say. A rope hanging slack, a
  // joint inside its limit and a pair that is not touching are all this
  // case, and it is why one value covers the stick and the inequality.
  if (length >= rest && length <= longest) return;

  const float target = length < rest ? rest : longest;
  // Two points sitting on each other have no direction to be moved
  // along; the next step's forces will separate them, and inventing a
  // direction here would make the answer depend on the arithmetic.
  if (!(length > 0.0f)) return;

  const Vec2 direction = offset * (1.0f / length);
  const float correction = (length - target) * stiffness;
  points.position[a] += direction * (correction * weightA / total);
  points.position[b] -= direction * (correction * weightB / total);
}

}  // namespace sigil::motion::physics
