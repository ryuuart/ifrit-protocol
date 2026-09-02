/** @file
 * band() — the point on a rail, the two band factories, and `across()`,
 * which installs the engine that sweeps a width profile on the value it
 * returns. The rails themselves, and the region between them, are
 * SigilGeometry's (`geometry::path::bandRegion`).
 */

#include <include/core/SkContourMeasure.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRefCnt.h>

#include <algorithm>
#include <utility>

#include "ComposeInternal.h"
#include "SpanArithmetic.h"
#include "SpanContours.h"

namespace sigil::compose {

using namespace detail;

SkPoint bandPointAt(const SkPath& spine, float along, float acrossPx) {
  float total = 0;
  measureContours(spine, &total);
  if (total <= 0) return {0, 0};
  const float want = std::clamp(along, 0.0f, 1.0f) * total;
  float consumed = 0;
  SkContourMeasureIter iter(spine, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    if (want <= consumed + len || consumed + len >= total - 1e-4f) {
      SkPoint pos;
      SkVector tan;
      const float d = std::clamp(want - consumed, 0.0f, len);
      if (contour->getPosTan(d, &pos, &tan))
        return {pos.fX + tan.y() * acrossPx, pos.fY - tan.x() * acrossPx};
      return pos;
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

Element band(Shape spine, Across width) {
  Element e;
  detail::DeriveData& derive = e.node()->deriveData.ensure();
  derive.bandSpine = std::move(spine);
  derive.bandWidth = std::move(width);
  return e;
}

Element band(Around spine, Across width) {
  Element e;
  detail::DeriveData& derive = e.node()->deriveData.ensure();
  derive.bandAround = std::move(spine.key);
  derive.bandWidth = std::move(width);
  // A borrowed spine is the target's SHAPE swept at a width, so what the
  // band waits for is that node's outline.
  derive.reads.push_back({derive.bandAround, sigil::core::Facet::Outline});
  return e;
}

}  // namespace sigil::compose
