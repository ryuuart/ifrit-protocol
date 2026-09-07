/** @file
 * What each kind of force adds to the force lane, including the three
 * steerings of a flock over the neighbours a point has.
 */

#include "sigilmotion/physics/Forces.h"

#include <cmath>

namespace sigil::motion::physics {
namespace {

void applyUniform(Points& points, const Force& force) {
  for (size_t i = 0; i < points.size(); ++i) {
    if (!points.movable(i)) continue;
    // Times the mass, because the force lane is a push and this kind is
    // stated as an acceleration: that multiplication IS what makes a
    // heavy point and a light one fall together.
    points.force[i] += force.vector * points.mass[i];
  }
}

void applyDrag(Points& points, const Force& force) {
  for (size_t i = 0; i < points.size(); ++i) {
    if (!points.movable(i)) continue;
    points.force[i] -= points.velocity[i] * (force.strength * points.mass[i]);
  }
}

void applyAttract(Points& points, const Force& force) {
  const float reach = force.radius;
  for (size_t i = 0; i < points.size(); ++i) {
    if (!points.movable(i)) continue;
    const Vec2 toward = force.point - points.position[i];
    const float distance = toward.length();
    if (distance <= 0.0f) continue;
    if (reach > 0.0f && distance > reach) continue;
    // Falling off with distance rather than with its square: the square
    // law is unusable at a screen's scale, where a point that wanders
    // close is thrown across the picture in one step. A falloff that
    // reaches zero AT the radius is what keeps a bounded attractor from
    // snapping things at its edge.
    //
    // And the pull STOPS GROWING one unit from the centre, which is the
    // distance `strength` is stated at: 1/d is unbounded as a point
    // arrives, so without the floor a point that lands on the attractor
    // is thrown off the picture by an arbitrarily large number.
    const float reached = distance < 1.0f ? 1.0f : distance;
    float falloff = 1.0f / reached;
    if (reach > 0.0f) falloff *= 1.0f - distance / reach;
    points.force[i] +=
        toward.normalized() * (force.strength * falloff * points.mass[i]);
  }
}

void applyWind(Points& points, const Force& force) {
  constexpr float kTurn = 6.2831853071795864769f;
  for (size_t i = 0; i < points.size(); ++i) {
    if (!points.movable(i)) continue;
    const float angle =
        force.field.at(points.position[i].x, points.position[i].y) * kTurn;
    points.force[i] += Vec2{std::cos(angle), std::sin(angle)} *
                       (force.strength * points.mass[i]);
  }
}

void applyFlock(Points& points, const Force& force) {
  if (!(force.radius > 0.0f)) return;
  const float reachSquared = force.radius * force.radius;
  const size_t count = points.size();
  for (size_t i = 0; i < count; ++i) {
    if (!points.movable(i)) continue;
    Vec2 away{}, heading{}, centre{};
    int neighbours = 0;
    // EVERY PAIR. A neighbour index over the point set — a uniform grid
    // or a tree built once per step — answers the same question in the
    // time one query takes rather than the time the whole set does, and
    // it is the same index a packing, a poisson scatter and a collision
    // pass all want. This body is the one place that has to change when
    // the tree grows one.
    for (size_t j = 0; j < count; ++j) {
      if (j == i) continue;
      const Vec2 offset = points.position[j] - points.position[i];
      const float distanceSquared = offset.lengthSquared();
      if (distanceSquared > reachSquared || distanceSquared <= 0.0f) continue;
      ++neighbours;
      heading += points.velocity[j];
      centre += points.position[j];
      // Weighted by how close it is, so the one about to be collided
      // with counts for more than the one at the edge of the reach.
      away -= offset * (1.0f / distanceSquared);
    }
    if (neighbours == 0) continue;
    const float share = 1.0f / (float)neighbours;
    const Vec2 alignment = heading * share - points.velocity[i];
    const Vec2 cohesion = centre * share - points.position[i];
    const Vec2 steering = away * force.flock.separation +
                          alignment * force.flock.alignment +
                          cohesion * force.flock.cohesion;
    points.force[i] += steering * (force.strength * points.mass[i]);
  }
}

}  // namespace

void Force::apply(Points& points, float seconds) const {
  switch (kind) {
    case ForceKind::Uniform:
      applyUniform(points, *this);
      return;
    case ForceKind::Drag:
      applyDrag(points, *this);
      return;
    case ForceKind::Attract:
      applyAttract(points, *this);
      return;
    case ForceKind::Wind:
      applyWind(points, *this);
      return;
    case ForceKind::Flock:
      applyFlock(points, *this);
      return;
    case ForceKind::Body:
      if (body) body(points, seconds, *this);
      return;
  }
}

}  // namespace sigil::motion::physics
