/** @file
 * THE CORNER TREATMENTS: every corner of a path rounded, the corners a
 * selection names rounded while the rest stand, and every corner of a
 * polyline contour chamfered.
 *
 * A corner is where two pieces meet, so each of these works over the
 * path's own nodes rather than over a resampling of it: a treated corner
 * has to land where the untreated one stood, and a corner the selection
 * passed over has to come back unchanged, curve handles and all.
 */

#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathEffect.h>
#include <include/core/SkStrokeRec.h>
#include <include/effects/SkCornerPathEffect.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

#include "OperationsInternal.h"
#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Operations.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Segments.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::geometry::path::operations {

namespace {

void appendCurve(SkPathBuilder& out, const Segment& piece) {
  switch (piece.kind) {
    case SegmentKind::Line:
      out.lineTo(toSk(piece.points[1]));
      break;
    case SegmentKind::Quad:
      out.quadTo(toSk(piece.points[1]), toSk(piece.points[2]));
      break;
    case SegmentKind::Conic:
      out.conicTo(toSk(piece.points[1]), toSk(piece.points[2]), piece.weight);
      break;
    case SegmentKind::Cubic:
      out.cubicTo(toSk(piece.points[1]), toSk(piece.points[2]),
                  toSk(piece.points[3]));
      break;
  }
}

/** How far along each of its two legs one corner is cut, or nothing
 *  where the corner is not one this selection rounds. */
struct Cut {
  bool rounds = false;
  float step = 0;
};

/** ROUNDING THE CORNERS A SELECTION NAMES. A corner here is two straight
 *  legs meeting at a node: the arc is the quadratic through the two
 *  points a step along each leg with the node as its control, which is
 *  the curve a corner effect lays down. `visual` holds the arc's
 *  stand-off from the node constant rather than the step, taking a right
 *  angle as the reference — so an acute corner takes a smaller step and
 *  an obtuse one a larger, and every corner reads as the same weight. */
SkPath selectedCorners(const SkPath& path, float radius,
                       const CornerOptions& options) {
  const float reference = std::cos(kPi * 0.25f);
  SkPathBuilder out(path.getFillType());
  for (const SegmentContour& contour : segments(path)) {
    if (contour.segments.empty()) continue;
    // The closing line is a leg like any other, so a rectangle rounds
    // the corner its walk started at as well as the three it passes.
    std::vector<Segment> legs = contour.segments;
    const bool closure = contour.closed && legs.back().end() != contour.start();
    if (closure) {
      Segment closing;
      closing.kind = SegmentKind::Line;
      closing.points[0] = legs.back().end();
      closing.points[1] = contour.start();
      legs.push_back(closing);
    }
    const size_t pieces = legs.size();
    const bool ring = contour.closed;
    const std::vector<Polyline> flat = flatten(toPath({&contour, 1}));
    const float area = flat.empty() ? 0.0f : flat.front().signedArea();

    // One cut per node that starts a piece; only a closed contour can
    // round the node its walk starts at, since an open contour's first
    // node is an end rather than a corner.
    std::vector<Cut> cuts(pieces);
    for (size_t j = 0; j < pieces; ++j) {
      if (j == 0 && !ring) continue;
      const Segment& before = legs[(j + pieces - 1) % pieces];
      const Segment& after = legs[j];
      if (before.kind != SegmentKind::Line || after.kind != SegmentKind::Line)
        continue;
      const glm::vec2 in = arrivingAlong(before);
      const glm::vec2 leaving = leavingAlong(after);
      const float turnDeg =
          std::acos(std::clamp(glm::dot(in, leaving), -1.0f, 1.0f)) * kRadToDeg;
      if (!(turnDeg > options.minTurnDeg)) continue;
      const float cross = in.x * leaving.y - in.y * leaving.x;
      const bool outward = area == 0 || (cross > 0) == (area > 0);
      if (options.outwardOnly && !outward) continue;
      const float halfAngle = (180.0f - turnDeg) * 0.5f * kDegToRad;
      float step = options.visual ? radius * reference /
                                        std::max(std::cos(halfAngle), 1e-2f)
                                  : radius;
      step = std::min({step, glm::length(before.end() - before.start()) * 0.5f,
                       glm::length(after.end() - after.start()) * 0.5f});
      if (!(step > 0)) continue;
      cuts[j] = Cut{true, step};
    }

    const auto leaveOf = [&](size_t j) {
      return legs[j].start() + leavingAlong(legs[j]) * cuts[j].step;
    };
    const auto arriveOf = [&](size_t j) {
      const Segment& before = legs[(j + pieces - 1) % pieces];
      return before.end() - arrivingAlong(before) * cuts[j].step;
    };

    out.moveTo(toSk(cuts[0].rounds ? leaveOf(0) : contour.start()));
    for (size_t j = 0; j < pieces; ++j) {
      const size_t nextNode = (j + 1) % pieces;
      const bool cutAhead = (j + 1 < pieces || ring) && cuts[nextNode].rounds;
      const bool implied = closure && j + 1 == pieces && !cutAhead;
      if (legs[j].kind == SegmentKind::Line) {
        // The closing leg with no arc on either side is the closure
        // itself, and `close()` draws it.
        if (!implied)
          out.lineTo(toSk(cutAhead ? arriveOf(nextNode) : legs[j].end()));
      } else {
        appendCurve(out, legs[j]);
      }
      if (cutAhead)
        out.quadTo(toSk(legs[nextNode].start()), toSk(leaveOf(nextNode)));
    }
    if (contour.closed) out.close();
  }
  return out.detach();
}

}  // namespace

