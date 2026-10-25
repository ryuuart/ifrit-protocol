#pragma once

/** @file
 * @ingroup material-paint
 *
 * THE GRADIENT BASES of a material: the three gradients as materials, so
 * `material::from(linearGradient(…)).layer(…)` builds up from one and
 * every place that takes a material takes one, in two spellings: CSS's,
 * the stops first and the direction, ending shape or start after them,
 * and the point-to-point form beneath it. Box units by default: the
 * points are fractions of the box the material paints.
 */

#include <sigilmaterial/core/Gradient.h>
#include <sigilmaterial/core/Material.h>

#include <glm/vec2.hpp>

namespace sigil::material {

/** THE LINEAR GRADIENT from @p start to @p end over @p stops. */
Material linearGradient(glm::vec2 start, glm::vec2 end, ColorStops stops,
                        GradientOptions options = {});
/** THE RADIAL GRADIENT out of @p center to @p radius; in box units a
 *  centred radius of 1 reaches the box's corners. */
Material radialGradient(glm::vec2 center, float radius, ColorStops stops,
                        GradientOptions options = {});
/** THE CONIC GRADIENT: the stops swept around @p center over the window
 *  `options.startDegrees` to `options.endDegrees`, clockwise from 12
 *  o'clock. */
Material conicGradient(glm::vec2 center, ColorStops stops,
                       GradientOptions options = {});

/** @name The CSS-shaped gradients
 *  The stops first, then what CSS writes before them, measured against
 *  the box the material paints: `linearGradient(stops)` runs to the
 *  bottom, `linearGradient(stops, 90)` to the right,
 *  `linearGradient(stops, Toward::TopLeft)` to the top-left corner. The
 *  point-to-point forms above are the advanced spelling.
 *  @{ */
/** CSS's `linear-gradient`: the stops along @p direction across the
 *  box. */
Material linearGradient(ColorStops stops, LinearDirection direction = {},
                        GradientOptions options = {});
/** CSS's `radial-gradient`: the stops out from `shape.at` to the ending
 *  shape @p shape names. */
Material radialGradient(ColorStops stops, RadialEndingShape shape = {},
                        GradientOptions options = {});
/** CSS's `conic-gradient`: the stops swept clockwise around `start.at`
 *  from `start.fromDegrees`, 0 at 12 o'clock; `options.startDegrees` and
 *  `options.endDegrees` narrow the window, measured from that start. */
Material conicGradient(ColorStops stops, ConicStart start = {},
                       GradientOptions options = {});
/** @} */

}  // namespace sigil::material
