#pragma once

/** @file
 * Letterboxing: where a canvas of one shape lands inside a box of
 * another.
 */

#include <include/core/SkRect.h>
#include <include/core/SkSize.h>

#include <algorithm>

namespace sigil::sketch {

/** WHERE A CANVAS LANDS INSIDE A BOX: the scale that fits it whole, and
 *  the point its top-left corner reaches.
 *
 *  Every host here does this — the live canvas fits a sketch into an
 *  item, the montage fits a plate into a viewport, a still fits one into
 *  a thumbnail — and the two halves must agree exactly, because a frame
 *  is DRAWN through the scale and a pointer is read BACK through its
 *  inverse. Two spellings of the same arithmetic is how they stop
 *  agreeing. */
struct Fit {
  float scale = 1.0f;
  float x = 0.0f;
  float y = 0.0f;

  /** The rectangle @p content occupies under this fit. */
  [[nodiscard]] SkRect rect(SkSize content) const {
    return SkRect::MakeXYWH(x, y, content.width() * scale,
                            content.height() * scale);
  }
};

/** @p content centred inside @p box at the largest scale that fits it
 *  whole, times @p zoom — which magnifies about the box's centre, so a
 *  zoom past 1 crops rather than moving the subject. */
[[nodiscard]] inline Fit fitInto(SkSize content, const SkRect& box,
                                 float zoom = 1.0f) {
  Fit fit;
  if (content.width() <= 0 || content.height() <= 0) return fit;
  fit.scale =
      std::min(box.width() / content.width(), box.height() / content.height()) *
      zoom;
  fit.x = box.centerX() - content.width() * fit.scale * 0.5f;
  fit.y = box.centerY() - content.height() * fit.scale * 0.5f;
  return fit;
}

}  // namespace sigil::sketch
