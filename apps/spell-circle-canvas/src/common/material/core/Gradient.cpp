/** @file
 * The stops a gradient takes, out of plain colours and out of a ramp.
 */

#include "sigilmaterial/core/Gradient.h"

#include <algorithm>

namespace sigil::material {

namespace {

/** How many readings a ramp that cannot hand over its own stops is
 *  written as: enough that a walk through OKLab or an eased curve shows
 *  no straight segments at the sizes a gradient is painted at. */
constexpr int kRampReadings = 64;

}  // namespace

ColorStops::ColorStops(const std::vector<Color>& colors)
    : m_evenlySpaced(true) {
  const size_t count = colors.size();
  m_stops.reserve(count);
  for (size_t i = 0; i < count; ++i)
    m_stops.push_back(
        {count > 1 ? (float)i / (float)(count - 1) : 0.0f, colors[i]});
}

ColorStops::ColorStops(const Ramp& ramp) {
  if (ramp.stops.empty()) return;
  if (ramp.space == RampSpace::Srgb && !ramp.easing.shape) {
    m_stops = ramp.stops;
    if (ramp.reverse) {
      std::reverse(m_stops.begin(), m_stops.end());
      for (ColorStop& stop : m_stops) stop.offset = 1.0f - stop.offset;
    }
    return;
  }
  m_stops.reserve(kRampReadings + 1);
  const float span = ramp.domainHigh - ramp.domainLow;
  for (int i = 0; i <= kRampReadings; ++i) {
    const float offset = (float)i / (float)kRampReadings;
    m_stops.push_back({offset, ramp.at(ramp.domainLow + offset * span)});
  }
  m_evenlySpaced = true;
}

}  // namespace sigil::material
