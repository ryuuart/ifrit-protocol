#pragma once

/** @file
 * glm's column-major mat4 as USD's row-vector GfMatrix4d. Element (row
 * i, column j) of the USD matrix is glm[i][j] — glm indexes column
 * first, so the elementwise copy is the transpose USD's convention
 * wants, and no explicit transpose appears anywhere.
 */

#include <pxr/base/gf/matrix4d.h>

#include <glm/mat4x4.hpp>

namespace sigil::usd {

inline pxr::GfMatrix4d toGf(const glm::mat4& m) {
  pxr::GfMatrix4d out;
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) out[i][j] = (double)m[i][j];
  return out;
}

}  // namespace sigil::usd
