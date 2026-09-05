#pragma once

/** @file
 * A ramp of stops as Skia takes it.
 *
 * `RampStop` is the colour leaf's value — a position and a colour, and
 * nothing about how it is drawn. These two are the crossing: the same
 * stops as a shader over a vertical span in the coordinates a node is
 * painted in, and as a paint over the unit square, which is what a text
 * fill and a mask take. Both clamp outside their span, because a ramp
 * carries no answer for what lies beyond its ends.
 */

#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Paint.h>

#include <vector>

namespace sigil::material::skia {

/** @p ramp as a gradient running down from @p y0 to @p y1. */
sk_sp<SkShader> verticalRamp(float y0, float y1,
                             const std::vector<RampStop>& ramp);

/** The same stops over the unit square, top to bottom. */
Paint unitRamp(const std::vector<RampStop>& ramp);

}  // namespace sigil::material::skia
