#pragma once

/** @file
 * Internal to the kernel — a fill's path: the stops a fill transition runs
 * through, and the colour that path stands at partway along it.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmotion/ease/Ease.h>

#include <functional>
#include <span>

#include "description/ComposeInternal.h"

namespace sigil::compose::detail {

/** ONE STOP ON A FILL'S PATH: the fill, where in the path's 0→1 progress
 *  it stands, and the curve the step arriving at it is read through. */
struct FillStop {
  Fill fill;
  float at = 0.0f;
  motion::Easing ease;
};

/** THE COLOUR A FILL PATH STANDS AT @p progress through it: the two stops
 *  around the progress, each read through @p resolve — which turns a
 *  reference into the colour it names — mixed through the arriving
 *  stop's curve. A step that lasts nothing is taken at once. */
material::Color colourAlong(std::span<const FillStop> path, float progress,
                            const std::function<Fill(const Fill&)>& resolve);

}  // namespace sigil::compose::detail
