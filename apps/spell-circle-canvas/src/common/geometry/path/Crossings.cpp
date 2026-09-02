/** @file
 * Crossing discovery and the patch a knot is repaired over: the paths are
 * flattened and every pair of segments is tested for a PROPER crossing —
 * coincident paths and endpoint touches are meetings rather than
 * crossings, and reporting them would put a knot at every corner of every
 * rectangle.
 */

#include "sigilgeometry/path/Crossings.h"

#include <include/core/SkContourMeasure.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkRect.h>
#include <include/pathops/SkPathOps.h>

#include <algorithm>
#include <cmath>

namespace sigil::geometry::path {

namespace {

struct Flat {
  std::vector<SkPoint> points;
  std::vector<float> at;  // cumulative arc length at each point
  float length = 0;
  SkRect bounds = SkRect::MakeEmpty();  // of `points` — the pair rejection
};

Flat flatten(const SkPath& path) {
  Flat f;
  SkContourMeasureIter iter(path, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    if (len <= 0) continue;
    const int steps = std::max(2, (int)std::ceil(len / 2.0f));
    for (int k = 0; k <= steps; ++k) {
      const float d = len * (float)k / (float)steps;
      SkPoint pos;
      if (!contour->getPosTan(d, &pos, nullptr)) continue;
      f.points.push_back(pos);
      f.at.push_back(f.length + d);
    }
    f.length += len;
    // A break between contours: repeat the last point so the segment loop
    // below can skip the join (a chord between two contours is not a
    // strand and must not manufacture crossings). Guarded because a
    // contour whose every getPosTan failed appends nothing at all.
    if (!f.points.empty()) {
      f.points.push_back(f.points.back());
      f.at.push_back(f.length);
    }
  }
  if (!f.points.empty()) f.bounds.setBounds({f.points.data(), f.points.size()});
  return f;
}

/** The point on a flattened strand at arc length `s`. */
SkPoint pointAtArc(const Flat& f, float s) {
  if (f.points.empty()) return {0, 0};
  s = std::clamp(s, 0.0f, f.length);
  for (size_t k = 0; k + 1 < f.at.size(); ++k) {
    if (s > f.at[k + 1]) continue;
    const float span = f.at[k + 1] - f.at[k];
    const float w = span > 1e-6f ? (s - f.at[k]) / span : 0.0f;
    return {f.points[k].fX + (f.points[k + 1].fX - f.points[k].fX) * w,
            f.points[k].fY + (f.points[k + 1].fY - f.points[k].fY) * w};
  }
  return f.points.back();
}

/** Does one strand change sides of the other's local direction at `hit`? */
bool changesSides(const Flat& other, float sOther, SkPoint hit, SkVector dir) {
  const float delta = 3.0f;
  const SkPoint before = pointAtArc(other, sOther - delta);
  const SkPoint after = pointAtArc(other, sOther + delta);
  const auto side = [&](SkPoint q) {
    return dir.x() * (q.fY - hit.fY) - dir.y() * (q.fX - hit.fX);
  };
  return side(before) * side(after) < 0.0f;
}

/** Do these two strands genuinely CROSS at `hit`, or only meet there?
 *
 *  BOTH directions are tested, and that is the point: asking only "does B
 *  change sides of A" is order-asymmetric, so an A endpoint landing on B's
 *  interior answered yes while the mirror case answered no — the same
 *  meeting classified two ways depending on which strand happened to be
 *  indexed first. A crossing is a symmetric property and is tested as one.
 *
 *  This is also what keeps a rectangle's corners from each becoming a knot:
 *  at a shared vertex the neighbours sit on one side (or collinear), so at
 *  least one of the two tests fails. */
bool crossesTransversally(const Flat& fa, float sA, const Flat& fb, float sB,
                          SkPoint hit, SkVector aDir, SkVector bDir) {
  return changesSides(fb, sB, hit, aDir) && changesSides(fa, sA, hit, bDir);
}

}  // namespace

std::vector<Crossing> discoverCrossings(const std::vector<SkPath>& strands) {
  std::vector<Crossing> found;
  if (strands.size() < 2) return found;
  std::vector<Flat> flats;
  flats.reserve(strands.size());
  for (const SkPath& p : strands) flats.push_back(flatten(p));

  for (size_t a = 0; a < strands.size(); ++a)
    for (size_t b = a + 1; b < strands.size(); ++b) {
      // COINCIDENT strands never cross. This is the layers() case, and
      // testing it by path identity is exact where it matters most.
      if (strands[a] == strands[b]) continue;
      const Flat& fa = flats[a];
      const Flat& fb = flats[b];
      // Bounds rejection before the segment-by-segment loop, which is
      // quadratic in the flattened point counts. A reported crossing's hit
      // point lies on a segment of EACH strand, up to the parametric eps
      // overshoot below — a fraction of the flattening step, so far under a
      // pixel — which means two strands whose bounds stay half a pixel
      // apart provably cannot cross. The 0.5 px outset is orders of
      // magnitude larger than that overshoot, so this skips only provably
      // empty work and cannot change an answer.
      SkRect nearA = fa.bounds, nearB = fb.bounds;
      nearA.outset(0.5f, 0.5f);
      nearB.outset(0.5f, 0.5f);
      if (!SkRect::Intersects(nearA, nearB)) continue;
      for (size_t i = 0; i + 1 < fa.points.size(); ++i) {
        const SkPoint p0 = fa.points[i], p1 = fa.points[i + 1];
        const SkVector r{p1.fX - p0.fX, p1.fY - p0.fY};
        if (r.length() <= 1e-6f) continue;  // the contour join
        for (size_t j = 0; j + 1 < fb.points.size(); ++j) {
          const SkPoint q0 = fb.points[j], q1 = fb.points[j + 1];
          const SkVector sv{q1.fX - q0.fX, q1.fY - q0.fY};
          if (sv.length() <= 1e-6f) continue;
          const float denom = r.x() * sv.y() - r.y() * sv.x();
          // Parallel or collinear: no transversal crossing. Two copies of
          // one path land here for every corresponding segment.
          if (std::abs(denom) < 1e-9f) continue;
          const SkVector d{q0.fX - p0.fX, q0.fY - p0.fY};
          const float t = (d.x() * sv.y() - d.y() * sv.x()) / denom;
          const float u = (d.x() * r.y() - d.y() * r.x()) / denom;
          // CLOSED intervals, then a transversality test.
          //
          // Strict interiors cannot be used here. Symmetric geometry — two
          // diagonals of a square, a horizontal met by verticals on a
          // regular sampling grid — puts a genuine crossing EXACTLY on a
          // sample boundary, and a strict test discards all of them. So the
          // endpoints are accepted, and the question that actually
          // separates the two cases is asked afterwards: does the other
          // strand pass THROUGH here, or does it merely touch?
          const float eps = 1e-3f;
          if (t < -eps || t > 1.0f + eps || u < -eps || u > 1.0f + eps)
            continue;
          const SkPoint hit{p0.fX + r.x() * t, p0.fY + r.y() * t};
          const float sA = fa.at[i] + (fa.at[i + 1] - fa.at[i]) * t;
          const float sB = fb.at[j] + (fb.at[j + 1] - fb.at[j]) * u;
          if (!crossesTransversally(fa, sA, fb, sB, hit, r, sv)) continue;
          Crossing x;
          x.a = a;
          x.b = b;
          x.at = hit;
          x.alongA = fa.length > 0 ? sA / fa.length : 0.0f;
          x.alongB = fb.length > 0 ? sB / fb.length : 0.0f;
          // Sampling can report one meeting from two adjacent segment
          // pairs; keep the first and drop its neighbours.
          bool duplicate = false;
          for (const Crossing& seen : found)
            if (seen.a == x.a && seen.b == x.b &&
                std::abs(seen.at.fX - x.at.fX) < 1.5f &&
                std::abs(seen.at.fY - x.at.fY) < 1.5f) {
              duplicate = true;
              break;
            }
          if (!duplicate) found.push_back(x);
        }
      }
    }

  // Numbered ALONG THE BOUNDARY: ascending by position on the lower-indexed
  // strand, then by strand pair, so the order is deterministic and a
  // positional pin means the same knot on every frame the geometry holds.
  std::sort(found.begin(), found.end(),
            [](const Crossing& l, const Crossing& r) {
              if (l.alongA != r.alongA) return l.alongA < r.alongA;
              if (l.a != r.a) return l.a < r.a;
              return l.b < r.b;
            });
  for (size_t i = 0; i < found.size(); ++i) found[i].index = i;
  return found;
}

SkPath crossingPatch(const SkPath& a, float reachA, const SkPath& b,
                     float reachB, SkPoint at, float maxRadius) {
  const auto tube = [](const SkPath& path, float reach) {
    SkPaint p;
    p.setStyle(SkPaint::kStroke_Style);
    // `reach` is the mark's FULL width, and the tube is twice it. That is
    // deliberately conservative: alignment can put the whole mark on ONE
    // side of the path (Align::Inner/Outer), so a tube of exactly the mark
    // width, centred on the path, would miss half of it. The cost is a
    // lens up to 2x larger than the true overlap — harmless with opaque
    // inks, and bounded by maxRadius either way.
    p.setStrokeWidth(std::max(reach, 0.5f) * 2.0f);
    p.setStrokeCap(SkPaint::kRound_Cap);
    p.setStrokeJoin(SkPaint::kRound_Join);
    return skpathutils::FillPathWithPaint(path, p);
  };
  // The knot's OWN territory. Without this the neighbouring lenses of an
  // ordinary braid touch, pathops merges them into one contour, and the
  // first crossing's patch claims the entire run.
  SkPathBuilder territoryBuilder;
  territoryBuilder.addCircle(at.fX, at.fY, std::max(maxRadius, 1.0f));
  const SkPath territory = territoryBuilder.detach();

  SkPath overlap, lens;
  if (Op(tube(a, reachA), tube(b, reachB), kIntersect_SkPathOp, &overlap) &&
      !overlap.isEmpty() &&
      Op(overlap, territory, kIntersect_SkPathOp, &lens) && !lens.isEmpty()) {
    // The intersection holds EVERY overlap of the two strands, which is one
    // component per crossing. Keep the component this crossing is in, so a
    // strand pair that meets several times repairs each meeting on its own
    // terms rather than repainting all of them at the first.
    SkPathBuilder mine;
    bool found = false;
    SkPath::Iter iter(lens, false);
    SkPathBuilder run;
    bool runOpen = false;
    const auto flushRun = [&] {
      if (!runOpen) return;
      SkPath contour = run.detach();
      SkRect bounds = contour.getBounds();
      bounds.outset(0.5f, 0.5f);
      if (bounds.contains(at.fX, at.fY)) {
        mine.addPath(contour);
        found = true;
      }
      runOpen = false;
    };
    SkPoint pts[4];
    for (SkPath::Verb verb = iter.next(pts); verb != SkPath::kDone_Verb;
         verb = iter.next(pts)) {
      switch (verb) {
        case SkPath::kMove_Verb:
          flushRun();
          run.moveTo(pts[0]);
          runOpen = true;
          break;
        case SkPath::kLine_Verb:
          run.lineTo(pts[1]);
          break;
        case SkPath::kQuad_Verb:
          run.quadTo(pts[1], pts[2]);
          break;
        case SkPath::kConic_Verb:
          run.conicTo(pts[1], pts[2], iter.conicWeight());
          break;
        case SkPath::kCubic_Verb:
          run.cubicTo(pts[1], pts[2], pts[3]);
          break;
        case SkPath::kClose_Verb:
          run.close();
          break;
        default:
          break;
      }
    }
    flushRun();
    if (found) return mine.detach();
    return lens;  // the point missed every component's box — repair it all
  }
  // Degenerate or non-overlapping: a disc sized for the perpendicular case
  // is the best available answer and is what the exact form replaced. Still
  // bounded by the knot's own territory.
  SkPathBuilder disc;
  disc.addCircle(at.fX, at.fY,
                 std::min(std::max({reachA, reachB, 3.0f}) + 1.0f,
                          std::max(maxRadius, 1.0f)));
  return disc.detach();
}

}  // namespace sigil::geometry::path
