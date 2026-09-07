/** @file
 * The ramp read: the domain onto [0, 1], the walk between two stops in
 * the space the ramp names, and the two crossings between a ramp and a
 * fixed table.
 */

#include "sigilmaterial/color/Ramp.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>

namespace sigil::material {
namespace {

/** A STOP LIST OUT OF ORDER, said once for the process. The read walks
 *  the stops in the order given and takes the first and the last as the
 *  extremes, so an unsorted list answers a colour that is wrong with
 *  nothing else to show for it — and a caller who built the list from a
 *  map or a set is the one who cannot see that from the picture. */
void reportUnorderedStops() {
  static std::once_flag once;
  std::call_once(once, [] {
    std::fprintf(stderr,
                 "[sigil::material] a Ramp's stops are not in position "
                 "order; the read takes the first and last stops as the "
                 "ends and walks the rest in the order given, so the "
                 "colours it answers are not the ramp that was meant. "
                 "Sort the stops by position. (said once)\n");
  });
}

bool inOrder(const std::vector<RampStop>& stops) {
  for (size_t i = 1; i < stops.size(); ++i)
    if (stops[i].pos < stops[i - 1].pos) return false;
  return true;
}

/** The hue @p b reached from @p a along the arc the ramp names, as an
 *  absolute angle that may run outside [0, 360) so the mix walks it
 *  without folding. */
float hueAlong(float a, float b, HueArc arc) {
  float delta = std::fmod(b - a, 360.0f);
  if (delta < 0.0f) delta += 360.0f;
  switch (arc) {
    case HueArc::Shorter:
      if (delta > 180.0f) delta -= 360.0f;
      break;
    case HueArc::Longer:
      if (delta < 180.0f) delta -= 360.0f;
      break;
    case HueArc::Increasing:
      break;
    case HueArc::Decreasing:
      if (delta > 0.0f) delta -= 360.0f;
      break;
  }
  return a + delta;
}

Color mixIn(RampSpace space, HueArc arc, const Color& a, const Color& b,
            float t) {
  switch (space) {
    case RampSpace::Srgb:
      return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
              a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
    case RampSpace::Linear:
      return mixLinear(a, b, t);
    case RampSpace::Oklab:
      return lerpOklab(a, b, t);
    case RampSpace::Oklch: {
      const Oklch la = toOklch(a), lb = toOklch(b);
      // A grey has no direction, so its hue is whatever the arithmetic
      // left in it: take the other end's, which is what keeps a ramp
      // from black to a colour on that colour's hue instead of swinging
      // through an angle nobody asked for.
      const float ha = la.chroma > 0.0f ? la.hueDegrees : lb.hueDegrees;
      const float hb = lb.chroma > 0.0f ? lb.hueDegrees : la.hueDegrees;
      const float hue = ha + (hueAlong(ha, hb, arc) - ha) * t;
      return fromOklch({la.L + (lb.L - la.L) * t,
                        la.chroma + (lb.chroma - la.chroma) * t, hue,
                        la.alpha + (lb.alpha - la.alpha) * t});
    }
  }
  return a;
}

}  // namespace

float Ramp::position(float value) const {
  const float span = domainHigh - domainLow;
  float u = span != 0.0f ? (value - domainLow) / span : 0.0f;
  u = std::clamp(u, 0.0f, 1.0f);
  if (reverse) u = 1.0f - u;
  if (easing.shape) u = std::clamp(easing.at(u), 0.0f, 1.0f);
  return u;
}

Color Ramp::at(float value) const {
  if (stops.empty()) return {0, 0, 0, 0};
  if (!inOrder(stops)) reportUnorderedStops();
  // The same search every reading of a ramp makes; what this one adds is
  // the SPACE the walk between the two stops happens in.
  const RampBracket span = rampBracket(stops, position(value));
  const Color& low = stops[span.low].color;
  const Color& high = stops[span.high].color;
  if (span.fraction <= 0.0f) return low;
  if (span.fraction >= 1.0f) return high;
  return mixIn(space, arc, low, high, span.fraction);
}

Palette palette(const Ramp& ramp, int entries) {
  Palette table;
  if (entries <= 0) return table;
  table.entries.reserve((size_t)entries);
  const float span = ramp.domainHigh - ramp.domainLow;
  for (int i = 0; i < entries; ++i) {
    const float centre = ((float)i + 0.5f) / (float)entries;
    table.entries.push_back(ramp.at(ramp.domainLow + centre * span));
  }
  return table;
}

Ramp ramp(const Palette& palette, RampSpace space) {
  Ramp built;
  built.space = space;
  const size_t count = palette.entries.size();
  built.stops.reserve(count);
  for (size_t i = 0; i < count; ++i)
    built.stops.push_back(
        {count > 1 ? (float)i / (float)(count - 1) : 0.0f, palette.entries[i]});
  return built;
}

}  // namespace sigil::material
