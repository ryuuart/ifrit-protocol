/** @file
 * The rails a width law cuts either side of a spine, their arc-length
 * sampling, the region between them — and the OTHER construction of a
 * band, the union of its cross-sections, which is the one a join
 * vocabulary and a direction-keyed width law both need.
 */

#include "sigilgeometry/path/Band.h"

#include <include/core/SkContourMeasure.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRefCnt.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/vec2.hpp>
#include <span>
#include <utility>
#include <vector>

#include "sigilgeometry/path/Contour.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::geometry::path {

namespace {

/** The spine's total arc length, measured once. */
float totalLength(const SkPath& path) {
  float total = 0;
  SkContourMeasureIter iter(path, false);
  while (sk_sp<SkContourMeasure> m = iter.next()) total += m->length();
  return total;
}

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
 *  Do not implement this by re-measuring the path per sample: that makes
 *  sampling quadratic in the sample count — and the sample count scales
 *  with the spine's length, so the cost grows fastest on exactly the large
 *  rings this is wanted for. Here the contours are measured once and the
 *  cursor only ever moves forward. */
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

SkPath bandRegionImpl(const SkPath& spine, const Profile& width,
                      Formation formation) {
  const float reach = width.max();
  if (spine.isEmpty() || reach <= 0) return SkPath();
  const float total = totalLength(spine);
  if (total <= 0) return SkPath();

  // PER CONTOUR, and that is load-bearing: a single moveTo/lineTo chain
  // across all contours closed ONCE bridges between them with a filled
  // chord, which fills the gap between two concentric ring spines.
  //
  // BOTH RAILS GO THROUGH profileOffset, which is the other half: a
  // constant width then rides parallel's corner repair (real
  // vertices, arc outside a turn, miter inside) instead of a naive
  // sample-and-displace that leaves a spur on the inside of every
  // rectangle.
  //
  // Sign and frame, the one convention for the whole band family: positive
  // `across` is LEFT of travel, which with y pointing down is OUTSIDE a
  // clockwise path — and clockwise is SkPath's own direction for rects and
  // circles, so an outward formation exits the shape. `parallel` means
  // the same side; a helper that flipped it would turn every band inside
  // out on one code path only.
  SkPathBuilder out(spine.getFillType());
  float consumed = 0;
  for (const auto& [contour, len] : splitContours(spine)) {
    const float sliceStart = total > 0 ? consumed / total : 0.0f;
    const float sliceSpan = total > 0 ? len / total : 1.0f;
    consumed += len;

    const SkPath outerRail =
        profileOffset(contour, Profile(BandRail{width, formation, true,
                                                sliceStart, sliceSpan, total}));
    const SkPath innerRail =
        profileOffset(contour, Profile(BandRail{width, formation, false,
                                                sliceStart, sliceSpan, total}));
    if (outerRail.isEmpty() || innerRail.isEmpty()) continue;

    // Zip by arc length rather than by index: parallel inserts
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

}  // namespace

SkPath bandRegion(const SkPath& spine, const Profile& width,
                  Formation formation) {
  return bandRegionImpl(spine, width, formation);
}

namespace {

/** Add one convex piece of a swept band, wound the way every other piece
 *  is.
 *
 *  EVERY sub-polygon must wind the same way. Under the winding fill a
 *  reversed piece laid over another cancels to 0 and punches a hole where
 *  the two overlap, which is precisely the overlap the inside of a bend is
 *  made of. Cheap to enforce here, and impossible to see coming from the
 *  picture. The winding question is asked over the stack: at most four
 *  points per piece, and one piece per step of every band in the frame. */
void addBandPiece(SkPathBuilder& b, const SkPoint* pts, size_t n) {
  if (n < 3) return;
  std::array<glm::vec2, 4> ring{};
  for (size_t i = 0; i < n && i < ring.size(); ++i) ring[i] = fromSk(pts[i]);
  b.moveTo(pts[0]);
  if (signedArea(std::span(ring).first(n)) > 0)
    for (size_t i = n; i-- > 1;) b.lineTo(pts[i]);
  else
    for (size_t i = 1; i < n; ++i) b.lineTo(pts[i]);
  b.close();
}

}  // namespace

SkPath sweptRegion(const SkPath& spine, const SweepWidth& width,
                   const Sweep& sweep) {
  SkPathBuilder band;
  if (!width) return SkPath();
  const float stride = std::max(sweep.stepPx, 0.5f);
  const float limit = std::max(sweep.miterLimit, 1.0f);
  SkContourMeasureIter iter(spine, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    // The stations: where the spine is, and how wide the band is there.
    std::vector<SkPoint> pos;
    std::vector<float> half;
    for (float d = 0;; d += stride) {
      const float at = std::min(d, len);
      SkPoint here;
      SkVector tan;
      if (!contour->getPosTan(at, &here, &tan)) break;
      float w = width(SweepStation{here, tan, at, len > 0 ? at / len : 0, len});
      // A NON-FINITE WIDTH PINCHES TO THE SPINE rather than poisoning the
      // band. Skia draws NONE of a path holding one non-finite vertex, so
      // a law that returns NaN at a single sample would delete the whole
      // mark, silently and with nothing on screen to say why; a local
      // pinch fails where the law failed.
      if (!std::isfinite(w)) w = 0.0f;
      pos.push_back(here);
      half.push_back(w * 0.5f);
      if (at >= len) break;
    }
    if (pos.size() < 2) continue;

    // One quadrilateral per step, each wound the same way, so the band is
    // the UNION of its cross-sections rather than one contour that
    // crosses itself where the spine turns hard.
    std::vector<SkVector> normal(pos.size() - 1);
    for (size_t i = 0; i + 1 < pos.size(); ++i) {
      const SkVector e{pos[i + 1].x() - pos[i].x(),
                       pos[i + 1].y() - pos[i].y()};
      const float L = std::hypot(e.x(), e.y());
      if (L < 1e-4f) {
        normal[i] = {0, 0};
        continue;
      }
      const SkVector n{-e.y() / L, e.x() / L};
      normal[i] = n;
      const SkPoint quad[4]{
          {pos[i].x() + n.x() * half[i], pos[i].y() + n.y() * half[i]},
          {pos[i + 1].x() + n.x() * half[i + 1],
           pos[i + 1].y() + n.y() * half[i + 1]},
          {pos[i + 1].x() - n.x() * half[i + 1],
           pos[i + 1].y() - n.y() * half[i + 1]},
          {pos[i].x() - n.x() * half[i], pos[i].y() - n.y() * half[i]}};
      addBandPiece(band, quad, 4);
    }

    // The joins, at every station two steps meet on. Both sides are
    // emitted and the winding fill absorbs the one on the inside of the
    // turn, which is cheaper and steadier than deciding which side is
    // outside from a cross product that vanishes on a straight run.
    for (size_t i = 1; i + 1 < pos.size(); ++i) {
      const SkVector& n0 = normal[i - 1];
      const SkVector& n1 = normal[i];
      const float turn = n0.x() * n1.y() - n0.y() * n1.x();
      const float h = half[i];
      // Below about a tenth of a pixel of opening there is nothing to
      // close, which is every station of a smoothly sampled curve.
      if (h <= 0 || std::abs(turn) * h < 0.1f) continue;
      if (sweep.join == SweepJoin::Round) {
        band.addCircle(pos[i].x(), pos[i].y(), h, SkPathDirection::kCCW);
        continue;
      }
      for (int side = -1; side <= 1; side += 2) {
        const float s = (float)side;
        const SkPoint a{pos[i].x() + n0.x() * h * s,
                        pos[i].y() + n0.y() * h * s};
        const SkPoint b{pos[i].x() + n1.x() * h * s,
                        pos[i].y() + n1.y() * h * s};
        if (sweep.join == SweepJoin::Miter) {
          // The rails meet where the bisector carries them. `cos` is the
          // half-angle's cosine, and 1/cos is the reach in half-widths —
          // Skia's miter limit, in the same units Skia states it in.
          SkVector m{n0.x() + n1.x(), n0.y() + n1.y()};
          const float mag = std::hypot(m.x(), m.y());
          const float cosHalf = mag * 0.5f;
          if (mag > 1e-4f && cosHalf > 1e-3f && 1.0f / cosHalf <= limit) {
            const float reach = h / cosHalf;
            const SkPoint tip{pos[i].x() + m.x() / mag * reach * s,
                              pos[i].y() + m.y() / mag * reach * s};
            const SkPoint wedge[4]{pos[i], a, tip, b};
            addBandPiece(band, wedge, 4);
            continue;
          }
        }
        const SkPoint wedge[3]{pos[i], a, b};
        addBandPiece(band, wedge, 3);
      }
    }
  }
  SkPath path = band.detach();
  path.setFillType(SkPathFillType::kWinding);
  return path;
}

SkPath profileOffset(const SkPath& spine, const Profile& profile) {
  if (spine.isEmpty()) return SkPath();
  const float total = totalLength(spine);
  if (total <= 0) return SkPath();
  // A CONSTANT profile is a parallel, and parallel already does
  // parallels exactly — it finds the real vertices and joins them (arc
  // outside a turn, miter inside) instead of chording across. The naive
  // sample-and-displace walk below cannot: at a hard corner it offsets one
  // sampled point along ONE edge's normal, which leaves a spur on the
  // inside of every rectangle. Delegating rather than growing a second
  // corner repair here is deliberate — two repairs would drift apart.
  // No sign conversion is needed: `parallel` is LEFT of travel, which is
  // this file's frame exactly.
  //
  // Constancy is detected by SAMPLING, and that is a real limitation, not
  // a rounding detail: a stepped profile whose period divides the sample
  // spacing reads as constant. Sampled at 97 points (prime, so no profile
  // whose period is a simple fraction aligns with it) offset by half a
  // step (so a value read exactly at 0, 1/2, 1 cannot be the whole basis).
  // A profile that defeats this still gets a correct-shaped answer — the
  // exact-corner parallel — just not the varying one it asked for.
  {
    const float first = profile.acrossAt(0.5f / 97.0f, total);
    bool constant = true;
    for (int k = 1; k < 97 && constant; ++k)
      constant = profile.acrossAt(((float)k + 0.5f) / 97.0f, total) == first;
    if (constant) return first == 0.0f ? spine : parallel(spine, first);
  }
  SkPathBuilder out(spine.getFillType());
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

}  // namespace sigil::geometry::path
