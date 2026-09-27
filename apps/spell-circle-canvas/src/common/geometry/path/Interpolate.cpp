/** @file
 * The exact in-between of two compatible outlines, over Skia's own
 * point-array interpolation.
 */

#include "sigilgeometry/advanced/Skia.h"
#include "sigilgeometry/path/Interpolate.h"

#include "sigilgeometry/path/Segments.h"

namespace sigil::geometry::path {

std::optional<SkPath> interpolate(const SkPath& a, const SkPath& b, float t) {
  if (compatible(a, b) != Compatible::Yes) return std::nullopt;
  if (!a.isInterpolatable(b)) return std::nullopt;
  SkPath out;
  // Skia's weight runs the other way: one is this path, zero the other.
  if (!a.interpolate(b, 1.0f - t, &out)) return std::nullopt;
  return out;
}

std::optional<Outline> interpolate(const Outline& a, const Outline& b,
                                   float t) {
  std::optional<SkPath> between = interpolate(toSk(a), toSk(b), t);
  if (!between) return std::nullopt;
  return fromSk(std::move(*between));
}

}  // namespace sigil::geometry::path
