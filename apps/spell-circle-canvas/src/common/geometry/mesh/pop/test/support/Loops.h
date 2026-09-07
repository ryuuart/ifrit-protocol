#pragma once

/** @file
 * The seed every pop chain in these tests is scattered along.
 */

#include <cmath>
#include <glm/vec3.hpp>
#include <vector>

namespace sigil::geometry::mesh::pop::test {

/** A flat ring of @p n points of radius @p radius in the xz plane — a seed
 *  with no height of its own, so anything that lifts a point off y = 0 is
 *  the operator under test and not the loop it started from. */
inline std::vector<glm::vec3> flatRing(int n, float radius) {
  std::vector<glm::vec3> out;
  out.reserve((size_t)n);
  for (int i = 0; i < n; ++i) {
    const float a = (float)i / (float)n * 2.0f * (float)M_PI;
    out.emplace_back(radius * std::cos(a), 0.0f, radius * std::sin(a));
  }
  return out;
}

}  // namespace sigil::geometry::mesh::pop::test
