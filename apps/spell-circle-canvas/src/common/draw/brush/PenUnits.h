#pragma once

/** @file
 * What the engine reads off the pen: the units an angle beside it is in,
 * and the clock a field is read at. Private to the library.
 */

#include <sigildraw/Math.h>
#include <sigildraw/Pen.h>

namespace sigil::draw::brush {

/** @p angle, given in the pen's angle mode, as radians. */
inline float toRadians(const Pen& pen, float angle) {
  return pen.angleMode() == DEGREES ? radians(angle) : angle;
}

/** The pen's clock in the seconds a field takes. */
inline float fieldSeconds(const Pen& pen) {
  return (float)(pen.millis() / 1000.0);
}

}  // namespace sigil::draw::brush
