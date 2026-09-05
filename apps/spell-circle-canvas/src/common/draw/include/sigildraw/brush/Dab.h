#pragma once

/** @file
 * Device input, and the evenly spaced dabs a tool deposits.
 */

#include <include/core/SkPoint.h>

namespace sigil::draw::brush {

/** One device observation. Pressure is a unit value. Tilt is zero for an
 *  upright stylus and one for a stylus flat against the surface. Barrel
 *  and tilt-direction angles are radians, and seconds is the host's
 *  clock. */
struct Input {
  SkPoint position{0, 0};
  float pressure = 1.0f;
  float tilt = 0.0f;
  float barrelRotation = 0.0f;
  double seconds = 0.0;
  float tiltDirection = 0.0f;

  bool operator==(const Input&) const = default;
};

/** One evenly spaced deposition event. Direction is the centreline
 *  tangent in radians, clockwise-positive on the y-down canvas as the
 *  pen's `rotate` is; speed is canvas units per second; distance is
 *  measured from the beginning of the stroke and progress is the unit
 *  position within a completed one. */
struct Dab {
  SkPoint position{0, 0};
  float pressure = 1.0f;
  float tilt = 0.0f;
  float barrelRotation = 0.0f;
  float direction = 0.0f;
  float speed = 0.0f;
  float distance = 0.0f;
  float progress = 0.0f;
  float tiltDirection = 0.0f;

  bool operator==(const Dab&) const = default;
};

}  // namespace sigil::draw::brush
