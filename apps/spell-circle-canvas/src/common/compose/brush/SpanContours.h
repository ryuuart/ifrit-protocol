#pragma once

/** @file
 * Internal to the brush tier — the contour measure the span normal form and the
 * band region share: where each contour of a path starts and ends in the
 * path's global arc length.
 */

#include <include/core/SkPath.h>

#include <vector>

namespace sigil::compose::detail {

/** Where each contour starts and ends in the path's GLOBAL arc length.
 *  Global, not per-contour, because that is what SkTrimPathEffect uses
 *  and therefore what trim() has always meant: a reveal and a trim of the
 *  same numbers must describe the same run. */
struct ContourRun {
  float start = 0, length = 0;
  bool closed = false;
};

/** Every contour's run in the path's global arc length, with the total in
 *  @p total. */
std::vector<ContourRun> measureContours(const SkPath& path, float* total);

}  // namespace sigil::compose::detail
