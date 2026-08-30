#pragma once

/** @file
 * The tiny safety vocabulary over glm the library's translation units
 * share, and the one orientation basis every stamp is placed with —
 * public, because the GPU instancing path stamps with the same basis.
 *  glm::normalize and glm::cross are used directly everywhere; these
 *  exist only for the degenerate-input policies glm leaves undefined.
 *  The pop cook's scatter basis is NOT here on purpose: its math is
 *  bit-matched to the GPU kernel and stays literal at the site.
 */

#include <glm/glm.hpp>

namespace sigil::geometry::mesh {

inline glm::vec3 normalized(glm::vec3 v, glm::vec3 fallback = {0, 0, 1}) {
  const float len = glm::length(v);
  return len < 1e-12f ? fallback : v * (1.0f / len);
}

/** Orientation basis with +z along @p dir — the construction
 *  points::instance() stamps with. World's instanced path calls this
 *  SAME function, so a Cloud renders identically merged, instanced,
 *  or GPU-drawn. */
inline void basisFor(glm::vec3 dir, glm::vec3 up, glm::vec3* x, glm::vec3* y,
                     glm::vec3* z) {
  *z = normalized(dir);
  glm::vec3 side = glm::cross(up, *z);
  if (glm::dot(side, side) < 1e-8f) side = glm::cross(glm::vec3{1, 0, 0}, *z);
  *x = normalized(side);
  *y = glm::cross(*z, *x);
}

}  // namespace sigil::geometry::mesh
