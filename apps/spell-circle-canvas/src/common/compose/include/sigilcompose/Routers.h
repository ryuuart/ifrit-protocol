#pragma once

/** @file
 * SigilCompose connector routers — Router values for connector().
 *
 * A Router is a plain function of the two endpoint rects returning the
 * routed path, and a RailRouter the same over an ordered run of anchor
 * points. There is no enum of route kinds: these are the stock values, and
 * a caller's own function is a peer of them.
 *
 * The routed path arrives as the connector's `PaintContext::outline`, so
 * any PathFormat or ContourWalk foreground dresses it.
 */

#include "sigilcompose/Compose.h"

#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathEffect.h>
#include <include/core/SkStrokeRec.h>
#include <include/effects/SkCornerPathEffect.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace sigil::compose::routers {

/** Where an orthogonal leg takes its turn. `MidX` is the Z every node
 *  graph editor defaults to (half way over, one vertical run, half way
 *  in); `HFirst` and `VFirst` are the two Ls — bend AT the target column
 *  (horizontal out of the source first) or AT the source column (vertical
 *  first). A circuit trace bends at the target column; a flowchart drops
 *  out of the source first. */
enum class Bend { MidX, HFirst, VFirst };

/** CUT EVERY LINE-LINE CORNER of @p path with a straight bevel @p cut px
 *  along each leg — on the orthogonal family's right angles that is the
 *  45° face of the game-UI and PCB corner convention, which
 *  `SkCornerPathEffect` cannot spell because it only rounds. The cut
 *  clamps to half of each adjacent leg, so short legs degenerate to a
 *  diagonal rather than crossing over. Straight-through vertices are left
 *  alone; closed polyline contours chamfer the closing vertex too, so a
 *  routed loop and a `shapes::chamfered` panel agree.
 *
 *  THIS IS A POLYLINE TREATMENT. A contour containing ANY curve segment —
 *  quad, conic or cubic — is copied through completely untouched, so a
 *  chamfer over `arc()`, `octilinear()`'s rounded output, or anything
 *  already run through a corner effect is a silent no-op on that
 *  contour. */
