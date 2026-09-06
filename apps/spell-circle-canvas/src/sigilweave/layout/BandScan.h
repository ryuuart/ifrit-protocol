#pragma once

/** @file
 * THE SCAN EVERY BAND ANSWER IS BUILT FROM, private to the layout stage:
 * the two coordinates a flow works in, and the occupied stretches of one
 * band of a flattened outline.
 *
 * `along` is the way the pen travels on a flow's lines; `across` is the
 * way its bands stack. A line flow reads x along and y across, a column
 * flow reads y along and x across, and that swap is the whole of the
 * difference between wrapping around a shape in lines and wrapping around
 * it in columns — which is why one scan serves both and there is no second
 * implementation for columns anywhere.
 */

#include <include/core/SkRect.h>

#include <glm/vec2.hpp>
#include <utility>
#include <vector>

#include "sigilweave/layout/Flow.h"

namespace sigil::weave::detail {

/// The slack a band scan works to: a hundredth of a layout unit, which is
/// finer than any standoff a reader can see and coarse enough that a
/// scanline never lands exactly on a vertex.
inline constexpr float kBandEpsilon = 0.01f;

float alongOf(FlowAxis axis, const glm::vec2& point);
float acrossOf(FlowAxis axis, const glm::vec2& point);
float alongMin(FlowAxis axis, const SkRect& rect);
float alongMax(FlowAxis axis, const SkRect& rect);
float acrossMin(FlowAxis axis, const SkRect& rect);
float acrossMax(FlowAxis axis, const SkRect& rect);

/** Occupied ALONG-intervals of a flattened polygon set within the band
 *  [@p bandStart, @p bandEnd] measured ACROSS: fill intervals sampled at
 *  three scanlines (respecting the fill rule, so holes and concave gaps
 *  stay open) unioned with every edge's along-travel through the band
 *  (conservative — it catches features that fall between the samples, like
 *  a star tip). Appends unmerged occupied spans. */
void bandOccupancy(const std::vector<std::vector<glm::vec2>>& contours,
                   bool evenOdd, FlowAxis axis, float bandStart, float bandEnd,
                   std::vector<std::pair<float, float>>& occupiedSpans);

/** Sorts @p spans and unions every pair that touches or overlaps. */
void mergeSpans(std::vector<std::pair<float, float>>& spans);

}  // namespace sigil::weave::detail
