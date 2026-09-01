#pragma once

/** @file
 * The moving frame — one orientation on a curve, and the currency every
 * rail is a sequence of. A pose read at a distance and a ring of a sweep
 * are the same type measured two ways, which is what lets a camera
 * flying a spline and a profile riding one speak a single vocabulary.
 */

#include <glm/glm.hpp>

namespace sigil::geometry::mesh::curve {

/** An orthonormal moving frame on the curve. */
struct Frame3 {
  glm::vec3 position{0, 0, 0};
  glm::vec3 tangent{0, 0, 1};
  glm::vec3 normal{0, 1, 0};    // "up", parallel-transported
  glm::vec3 binormal{1, 0, 0};  // tangent x normal
  float t = 0;                  // curve parameter
};

}  // namespace sigil::geometry::mesh::curve
