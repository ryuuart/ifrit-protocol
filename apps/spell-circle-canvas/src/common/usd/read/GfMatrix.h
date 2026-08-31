#pragma once

/** @file
 * USD's row-vector GfMatrix4d as glm's column-major mat4. glm[i][j] is
 * element (row i, column j) of the USD matrix — glm indexes column
 * first, so the elementwise copy is the transpose glm's convention
 * wants, and no explicit transpose appears anywhere.
 */

#include <pxr/base/gf/matrix4d.h>

#include <glm/mat4x4.hpp>

namespace sigil::usd {

inline glm::mat4 fromGf(const pxr::GfMatrix4d& g) {
  glm::mat4 out;
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) out[i][j] = (float)g[i][j];
  return out;
}

}  // namespace sigil::usd
