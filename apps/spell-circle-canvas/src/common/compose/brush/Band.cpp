/** @file
 * band() — the strand region cut either side of a spine: the rails, their
 * arc-length sampling, the region between them, the point on a rail, the
 * profile offset, the two band factories, and `across()`, which installs the
 * engine that sweeps a width profile on the value it returns.
 */

#include <include/core/SkContourMeasure.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkShader.h>
#include <include/core/SkTypes.h>  // SkDebugf — the slot-rename diagnostic
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/pathops/SkPathOps.h>

#include <algorithm>
#include <cmath>   // std::isfinite — the profileOffset non-finite guard
#include <cstdio>  // std::snprintf — variationDrive's effect key
#include <set>

#include "ComposeInternal.h"
#include "SpanArithmetic.h"
#include "SpanContours.h"
#include "sigilgeometry/path/Contour.h"

namespace sigil::compose {

using namespace detail;
using detail::Kind;

namespace detail {

/** One of a band's two rails, as a Profile — so the rail is built by
 *  profileOffset and gets the SAME corner repair a relative strand gets.
 *  Formation is folded in here rather than at the sample site, which is
 *  what lets the two rails be two ordinary profiles.
 *
 *  `slice` remaps this contour's own [0,1] onto its span of the WHOLE
 *  spine, so a multi-contour spine keeps one continuous parameterisation
 *  even though the rails are built one contour at a time (which is what
 *  keeps the region from bridging between contours).
 *
 *  `spineLen` is the WHOLE spine's measured length, which is what a
 *  px-keyed base profile is evaluated against — the rail itself is always
 *  fraction-keyed (it is asked in fractions of its own contour), so the
 *  conversion happens here, once, on the way in. */
struct BandRail {
  Profile base;
  Formation formation = Formation::Centered;
  bool outer = true;
  float sliceStart = 0.0f, sliceSpan = 1.0f;
  float spineLen = 0.0f;
  bool operator==(const BandRail&) const = default;
  float max() const { return base.max(); }
  float across(float along) const {
    const float w = base.acrossAt(sliceStart + along * sliceSpan, spineLen);
    switch (formation) {
      case Formation::Centered:
        return outer ? w * 0.5f : -w * 0.5f;
      case Formation::Outward:
        return outer ? w : 0.0f;
      case Formation::Inward:
        return outer ? 0.0f : -w;
    }
    return 0.0f;
  }
};

/** Uniform arc-length samples of a rail, in ONE forward walk.
 *
 *  Do not implement this by calling bandPointAt per sample: that function
 *  re-measures the whole path on every call, which makes sampling quadratic
 *  in the sample count — and the sample count scales with the spine's
 *  length, so the cost grows fastest on exactly the large rings this is
 *  wanted for. Here the contours are measured once and the cursor only ever
 *  moves forward. */
std::vector<SkPoint> sampleRail(const SkPath& rail, int steps) {
  std::vector<SkPoint> out;
  std::vector<sk_sp<SkContourMeasure>> contours;
  float total = 0;
  SkContourMeasureIter iter(rail, false);
  while (sk_sp<SkContourMeasure> m = iter.next()) {
    total += m->length();
    contours.push_back(std::move(m));
  }
  if (total <= 0 || contours.empty()) return out;
  out.reserve((size_t)steps + 1);
  size_t at = 0;
  float consumed = 0;
  for (int k = 0; k <= steps; ++k) {
    const float want = total * (float)k / (float)steps;
    while (at + 1 < contours.size() &&
           want > consumed + contours[at]->length()) {
      consumed += contours[at]->length();
      ++at;
    }
    SkPoint pos;
    const float d = std::clamp(want - consumed, 0.0f, contours[at]->length());
    if (contours[at]->getPosTan(d, &pos, nullptr)) out.push_back(pos);
  }
  return out;
}

/** The contours of a path, each as its own path — so a rail pair can be
 *  zipped and CLOSED per contour instead of chained into one run. */
std::vector<std::pair<SkPath, float>> splitContours(const SkPath& path) {
  std::vector<std::pair<SkPath, float>> out;
  SkContourMeasureIter iter(path, false);
  while (sk_sp<SkContourMeasure> m = iter.next()) {
    const float len = m->length();
    if (len <= 0) continue;
    SkPathBuilder b;
    (void)m->getSegment(0, len, &b, true);
    if (m->isClosed()) b.close();
    out.emplace_back(b.detach(), len);
  }
  return out;
}

SkPath bandRegion(const SkPath& spine, const Across& width,
                  Formation formation) {
  const float reach = width.profile.max();
  if (spine.isEmpty() || reach <= 0) return SkPath();
  float total = 0;
  measureContours(spine, &total);
  if (total <= 0) return SkPath();

  // PER CONTOUR, and that is load-bearing: a single moveTo/lineTo chain
  // across all contours closed ONCE bridges between them with a filled
  // chord, so two concentric ring spines came out as a filled disc.
  //
  // BOTH RAILS GO THROUGH profileOffset, which is the other half: a
  // constant width then rides geometry::path::parallel's corner repair (real
  // vertices, arc outside a turn, miter inside) instead of a naive
  // sample-and-displace that leaves a spur on the inside of every
  // rectangle.
  //
  // Sign and frame, the one convention for the whole band family: positive
  // `across` is LEFT of travel, which with y pointing down is OUTSIDE a
  // clockwise path — and clockwise is SkPath's own direction for rects and
  // circles, so `.outward()` exits the shape. bandPointAt and
  // geometry::path::parallel mean the same side; a helper that flipped it would
  // turn every band inside out on one code path only.
  SkPathBuilder out;
  float consumed = 0;
  for (const auto& [contour, len] : splitContours(spine)) {
    const float sliceStart = total > 0 ? consumed / total : 0.0f;
    const float sliceSpan = total > 0 ? len / total : 1.0f;
    consumed += len;

    const SkPath outerRail =
        profileOffset(contour, Profile(BandRail{width.profile, formation, true,
                                                sliceStart, sliceSpan, total}));
    const SkPath innerRail =
        profileOffset(contour, Profile(BandRail{width.profile, formation, false,
                                                sliceStart, sliceSpan, total}));
    if (outerRail.isEmpty() || innerRail.isEmpty()) continue;

    // Zip by arc length rather than by index: geometry::path::parallel inserts
    // join geometry, so the two rails do not share a point count.
    const int steps = std::max(16, (int)std::ceil(len / 2.0f));
    const std::vector<SkPoint> outerPts = sampleRail(outerRail, steps);
    const std::vector<SkPoint> innerPts = sampleRail(innerRail, steps);
    if (outerPts.size() < 2 || innerPts.size() < 2) continue;

    out.moveTo(outerPts.front());
    for (size_t k = 1; k < outerPts.size(); ++k) out.lineTo(outerPts[k]);
    for (size_t k = innerPts.size(); k-- > 0;) out.lineTo(innerPts[k]);
    out.close();
  }
  return out.detach();
}

}  // namespace detail

SkPath bandRegion(const SkPath& spine, const Across& width,
                  Formation formation) {
  return detail::bandRegion(spine, width, formation);
}

// ---------------------------------------------------------------------------
// band()

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

SkPath profileOffset(const SkPath& spine, const Profile& profile) {
  if (spine.isEmpty()) return SkPath();
  float total = 0;
  measureContours(spine, &total);
  if (total <= 0) return SkPath();
  // A CONSTANT profile is a parallel, and geometry::path::parallel already does
  // parallels exactly — it finds the real vertices and joins them (arc
  // outside a turn, miter inside) instead of chording across. The naive
  // sample-and-displace walk below cannot: at a hard corner it offsets one
  // sampled point along ONE edge's normal, which leaves a spur on the
  // inside of every rectangle. Delegating rather than growing a second
  // corner repair here is deliberate — two repairs would drift apart.
  // No sign conversion is needed: geometry::path::parallel is LEFT of travel,
  // which is this file's frame exactly (see bandPointAt).
  //
  // Constancy is detected by SAMPLING, and that is a real limitation, not
  // a rounding detail: a stepped profile whose period divides the sample
  // spacing reads as constant. Sampled at 97 points (prime, so no profile
  // whose period is a simple fraction aligns with it) offset by half a
  // step (so a value read exactly at 0, 1/2, 1 cannot be the whole basis).
  // A profile that defeats this still gets a correct-shaped answer — the
  // exact-corner parallel — just not the varying one it asked for. If that
  // ever bites, the honest fix is a `constant()` query on the Profile
  // seam, which is additive.
  {
    const float first = profile.acrossAt(0.5f / 97.0f, total);
    bool constant = true;
    for (int k = 1; k < 97 && constant; ++k)
      constant = profile.acrossAt(((float)k + 0.5f) / 97.0f, total) == first;
    if (constant)
      return first == 0.0f ? spine : geometry::path::parallel(spine, first);
  }
  SkPathBuilder out;
  SkContourMeasureIter iter(spine, false);
  float consumed = 0;
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    const float base = consumed;
    consumed += len;
    if (len <= 0) continue;
    const int steps = std::max(8, (int)std::ceil(len / 2.0f));
    bool started = false;
    for (int k = 0; k <= steps; ++k) {
      const float d = len * (float)k / (float)steps;
      SkPoint pos;
      SkVector tan;
      if (!contour->getPosTan(d, &pos, &tan)) continue;
      // The band's frame: positive across is LEFT of travel, which with y
      // down is outside a clockwise path. One body for the band's rails
      // and a relative strand, so the two cannot drift apart.
      float w = profile.acrossAt(total > 0 ? (base + d) / total : 0.0f, total);
      // ONE non-finite sample would delete the WHOLE band: Skia draws none
      // of a path that contains a non-finite vertex, and says nothing. An
      // author profile only has to misbehave at a single parameter value to
      // hit this — sqrt(sin(pi*along)) rounds to a tiny negative at
      // along == 1 — so a non-finite width is clamped to a LOCAL pinch down
      // to the spine rather than allowed to erase everything.
      if (!std::isfinite(w)) w = 0.0f;
      const SkPoint at{pos.fX + tan.y() * w, pos.fY - tan.x() * w};
      if (!started) {
        out.moveTo(at);
        started = true;
      } else {
        out.lineTo(at);
      }
    }
    if (started && contour->isClosed()) out.close();
  }
  return out.detach();
}

Across across(float px) {
  Across out{strand::offset(px)};
  out.resolver = detail::strokeResolver();
  return out;
}
Across across(Profile p) {
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
  return e;
}

}  // namespace sigil::compose
