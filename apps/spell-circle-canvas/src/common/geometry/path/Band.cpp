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

#include "OffsetInternal.h"
#include "sigilgeometry/advanced/Skia.h"
#include "sigilgeometry/path/Contour.h"
#include "sigilgeometry/path/Polyline.h"

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
  Formation formation = Formation::Center;
  bool outer = true;
  float sliceStart = 0.0f, sliceSpan = 1.0f;
  float spineLen = 0.0f;
  bool operator==(const BandRail&) const = default;
  float max() const { return base.max(); }
  float across(float along) const {
    const float w = base.acrossAt(sliceStart + along * sliceSpan, spineLen);
    switch (formation) {
      case Formation::Center:
        return outer ? w * 0.5f : -w * 0.5f;
      case Formation::Outer:
        return outer ? w : 0.0f;
      case Formation::Inner:
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
  // BOTH RAILS GO THROUGH profileOffset, which is the other half: every
  // rail then takes the real-vertex repair (arc outside a turn, miter
  // inside) rather than the spur a sample-and-displace walk leaves on the
  // inside of every rectangle.
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
      float w = width(SweepStation{fromSk(here), fromSk(tan), at,
                                   len > 0 ? at / len : 0, len});
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
    // A station closer to the last than a ten-thousandth of a pixel has no
    // direction of its own: it is the same station.
    {
      size_t kept = 1;
      for (size_t i = 1; i < pos.size(); ++i) {
        if (std::hypot(pos[i].x() - pos[kept - 1].x(),
                       pos[i].y() - pos[kept - 1].y()) < 1e-4f) {
          half[kept - 1] = std::max(half[kept - 1], half[i]);
          continue;
        }
        pos[kept] = pos[i];
        half[kept] = half[i];
        ++kept;
      }
      pos.resize(kept);
      half.resize(kept);
    }
    if (pos.size() < 2) continue;

    // The normal of each step, left of travel.
    std::vector<SkVector> normal(pos.size() - 1);
    std::vector<SkVector> along(pos.size() - 1);
    for (size_t i = 0; i + 1 < pos.size(); ++i) {
      const SkVector e{pos[i + 1].x() - pos[i].x(),
                       pos[i + 1].y() - pos[i].y()};
      const float L = std::hypot(e.x(), e.y());
      along[i] = {e.x() / L, e.y() / L};
      normal[i] = {-e.y() / L, e.x() / L};
    }
    const auto offset = [&](size_t i, SkVector n, float reach, float side) {
      return SkPoint{pos[i].x() + n.x() * reach * side,
                     pos[i].y() + n.y() * reach * side};
    };

    // THE BAND IS LAID IN RUNS. Where the spine bends gently, one strip —
    // the left rail forward, the right rail back — covers what that run's
    // quadrilaterals and the joins between them would, to within a
    // twentieth of a pixel: a station's rails stand on the bisector of its
    // two steps, at the reach that keeps each rail parallel to its step.
    // A station that bends harder than that, or whose inner rail would
    // fold back on itself, ends the run; there the two runs meet on their
    // own steps' cross-sections and the join closes the outside, as a pen
    // closes a corner. A strip is wound as one quadrilateral of it is, so
    // the winding fill unions runs, joins and overlaps alike.
    constexpr float kRunTolerancePx = 0.05f;
    std::vector<SkPoint> left, right;
    const auto flushRun = [&] {
      if (left.size() < 2) {
        left.clear();
        right.clear();
        return;
      }
      std::vector<SkPoint> ring(left);
      ring.insert(ring.end(), right.rbegin(), right.rend());
      // The orientation one quadrilateral of the strip has, which every
      // piece of the band shares.
      const std::array<glm::vec2, 4> first{fromSk(left[0]), fromSk(left[1]),
                                           fromSk(right[1]), fromSk(right[0])};
      band.moveTo(ring[0]);
      if (signedArea(first) > 0)
        for (size_t k = ring.size(); k-- > 1;) band.lineTo(ring[k]);
      else
        for (size_t k = 1; k < ring.size(); ++k) band.lineTo(ring[k]);
      band.close();
      left.clear();
      right.clear();
    };
    const auto startRun = [&](size_t i) {
      left.push_back(offset(i, normal[i], half[i], 1));
      right.push_back(offset(i, normal[i], half[i], -1));
    };
    // The joins, at a station two runs meet on. Both sides are emitted and
    // the winding fill absorbs the one on the inside of the turn, which is
    // cheaper and steadier than deciding which side is outside from a
    // cross product that vanishes on a straight run.
    const auto emitJoin = [&](size_t i) {
      const SkVector& n0 = normal[i - 1];
      const SkVector& n1 = normal[i];
      const float turn = n0.x() * n1.y() - n0.y() * n1.x();
      const float h = half[i];
      // Below about a tenth of a pixel of opening there is nothing to
      // close.
      if (h <= 0 || std::abs(turn) * h < 0.1f) return;
      if (sweep.join == SweepJoin::Round) {
        band.addCircle(pos[i].x(), pos[i].y(), h, SkPathDirection::kCCW);
        return;
      }
      for (int side = -1; side <= 1; side += 2) {
        const float s = (float)side;
        const SkPoint a = offset(i, n0, h, s);
        const SkPoint b = offset(i, n1, h, s);
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
    };
    // Whether a step's rails, from @p from to @p to, still run forward.
    const auto forward = [&](size_t step, SkPoint from, SkPoint to) {
      return (to.x() - from.x()) * along[step].x() +
                 (to.y() - from.y()) * along[step].y() >
             0;
    };
    startRun(0);
    for (size_t i = 1; i + 1 < pos.size(); ++i) {
      const SkVector& n0 = normal[i - 1];
      const SkVector& n1 = normal[i];
      const float angle = std::atan2(n0.x() * n1.y() - n0.y() * n1.x(),
                                     n0.x() * n1.x() + n0.y() * n1.y());
      const float h = half[i];
      bool gentle =
          h * angle * angle / 8 <= kRunTolerancePx && std::abs(angle) < 0.5f;
      SkPoint l{}, r{};
      if (gentle) {
        SkVector m{n0.x() + n1.x(), n0.y() + n1.y()};
        const float mag = std::hypot(m.x(), m.y());
        m = {m.x() / mag, m.y() / mag};
        const float reach = h / (m.x() * n1.x() + m.y() * n1.y());
        l = offset(i, m, reach, 1);
        r = offset(i, m, reach, -1);
        const SkPoint nextLeft = offset(i + 1, n1, half[i + 1], 1);
        const SkPoint nextRight = offset(i + 1, n1, half[i + 1], -1);
        gentle = forward(i - 1, left.back(), l) &&
                 forward(i - 1, right.back(), r) && forward(i, l, nextLeft) &&
                 forward(i, r, nextRight);
      }
      if (gentle) {
        left.push_back(l);
        right.push_back(r);
        continue;
      }
      left.push_back(offset(i, n0, h, 1));
      right.push_back(offset(i, n0, h, -1));
      flushRun();
      emitJoin(i);
      startRun(i);
    }
    const size_t last = pos.size() - 1;
    left.push_back(offset(last, normal[last - 1], half[last], 1));
    right.push_back(offset(last, normal[last - 1], half[last], -1));
    flushRun();
  }
  SkPath path = band.detach();
  path.setFillType(SkPathFillType::kWinding);
  return path;
}

SkPath profileOffset(const SkPath& spine, const Profile& profile) {
  if (spine.isEmpty()) return SkPath();
  const float total = totalLength(spine);
  if (total <= 0) return SkPath();
  // A CONSTANT profile IS a parallel, and `parallel` answers a parallel
  // in its own terms — for a law that never changes, every number the
  // walk below computes is a number `parallel` already computes, so
  // delegating is the way the two stay the same rail to the bit. No sign
  // conversion is needed: `parallel` is LEFT of travel, which is this
  // file's frame exactly.
  //
  // Constancy is detected by SAMPLING, and that is a real limitation, not
  // a rounding detail: a stepped profile whose period divides the sample
  // spacing reads as constant. Sampled at 97 points (prime, so no profile
  // whose period is a simple fraction aligns with it) offset by half a
  // step (so a value read exactly at 0, 1/2, 1 cannot be the whole basis).
  // A profile that defeats this is offset by its own first sample, which
  // is a rail of the wrong width rather than a broken one.
  {
    const float first = profile.acrossAt(0.5f / 97.0f, total);
    bool constant = true;
    for (int k = 1; k < 97 && constant; ++k)
      constant = profile.acrossAt(((float)k + 0.5f) / 97.0f, total) == first;
    if (constant) return first == 0.0f ? spine : parallel(spine, first);
  }
  SkPathBuilder out(spine.getFillType());
  float consumed = 0;
  for (const Contour& contour : contoursOf(spine)) {
    const float len = contour.length();
    const float base = consumed;
    consumed += len;
    const int steps = std::max(8, (int)std::ceil(len / 2.0f));
    const float stride = len / (float)steps;
    // The band's frame: positive across is LEFT of travel, which with y
    // down is outside a clockwise path. One body for the band's rails
    // and a relative strand, so the two cannot drift apart.
    //
    // ONE non-finite sample would delete the WHOLE band: Skia draws none
    // of a path that contains a non-finite vertex, and says nothing. An
    // author profile only has to misbehave at a single parameter value to
    // hit this — sqrt(sin(pi*along)) rounds to a tiny negative at
    // along == 1 — so a non-finite width is clamped to a LOCAL pinch down
    // to the spine rather than allowed to erase everything.
    const auto acrossAt = [&](float distance) {
      const float width =
          profile.acrossAt(total > 0 ? (base + distance) / total : 0.0f, total);
      return std::isfinite(width) ? width : 0.0f;
    };
    // THE REAL VERTICES, JOINED. Displacing each sample along its own
    // single tangent normal overshoots every corner the contour turns
    // toward the offset side: the two offset edges meet BEFORE the
    // vertex's perpendicular foot, so the samples nearest the corner sit
    // past the meeting and the walk doubles back into a small loop. A
    // cornerless contour has no joins and is walked exactly as it always
    // was.
    const std::vector<OffsetJoin> joins =
        offsetJoins(contour, acrossAt, stride);
    size_t next = 0;
    bool started = false;
    for (int k = 0; k <= steps; ++k) {
      const float d = len * (float)k / (float)steps;
      while (next < joins.size() && joins[next].distance <= d)
        appendOffsetJoin(out, joins[next++], started);
      const auto sample = contour.at(d);
      if (!sample) continue;
      if (joinAlreadyWrote(std::span(joins).first(next), d)) continue;
      if (swallowedByJoin(joins, contour, d)) continue;
      const float width = acrossAt(d);
      appendOffsetPoint(out,
                        {sample->position.x + sample->tangent.y * width,
                         sample->position.y - sample->tangent.x * width},
                        started);
    }
    while (next < joins.size()) appendOffsetJoin(out, joins[next++], started);
    if (started && contour.closed()) out.close();
  }
  return out.detach();
}

Outline profileOffset(const Outline& spine, const Profile& profile) {
  return fromSk(profileOffset(toSk(spine), profile));
}

Outline bandRegion(const Outline& spine, const Profile& width,
                   Formation formation) {
  return fromSk(bandRegion(toSk(spine), width, formation));
}

Outline sweptRegion(const Outline& spine, const SweepWidth& width,
                    const Sweep& sweep) {
  return fromSk(sweptRegion(toSk(spine), width, sweep));
}

}  // namespace sigil::geometry::path
