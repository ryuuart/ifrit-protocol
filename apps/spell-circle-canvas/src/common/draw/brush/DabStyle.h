#pragma once

/** @file
 * What one dab looks like once the tool's dynamics have been applied:
 * where it lands, how big, how opaque, at what angle and aspect. Private
 * to the executors.
 */

#include <include/core/SkColor.h>
#include <include/core/SkPoint.h>
#include <sigildraw/brush/Dab.h>
#include <sigildraw/brush/Tool.h>
#include <sigilmaterial/color/Color.h>

#include <algorithm>

namespace sigil::draw {
class Pen;
}

namespace sigil::draw::brush {

struct DabStyle {
  SkPoint position;
  float size;
  float opacity;
  float angle;
  float aspect;
};

/** The tool's envelope at the dab's progress, times the dab's own
 *  pressure. */
inline float pressureAt(const Tool& tool, const Dab& dab) {
  return std::max(0.0f, dab.pressure * tool.pressure.at(dab.progress));
}

/** The tool's colour at @p alpha of its load; the colour's own alpha
 *  multiplies in. */
inline material::Color pigment(const Tool& tool, float alpha) {
  material::Color color = tool.color;
  color.a = std::clamp(color.a * tool.opacity * alpha, 0.0f, 1.0f);
  return color;
}

/** The dab's style from the tool's pressure, speed, tilt and jitter
 *  responses. @p scatterPosition moves it across the centreline by the
 *  tool's scatter; the tips that scatter their own particles pass false. */
DabStyle styleDab(Pen& pen, const Tool& tool, const Dab& dab,
                  bool scatterPosition = true);

}  // namespace sigil::draw::brush
