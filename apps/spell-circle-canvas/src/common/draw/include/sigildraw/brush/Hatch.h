#pragma once

/** @file
 * @ingroup draw-brush
 *
 * Parallel brush marks through a polygon.
 */

#include <include/core/SkPoint.h>
#include <sigildraw/Constants.h>
#include <sigildraw/brush/Tool.h>
#include <sigilgeometry/kit/Hatches.h>

#include <span>

namespace sigil::draw {
class Pen;
}

namespace sigil::draw::brush {

/** Parallel marks clipped to a polygon. WHERE the lines lie is
 *  SigilGeometry's hatch — spacing, angle in radians, taper, origin,
 *  inset and a cross pass — and a mark is laid along each with the tool.
 *  Jitter is a fraction of the spacing and moves each mark's ends after
 *  the clip, by up to twice that fraction of the spacing, so a jittered
 *  mark may reach past the edge. Continuous joins the marks into one
 *  serpentine line. */
struct Hatch {
  /** @trap A pattern written out in place, `{.pattern = {.spacing = 8}}`,
   *  starts from the geometry value's own defaults, whose angle is zero:
   *  the quarter turn here is this member's default, not the pattern's. */
  geometry::shapes::Hatch pattern{.spacing = 5.0f, .angle = QUARTER_PI};
  float jitter = 0.0f;
  bool continuous = false;

  bool operator==(const Hatch&) const = default;
};

/** The pattern taper p5's gradient dial names: a share of a tenth of one
 *  step per lane, clamped to [-1, 1], spreading the lines for a positive
 *  dial and crowding them, by the inverse ratio, for a negative one. */
float gradientTaper(float gradient);

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
