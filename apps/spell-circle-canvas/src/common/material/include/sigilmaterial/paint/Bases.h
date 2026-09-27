#pragma once

/** @file
 * @ingroup material-paint
 *
 * THE GRADIENT BASES of a material: the three gradients as materials, so
 * `material::from(linearGradient(…)).layer(…)` builds up from one and
 * every place that takes a material takes one. Box units by default: the
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
 *  `options.startDegrees` to `options.endDegrees`. */
Material conicGradient(glm::vec2 center, ColorStops stops,
                       GradientOptions options = {});

}  // namespace sigil::material
