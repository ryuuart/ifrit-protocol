#pragma once

/** @file
 * HOW FAR A STROKE'S INK STANDS FROM THE PATH IT WIDENS — the number a
 * mark compares with its own `bleed()` before it says it stays on the
 * outline it is handed. Internal to the brush tier.
 */

#include <sigilgeometry/path/Stroke.h>

#include <algorithm>
#include <cmath>

namespace sigil::compose::detail {

/** The renderer's miter limit where a mark sets none, in half-widths. */
inline constexpr float kDefaultMiterLimit = 4.0f;

/** The reach of a band @p width wide stroked along a path with @p join and
 *  @p cap: half the width, grown at a mitred corner to @p miterLimit
 *  half-widths and at a square cap to the corner of its square — the
 *  inflation the renderer bounds a stroked path by. A round or bevelled
 *  corner and a butt or round end stand within the half width. */
inline float strokeReach(float width, geometry::path::Join join,
                         geometry::path::Cap cap,
                         float miterLimit = kDefaultMiterLimit) {
  const float half = std::max(width, 0.0f) * 0.5f;
  const float corner = join == geometry::path::Join::Miter
                           ? std::max(miterLimit, 1.0f)
                           : 1.0f;
  const float end = cap == geometry::path::Cap::Square ? std::sqrt(2.0f) : 1.0f;
  return half * std::max(corner, end);
}

}  // namespace sigil::compose::detail
