/** @file
 * The response curve, and what each drive reads off a dab.
 */

#include <sigildraw/brush/Dab.h>
#include <sigildraw/brush/Dynamics.h>

#include <algorithm>
#include <cmath>

namespace sigil::draw::brush {

float Curve::at(float input) const {
  const float unit = std::clamp(input, 0.0f, 1.0f);
  if (curve) return curve(unit);
  const float shaped =
      bend == 1.0f ? unit : std::pow(unit, std::max(0.01f, bend));
  return minimum + (maximum - minimum) * shaped;
}

float Response::at(const Dab& dab, float pressure,
                   float speedReference) const {
  switch (drive) {
    case Drive::Pressure:
      return curve.at(pressure);
    case Drive::Velocity:
      return curve.at(dab.speed / std::max(1.0f, speedReference));
    case Drive::Tilt:
      return curve.at(dab.tilt);
  }
  return 1.0f;
}

}  // namespace sigil::draw::brush
