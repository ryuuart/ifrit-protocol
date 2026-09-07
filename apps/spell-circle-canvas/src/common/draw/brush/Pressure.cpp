/** @file
 * The pressure envelope evaluated along a stroke.
 */

#include <sigildraw/brush/Pressure.h>

#include <algorithm>
#include <cmath>

namespace sigil::draw::brush {

float Pressure::at(float progress) const {
  progress = std::clamp(progress, 0.0f, 1.0f);
  if (curve) return std::max(0.0f, curve(progress));
  if (gaussian) {
    // The bell rises over a wider half than it falls, as a hand does.
    const float halfWidth = std::max(
        0.0001f, gaussian->width * (progress < gaussian->center ? 1.2f : 0.8f));
    const float distance = std::abs((progress - gaussian->center) / halfWidth);
    const float value =
        1.0f / (1.0f + std::pow(distance, 2.0f * gaussian->sharpness));
    return gaussian->minimum + (gaussian->maximum - gaussian->minimum) * value;
  }
  return progress < 0.5f ? start + (middle - start) * progress * 2.0f
                         : middle + (end - middle) * (progress - 0.5f) * 2.0f;
}

Pressure Pressure::gaussianProfile(float centerJitter, float widthJitter,
                                   float minimum, float maximum) {
  Pressure result;
  result.gaussian = Gaussian{.minimum = minimum,
                             .maximum = maximum,
                             .centerJitter = std::max(0.0f, centerJitter),
                             .widthJitter = std::max(0.0f, widthJitter)};
  return result;
}

}  // namespace sigil::draw::brush
