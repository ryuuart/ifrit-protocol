#pragma once

/** @file
 * Snapping an animated layout input to a grid before it enters a guard key,
 * so a measure that breathes by a fraction of a pixel per frame poses the
 * same layout problem frame after frame and re-hits the cache.
 */

#include <cmath>

namespace sigil::weave::kit {

/** Snaps `value` to whole multiples of `step`.
 *
 * The companion trick for animated layout inputs: a breathing measure or
 * drifting box moves well under a pixel per frame, so quantizing it before
 * it enters a guard key means most frames pose the *same* layout problem
 * and re-hit the cache — and sub-pixel measure changes are invisible
 * anyway. Pick `step` as the coarsest granularity the effect can't show.
 */
[[nodiscard]] inline float quantize(float value, float step = 1.0f) {
  return std::round(value / step) * step;
}

}  // namespace sigil::weave::kit