SkPath roundCorners(const SkPath& path, float radius,
                    const CornerOptions& options) {
  if (radius <= 0) return path;
  if (options == CornerOptions{}) {
    // A corner effect writes verbs alone and never sets a fill type, so
    // the destination has to carry the source's.
    SkPathBuilder dst(path.getFillType());
    SkStrokeRec rec(SkStrokeRec::kFill_InitStyle);
    if (sk_sp<SkPathEffect> fx = SkCornerPathEffect::Make(radius);
        fx && fx->filterPath(&dst, path, &rec))
      return dst.detach();
    return path;
  }
  return selectedCorners(path, radius, options);
}

SkPath chamferCorners(const SkPath& path, float cut) {
  if (cut <= 0 || path.isEmpty()) return path;
  SkPathBuilder out(path.getFillType());
  std::vector<SkPoint> run;  // current contour's polyline vertices
  SkPathBuilder verbatim;    // the same contour, copied exactly
  bool closed = false, anyCurve = false;

  // A vertex's cut points: entry on the incoming leg, exit on the
  // outgoing leg, each clamped to half its leg. False at a
  // straight-through or degenerate vertex (no corner to cut).
  const auto cutAt = [&](size_t i, SkPoint& entry, SkPoint& exit) {
    const size_t n = run.size();
    const SkPoint prev = run[(i + n - 1) % n], v = run[i],
                  next = run[(i + 1) % n];
    const SkVector in{v.x() - prev.x(), v.y() - prev.y()};
    const SkVector outV{next.x() - v.x(), next.y() - v.y()};
    const float lenIn = std::hypot(in.x(), in.y());
    const float lenOut = std::hypot(outV.x(), outV.y());
    if (lenIn < 1e-4f || lenOut < 1e-4f) return false;
    const float cross = in.x() * outV.y() - in.y() * outV.x();
    const float dot = in.x() * outV.x() + in.y() * outV.y();
    if (std::abs(cross) <= 1e-4f * lenIn * lenOut && dot > 0)
      return false;  // straight through — no corner
    const float cIn = std::min(cut, lenIn * 0.5f);
    const float cOut = std::min(cut, lenOut * 0.5f);
    entry = {v.x() - in.x() / lenIn * cIn, v.y() - in.y() / lenIn * cIn};
    exit = {v.x() + outV.x() / lenOut * cOut, v.y() + outV.y() / lenOut * cOut};
    return true;
  };

  const auto emitChamfered = [&] {
    if (run.empty()) return;
    if (closed && run.size() > 1 && run.front() == run.back())
      run.pop_back();  // the closing joint belongs to close()
    const size_t n = run.size();
    SkPoint entry, exit;
    if (n < 3) {  // nothing to cut — as collected
      out.moveTo(run.front());
      for (size_t i = 1; i < n; ++i) out.lineTo(run[i]);
      if (closed) out.close();
      return;
    }
    if (!closed) {
      out.moveTo(run.front());
      for (size_t i = 1; i + 1 < n; ++i) {
        if (cutAt(i, entry, exit)) {
          out.lineTo(entry);
          out.lineTo(exit);
        } else {
          out.lineTo(run[i]);
        }
      }
      out.lineTo(run.back());
    } else {  // every vertex is interior, the moveTo joint included
      bool started = false;
      const auto put = [&](SkPoint p) {
        if (started)
          out.lineTo(p);
        else {
          out.moveTo(p);
          started = true;
        }
      };
      for (size_t i = 0; i < n; ++i) {
        if (cutAt(i, entry, exit)) {
          put(entry);
          out.lineTo(exit);
          started = true;
        } else {
          put(run[i]);
        }
      }
      out.close();
    }
  };

  const auto flushContour = [&] {
    if (anyCurve)  // chamfer is a polyline treatment: curves pass through
      out.addPath(verbatim.detach());
    else
      emitChamfered();
    verbatim = SkPathBuilder();
    run.clear();
    closed = false;
    anyCurve = false;
  };

  SkPath::Iter iter(path, false);
  SkPoint pts[4];
  SkPath::Verb verb;
  while ((verb = iter.next(pts)) != SkPath::kDone_Verb) {
    switch (verb) {
      case SkPath::kMove_Verb:
        flushContour();
        run.push_back(pts[0]);
        verbatim.moveTo(pts[0]);
        break;
      case SkPath::kLine_Verb:
        run.push_back(pts[1]);
        verbatim.lineTo(pts[1]);
        break;
      case SkPath::kQuad_Verb:
        anyCurve = true;
        verbatim.quadTo(pts[1], pts[2]);
        break;
      case SkPath::kConic_Verb:
        anyCurve = true;
        verbatim.conicTo(pts[1], pts[2], iter.conicWeight());
        break;
      case SkPath::kCubic_Verb:
        anyCurve = true;
        verbatim.cubicTo(pts[1], pts[2], pts[3]);
        break;
      case SkPath::kClose_Verb:
        closed = true;
        verbatim.close();
        break;
      default:
        break;
    }
  }
  flushContour();
  return out.detach();
}

}  // namespace sigil::geometry::path::operations