inline SkPath chamfer(const SkPath &path, float cut) {
  if (cut <= 0 || path.isEmpty())
    return path;
  SkPathBuilder out;
  std::vector<SkPoint> run; // current contour's polyline vertices
  SkPathBuilder verbatim;   // the same contour, copied exactly
  bool closed = false, anyCurve = false;

  // A vertex's cut points: entry on the incoming leg, exit on the
  // outgoing leg, each clamped to half its leg. False at a
  // straight-through or degenerate vertex (no corner to cut).
  const auto cutAt = [&](size_t i, SkPoint &entry, SkPoint &exit) {
    const size_t n = run.size();
    const SkPoint prev = run[(i + n - 1) % n], v = run[i],
                  next = run[(i + 1) % n];
    const SkVector in{v.x() - prev.x(), v.y() - prev.y()};
    const SkVector outV{next.x() - v.x(), next.y() - v.y()};
    const float lenIn = std::hypot(in.x(), in.y());
    const float lenOut = std::hypot(outV.x(), outV.y());
    if (lenIn < 1e-4f || lenOut < 1e-4f)
      return false;
    const float cross = in.x() * outV.y() - in.y() * outV.x();
    const float dot = in.x() * outV.x() + in.y() * outV.y();
    if (std::abs(cross) <= 1e-4f * lenIn * lenOut && dot > 0)
      return false; // straight through — no corner
    const float cIn = std::min(cut, lenIn * 0.5f);
    const float cOut = std::min(cut, lenOut * 0.5f);
    entry = {v.x() - in.x() / lenIn * cIn, v.y() - in.y() / lenIn * cIn};
    exit = {v.x() + outV.x() / lenOut * cOut,
            v.y() + outV.y() / lenOut * cOut};
    return true;
  };

  const auto emitChamfered = [&] {
    if (run.empty())
      return;
    if (closed && run.size() > 1 && run.front() == run.back())
      run.pop_back(); // the closing joint belongs to close()
    const size_t n = run.size();
    SkPoint entry, exit;
    if (n < 3) { // nothing to cut — as collected
      out.moveTo(run.front());
      for (size_t i = 1; i < n; ++i)
        out.lineTo(run[i]);
      if (closed)
        out.close();
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
    } else { // every vertex is interior, the moveTo joint included
      bool started = false;
      // NOT named `emit`: this header reaches Qt TUs, where that is a macro.
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
    if (anyCurve) // chamfer is a polyline treatment: curves pass through
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

namespace detail {

/** Consecutive-duplicate and forward-collinear collapse over a waypoint
 *  run — why the manhattan family emits no zero-length or split segments
 *  on axis-aligned pairs. Reversals (spikes) are kept: a doubled-back leg
 *  is real geometry, not a redundant vertex. */
inline void collapseCollinear(std::vector<SkPoint> &pts) {
  size_t w = 0;
  for (size_t i = 0; i < pts.size(); ++i) {
    if (w > 0 && pts[i] == pts[w - 1])
      continue; // zero-length
    if (w >= 2) {
      const SkVector a{pts[w - 1].x() - pts[w - 2].x(),
                       pts[w - 1].y() - pts[w - 2].y()};
      const SkVector b{pts[i].x() - pts[w - 1].x(),
                       pts[i].y() - pts[w - 1].y()};
      const float cross = a.x() * b.y() - a.y() * b.x();
      const float dot = a.x() * b.x() + a.y() * b.y();
      if (cross == 0.0f && dot > 0.0f) {
        pts[w - 1] = pts[i]; // extend the straight run
        continue;
      }
    }
    pts[w++] = pts[i];
  }
  pts.resize(w);
}

/** The shared manhattan construction: waypoints per leg by bend policy,
 *  collapsed, then one corner treatment — `chamferCut` wins over
 *  `cornerRadius` when both are set (they are alternatives, not layers). */
inline SkPath manhattanPath(std::span<const SkPoint> anchors, Bend bend,
                            float cornerRadius, float chamferCut) {
  SkPathBuilder b;
  if (anchors.empty())
    return b.detach();
  std::vector<SkPoint> way;
  way.reserve(anchors.size() * 3);
  way.push_back(anchors.front());
  for (size_t i = 1; i < anchors.size(); ++i) {
    const SkPoint a = anchors[i - 1], c = anchors[i];
    switch (bend) {
    case Bend::MidX: {
      const float midX = (a.x() + c.x()) / 2;
      way.push_back({midX, a.y()});
      way.push_back({midX, c.y()});
      break;
    }
    case Bend::HFirst:
      way.push_back({c.x(), a.y()});
      break;
    case Bend::VFirst:
      way.push_back({a.x(), c.y()});
      break;
    }
    way.push_back(c);
  }
  collapseCollinear(way);
  b.moveTo(way.front());
  for (size_t i = 1; i < way.size(); ++i)
    b.lineTo(way[i]);
  SkPath path = b.detach();
  if (chamferCut > 0)
    return chamfer(path, chamferCut);
  if (cornerRadius <= 0)
    return path;
  SkPathBuilder rounded;
  SkStrokeRec rec(SkStrokeRec::kFill_InitStyle);
  if (sk_sp<SkPathEffect> fx = SkCornerPathEffect::Make(cornerRadius);
      fx && fx->filterPath(&rounded, path, &rec))
    return rounded.detach();
  return path;
}

} // namespace detail

/** Straight center-to-center line — the connector default, as a named
 *  value for symmetry. */
inline Router straight() {
  return [](const SkRect &from, const SkRect &to) {
    SkPathBuilder b;
    b.moveTo(from.centerX(), from.centerY());
    b.lineTo(to.centerX(), to.centerY());
    return b.detach();
  };
}

/** Orthogonal (Manhattan) route: horizontal out of the source, one
 *  vertical run at the midpoint, horizontal into the target. A
 *  positive @p cornerRadius rounds the two turns. */
inline Router orthogonal(float cornerRadius = 0.0f) {
  return [cornerRadius](const SkRect &from, const SkRect &to) {
    const float fx = from.centerX(), fy = from.centerY();
    const float tx = to.centerX(), ty = to.centerY();
    const float midX = (fx + tx) / 2;
    SkPathBuilder b;
    b.moveTo(fx, fy);
    b.lineTo(midX, fy);
    b.lineTo(midX, ty);
    b.lineTo(tx, ty);
    SkPath path = b.detach();
    if (cornerRadius <= 0)
      return path;
    SkPathBuilder roundedPath;
    SkStrokeRec rec(SkStrokeRec::kFill_InitStyle);
    if (sk_sp<SkPathEffect> fx2 = SkCornerPathEffect::Make(cornerRadius);
        fx2 && fx2->filterPath(&roundedPath, path, &rec))
      return roundedPath.detach();
    return path;
  };
}

/** Orthogonal route with a bend policy: where the overload above always
 *  bends at midX (a Z), this one also spells the two Ls — see `Bend`.
 *  Collinear points collapse, so an axis-aligned pair emits ONE segment
 *  rather than three with zero-length ends, and the corner is either
 *  rounded (@p cornerRadius, SkCornerPathEffect) or cut at 45°
 *  (@p chamferCut — see `chamfer()` above). The two are alternatives:
 *  chamfer wins when both are set.
 *
 *  The zero-argument `orthogonal()` is NOT this function with defaults. It
 *  emits its degenerate verbs verbatim, and that output is frozen because
 *  existing routes depend on it byte for byte; this is the spelling to
 *  reach for in new code. */
inline Router orthogonal(Bend bend, float cornerRadius = 0.0f,
                         float chamferCut = 0.0f) {
  return [bend, cornerRadius, chamferCut](const SkRect &from,
                                          const SkRect &to) {
    const SkPoint ends[2] = {{from.centerX(), from.centerY()},
                             {to.centerX(), to.centerY()}};
    return detail::manhattanPath(ends, bend, cornerRadius, chamferCut);
  };
}

// ---------------------------------------------------------------------------
// Rail routers (rail(): an ordered run of anchor points → the line's path)

/** The RAIL spelling of the orthogonal family: `rail(stops,
 *  routers::manhattan())`. `orthogonal()` cannot be used here — it is a
 *  pairwise Router and `rail()` takes a RailRouter over the whole anchor
 *  run.
 *
 *  Each consecutive anchor pair runs H/V legs per @p bend; collinear
 *  points collapse, so axis-aligned anchors thread as single clean
 *  segments; corners round with @p cornerRadius or cut at 45° with
 *  @p chamferCut, and chamfer wins when both are set. */
inline RailRouter manhattan(Bend bend = Bend::MidX, float cornerRadius = 0.0f,
                            float chamferCut = 0.0f) {
  return [bend, cornerRadius, chamferCut](std::span<const SkPoint> pts) {
    return detail::manhattanPath(pts, bend, cornerRadius, chamferCut);
  };
}

/** Adapts any pairwise Router into a RailRouter: consecutive anchors are
 *  routed pairwise (each anchor as a point rect, so center-to-center
 *  routers see the anchor itself) and the legs stitch into ONE contour —
 *  terminal caps and casings fire once at the run's ends, not at every
 *  waypoint. Junction moves are dropped, zero-length segments collapse
 *  and exactly-collinear line runs merge; curve legs (arc()) ride
 *  through untouched. */
inline RailRouter fromPairwise(Router router) {
  return [router = std::move(router)](std::span<const SkPoint> pts) {
    SkPathBuilder b;
    if (pts.size() < 2 || !router) {
      if (!pts.empty())
        b.moveTo(pts.front());
      for (size_t i = 1; i < pts.size(); ++i)
        b.lineTo(pts[i]);
      return b.detach();
    }
    // Collect into an op list so a collinear lineTo can extend the
    // previous one (SkPathBuilder cannot rewrite its tail).
    struct Op {
      SkPath::Verb verb;
      SkPoint p[3];
      float w = 0; // conic weight
    };
    std::vector<Op> ops;
    SkPoint cur = pts.front();
    ops.push_back({SkPath::kMove_Verb, {cur}});
    const auto pushLine = [&](SkPoint p) {
      if (p == cur)
        return; // zero-length — collapse
      if (ops.back().verb == SkPath::kLine_Verb) {
        // Merge an exactly-forward-collinear run: the last segment's own
        // start is the endpoint of the op before it, whatever its verb.
        const Op &before = ops[ops.size() - 2];
        SkPoint segStart = before.p[0];
        if (before.verb == SkPath::kQuad_Verb ||
            before.verb == SkPath::kConic_Verb)
          segStart = before.p[1];
        else if (before.verb == SkPath::kCubic_Verb)
          segStart = before.p[2];
        const SkVector a{cur.x() - segStart.x(), cur.y() - segStart.y()};
        const SkVector d{p.x() - cur.x(), p.y() - cur.y()};
        const float cross = a.x() * d.y() - a.y() * d.x();
        const float dot = a.x() * d.x() + a.y() * d.y();
        if (cross == 0.0f && dot > 0.0f) {
          ops.back().p[0] = p;
          cur = p;
          return;
        }
      }
      ops.push_back({SkPath::kLine_Verb, {p}});
      cur = p;
    };
    for (size_t i = 1; i < pts.size(); ++i) {
      const SkPath leg =
          router(SkRect::MakeXYWH(pts[i - 1].x(), pts[i - 1].y(), 0, 0),
                 SkRect::MakeXYWH(pts[i].x(), pts[i].y(), 0, 0));
      SkPath::Iter iter(leg, false);
      SkPoint lp[4];
      SkPath::Verb verb;
      while ((verb = iter.next(lp)) != SkPath::kDone_Verb) {
        switch (verb) {
        case SkPath::kMove_Verb:
          // The stitch: a leg starts where the last one ended; a router
          // that starts elsewhere gets a bridging line instead of a gap.
          pushLine(lp[0]);
          break;
        case SkPath::kLine_Verb:
          pushLine(lp[1]);
          break;
        case SkPath::kQuad_Verb:
          if (!(lp[1] == cur && lp[2] == cur)) {
            ops.push_back({SkPath::kQuad_Verb, {lp[1], lp[2]}});
            cur = lp[2];
          }
          break;
        case SkPath::kConic_Verb:
          if (!(lp[1] == cur && lp[2] == cur)) {
            ops.push_back(
                {SkPath::kConic_Verb, {lp[1], lp[2]}, iter.conicWeight()});
            cur = lp[2];
          }
          break;
        case SkPath::kCubic_Verb:
          if (!(lp[1] == cur && lp[2] == cur && lp[3] == cur)) {
            ops.push_back({SkPath::kCubic_Verb, {lp[1], lp[2], lp[3]}});
            cur = lp[3];
          }
          break;
        default: // routes are open; close verbs do not stitch
          break;
        }
      }
    }
    for (const Op &op : ops) {
      switch (op.verb) {
      case SkPath::kMove_Verb: b.moveTo(op.p[0]); break;
      case SkPath::kLine_Verb: b.lineTo(op.p[0]); break;
      case SkPath::kQuad_Verb: b.quadTo(op.p[0], op.p[1]); break;
      case SkPath::kConic_Verb: b.conicTo(op.p[0], op.p[1], op.w); break;
      case SkPath::kCubic_Verb: b.cubicTo(op.p[0], op.p[1], op.p[2]); break;
      default: break;
      }
    }
    return b.detach();
  };
}

/** Straight polyline through the waypoints; a positive @p cornerRadius
 *  rounds every turn (SkCornerPathEffect). */
inline RailRouter polyline(float cornerRadius = 0.0f) {
  return [cornerRadius](std::span<const SkPoint> pts) {
    SkPathBuilder b;
    if (pts.empty())
      return b.detach();
    b.moveTo(pts.front());
    for (size_t i = 1; i < pts.size(); ++i)
      b.lineTo(pts[i]);
    SkPath path = b.detach();
    if (cornerRadius <= 0)
      return path;
    SkPathBuilder roundedPath;
    SkStrokeRec rec(SkStrokeRec::kFill_InitStyle);
    if (sk_sp<SkPathEffect> fx = SkCornerPathEffect::Make(cornerRadius);
        fx && fx->filterPath(&roundedPath, path, &rec))
      return roundedPath.detach();
    return path;
  };
}

/** The metro-map router: each leg runs a 45° diagonal for the shorter
 *  delta, then finishes straight — every segment ends up horizontal,
 *  vertical, or diagonal (octilinearity, the schematic-map convention);
 *  @p cornerRadius rounds the turns. */
inline RailRouter octilinear(float cornerRadius = 8.0f) {
  return [cornerRadius](std::span<const SkPoint> pts) {
    SkPathBuilder b;
    if (pts.empty())
      return b.detach();
    b.moveTo(pts.front());
    for (size_t i = 1; i < pts.size(); ++i) {
      const SkPoint from = pts[i - 1], to = pts[i];
      const float dx = to.x() - from.x(), dy = to.y() - from.y();
      const float diag = std::min(std::abs(dx), std::abs(dy));
      if (diag > 0.5f && std::abs(std::abs(dx) - std::abs(dy)) > 0.5f) {
        // Diagonal leg first (45°), then the axis-aligned remainder.
        const SkPoint mid = {from.x() + std::copysign(diag, dx),
                             from.y() + std::copysign(diag, dy)};
        b.lineTo(mid);
      }
      b.lineTo(to);
    }
    SkPath path = b.detach();
    if (cornerRadius <= 0)
      return path;
    SkPathBuilder roundedPath;
    SkStrokeRec rec(SkStrokeRec::kFill_InitStyle);
    if (sk_sp<SkPathEffect> fx = SkCornerPathEffect::Make(cornerRadius);
        fx && fx->filterPath(&roundedPath, path, &rec))
      return roundedPath.detach();
    return path;
  };
}

/** The orbit router: when two consecutive anchors sit at (nearly) the same
 *  radius from `center`, the leg follows the CIRCLE between them — the
 *  short way around — instead of chording across. Radius-changing legs
 *  stay straight spokes. This is how a skill-tree or orbital diagram
 *  reads, where nodes live on concentric rings and their in-ring links are
 *  arcs rather than chords. `tolerance` is the radius-match slack as a
 *  fraction of the radius. */
inline RailRouter orbit(SkPoint center, float tolerance = 0.05f) {
  return [center, tolerance](std::span<const SkPoint> pts) {
    SkPathBuilder b;
    if (pts.empty())
      return b.detach();
    b.moveTo(pts.front());
    for (size_t i = 1; i < pts.size(); ++i) {
      const SkPoint from = pts[i - 1], to = pts[i];
      const float r1 = SkPoint::Distance(from, center);
      const float r2 = SkPoint::Distance(to, center);
      const float r = (r1 + r2) * 0.5f;
      if (r > 1.0f && std::abs(r1 - r2) <= tolerance * r) {
        const float a1 = std::atan2(from.y() - center.y(),
                                    from.x() - center.x()) *
                         57.29578f;
        const float a2 =
            std::atan2(to.y() - center.y(), to.x() - center.x()) * 57.29578f;
        float sweep = a2 - a1;
        while (sweep > 180.0f)
          sweep -= 360.0f;
        while (sweep <= -180.0f)
          sweep += 360.0f;
        b.arcTo(SkRect::MakeLTRB(center.x() - r, center.y() - r,
                                 center.x() + r, center.y() + r),
                a1, sweep, false);
      } else {
        b.lineTo(to);
      }
    }
    return b.detach();
  };
}

/** Circular-ish bow between the centers: the route's midpoint bulges
 *  off the chord by @p bulge × chord-length (sign picks the side). */
inline Router arc(float bulge = 0.25f) {
  return [bulge](const SkRect &from, const SkRect &to) {
    const SkPoint a{from.centerX(), from.centerY()};
    const SkPoint c{to.centerX(), to.centerY()};
    const SkVector chord{c.x() - a.x(), c.y() - a.y()};
    const float len = std::hypot(chord.x(), chord.y());
    SkPathBuilder b;
    b.moveTo(a);
    if (len < 1e-3f) {
      b.lineTo(c);
      return b.detach();
    }
    const SkVector normal{-chord.y() / len, chord.x() / len};
    // A quadratic passes halfway to its control point at t=0.5, so the
    // control sits at twice the requested bulge.
    const SkPoint control{(a.x() + c.x()) / 2 + normal.x() * 2 * bulge * len,
                          (a.y() + c.y()) / 2 + normal.y() * 2 * bulge * len};
    b.quadTo(control, c);
    return b.detach();
  };
}

} // namespace sigil::compose::routers
