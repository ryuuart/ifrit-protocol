/** @file
 * band() — the point on a band's spine, the band factory, and `across()`,
 * which installs the engine that sweeps a width profile on the value it
 * returns. The rails of the band itself, and the region between them,
 * are SigilGeometry's (`geometry::path::bandRegion`).
 */

#include <include/core/SkPoint.h>
#include <include/core/SkRefCnt.h>

#include <algorithm>
#include <utility>

#include "ComposeInternal.h"
#include "SpanArithmetic.h"
#include "SpanContours.h"
#include "sigilgeometry/path/Contour.h"

namespace sigil::compose {

using namespace detail;

SkPoint bandPointAt(const SkPath& spine, float along, float acrossPx) {
  float total = 0;
  measureContours(spine, &total);
  if (total <= 0) return {0, 0};
  const float want = std::clamp(along, 0.0f, 1.0f) * total;
  float consumed = 0;
  for (const geometry::path::Contour& contour :
       geometry::path::Contour::of(spine)) {
    const float len = contour.length();
    if (want <= consumed + len || consumed + len >= total - 1e-4f) {
      const float d = std::clamp(want - consumed, 0.0f, len);
      if (const auto sample = contour.at(d))
        return {sample->position.x + sample->tangent.y * acrossPx,
                sample->position.y - sample->tangent.x * acrossPx};
      return {0, 0};
    }
    consumed += len;
  }
  return {0, 0};
}

Across across(float px) {
  Across out{geometry::path::profile::offset(px)};
  out.resolver = detail::strokeResolver();
  return out;
}
Across across(geometry::path::Profile p) {
  Across out{std::move(p)};
  out.resolver = detail::strokeResolver();
  return out;
}

Band band(Shape spine, Across width) {
  Band e{std::make_shared<detail::ElementNode>()};
  detail::DeriveData& derive = e.node()->deriveData.ensure();
  derive.bandSpine = std::move(spine);
  derive.bandWidth = std::move(width);
  return e;
}

}  // namespace sigil::compose
