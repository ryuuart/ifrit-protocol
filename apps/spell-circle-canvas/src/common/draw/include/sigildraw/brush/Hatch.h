#pragma once

/** @file
 * Parallel brush marks through a polygon.
 */

#include <include/core/SkPoint.h>
#include <sigildraw/Constants.h>
#include <sigildraw/brush/Tool.h>

#include <span>

namespace sigil::draw {
class Pen;
}

namespace sigil::draw::brush {

/** Parallel marks clipped to a polygon. The angle is radians. Jitter is
 *  a fraction of the spacing and moves each mark's ends after the clip,
 *  by up to twice that fraction of the spacing, so a jittered mark may
 *  reach past the edge. Gradient changes the spacing across the shape by
 *  a tenth of itself per lane. Continuous joins the marks into one
 *  serpentine line. */
struct Hatch {
  float spacing = 5.0f;
  float angle = QUARTER_PI;
  float jitter = 0.0f;
  float gradient = 0.0f;
  bool continuous = false;

  bool operator==(const Hatch&) const = default;
};

/** Paints parallel marks through the polygon with the tool, thinned at
 *  both ends of each mark, and restores the pen's state. */
void hatch(Pen& pen, const Tool& tool, std::span<const SkPoint> polygon,
           const Hatch& style = {});

/** The same through several even-odd contours: crossings are paired
 *  across the whole collection, so inner contours cut holes and disjoint
 *  contours are separate islands of one gesture. */
void hatch(Pen& pen, const Tool& tool,
           std::span<const std::span<const SkPoint>> contours,
           const Hatch& style = {});

}  // namespace sigil::draw::brush
