/** @file
 * The three operations that keep the lanes the same length.
 */

#include "sigilmotion/physics/Points.h"

namespace sigil::motion::physics {

size_t Points::add(Vec2 at, Vec2 startingVelocity, float startingMass,
                   bool held) {
  position.push_back(at);
  // Where it was when the step began is where it is: a point that has
  // not been stepped yet has moved nowhere, and any other answer would
  // hand the first constraint pass a velocity nobody asked for.
  previous.push_back(at);
  velocity.push_back(startingVelocity);
  force.push_back({});
  mass.push_back(startingMass);
  pinned.push_back(held ? 1u : 0u);
  return position.size() - 1;
}

void Points::remove(size_t index) {
  if (index >= position.size()) return;
  const size_t last = position.size() - 1;
  position[index] = position[last];
  previous[index] = previous[last];
  velocity[index] = velocity[last];
  force[index] = force[last];
  mass[index] = mass[last];
  pinned[index] = pinned[last];
  position.pop_back();
  previous.pop_back();
  velocity.pop_back();
  force.pop_back();
  mass.pop_back();
  pinned.pop_back();
}

void Points::clear() {
  position.clear();
  previous.clear();
  velocity.clear();
  force.clear();
  mass.clear();
  pinned.clear();
}

}  // namespace sigil::motion::physics
