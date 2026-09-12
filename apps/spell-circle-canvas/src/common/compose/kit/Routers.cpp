/** @file
 * The stock connector and rail routers, and the corner treatments a routed
 * path takes.
 */

#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathEffect.h>
#include <include/core/SkStrokeRec.h>
#include <include/effects/SkCornerPathEffect.h>
#include <sigilcompose/kit/Routers.h>
#include <sigilgeometry/path/Numeric.h>
#include <sigilgeometry/path/Operations.h>

#include <algorithm>
#include <cmath>
#include <span>
#include <utility>
#include <vector>

namespace sigil::compose::routers {

namespace {

/** Consecutive-duplicate and forward-collinear collapse over a waypoint
 *  run — why the manhattan family emits no zero-length or split segments
 *  on axis-aligned pairs. Reversals (spikes) are kept: a doubled-back leg
 *  is real geometry, not a redundant vertex. */
void collapseCollinear(std::vector<SkPoint>& pts) {
  size_t w = 0;
  for (size_t i = 0; i < pts.size(); ++i) {
    if (w > 0 && pts[i] == pts[w - 1]) continue;  // zero-length
    if (w >= 2) {
      const SkVector a{pts[w - 1].x() - pts[w - 2].x(),
                       pts[w - 1].y() - pts[w - 2].y()};
      const SkVector b{pts[i].x() - pts[w - 1].x(),
                       pts[i].y() - pts[w - 1].y()};
      const float cross = a.x() * b.y() - a.y() * b.x();
      const float dot = a.x() * b.x() + a.y() * b.y();
      if (cross == 0.0f && dot > 0.0f) {
        pts[w - 1] = pts[i];  // extend the straight run
        continue;
      }
    }
    pts[w++] = pts[i];
  }
  pts.resize(w);
}

/** One coordinate moved to the nearest whole number of @p advance from
 *  @p from — how a corner is put where a stamped leg's tile count ends.
 *  An advance of nothing moves nothing. */
float onAdvance(float from, float to, float advance) {
  if (!(advance > 0)) return to;
  const float delta = to - from;
  const float steps = std::round(std::abs(delta) / advance);
  if (steps <= 0) return from;
  return from + (delta > 0 ? 1.0f : -1.0f) * advance * steps;
}

/** Pull @p end back toward @p toward by @p inset px — the room a route
 *  gives up at each of its ends. Never past the far end of the leg, and
 *  a leg with no length keeps its point. */
void pullIn(SkPoint& end, const SkPoint& toward, float inset) {
  if (!(inset > 0)) return;
  const SkVector d{toward.x() - end.x(), toward.y() - end.y()};
  const float len = std::hypot(d.x(), d.y());
  if (len <= 0) return;
  const float take = std::min(inset, len);
  end = {end.x() + d.x() / len * take, end.y() + d.y() / len * take};
}

/** The shared manhattan construction: waypoints per leg by bend policy,
 *  collapsed, then the stamp's own two moves, then one corner treatment —
 *  `chamferCut` wins over `cornerRadius` when both are set (they are
 *  alternatives, not layers).
 *
 *  THE STAMP IS APPLIED BEFORE THE CORNER TREATMENT and after the
 *  collapse. Before, because a rounded or chamfered corner is a
 *  replacement for the vertex the count is measured from; after, because
 *  a collapsed run is the leg a stamp is laid along, and quantising the
 *  vertex a collapse is about to delete would move a straight route. */
SkPath manhattanPath(std::span<const SkPoint> anchors, Bend bend,
                     float cornerRadius, float chamferCut, Stamp stamp = {}) {
  SkPathBuilder b;
  if (anchors.empty()) return b.detach();
  std::vector<SkPoint> way;
  way.reserve(anchors.size() * 3);
  way.push_back(anchors.front());
  for (size_t i = 1; i < anchors.size(); ++i) {
    const SkPoint a = anchors[i - 1], c = anchors[i];
    switch (bend) {
      case Bend::MidX: {
        const float midX = onAdvance(a.x(), (a.x() + c.x()) / 2, stamp.advance);
        way.push_back({midX, a.y()});
        way.push_back({midX, c.y()});
        break;
      }
      case Bend::HFirst:
        way.push_back({onAdvance(a.x(), c.x(), stamp.advance), a.y()});
        break;
      case Bend::VFirst:
        way.push_back({a.x(), onAdvance(a.y(), c.y(), stamp.advance)});
        break;
    }
    way.push_back(c);
  }
  collapseCollinear(way);
  if (stamp.endInset > 0 && way.size() >= 2) {
    pullIn(way.front(), way[1], stamp.endInset);
    pullIn(way.back(), way[way.size() - 2], stamp.endInset);
  }
  if (way.size() < 2) return b.detach();
  b.moveTo(way.front());
  for (size_t i = 1; i < way.size(); ++i) b.lineTo(way[i]);
  SkPath path = b.detach();
  if (chamferCut > 0)
    return geometry::path::operations::chamferCorners(path, chamferCut);
  if (cornerRadius <= 0) return path;
  SkPathBuilder rounded;
  SkStrokeRec rec(SkStrokeRec::kFill_InitStyle);
  if (sk_sp<SkPathEffect> fx = SkCornerPathEffect::Make(cornerRadius);
      fx && fx->filterPath(&rounded, path, &rec))
    return rounded.detach();
  return path;
}

/** Every stock router is a SCHEME rather than a closure: a small value
 *  whose fields are the parameters it routes by, so two separately built
 *  routers of one kind compare equal and the node holding one prunes. A
 *  closure would be a fresh identity per describe and would re-patch and
 *  re-record a route that never moved. */

struct StraightRoute {
  bool operator==(const StraightRoute&) const = default;
  SkPath route(const SkRect& from, const SkRect& to) const {
    SkPathBuilder b;
    b.moveTo(from.centerX(), from.centerY());
    b.lineTo(to.centerX(), to.centerY());
    return b.detach();
  }
};

/** The frozen Z: its degenerate verbs are emitted verbatim, so it is a
 *  scheme of its own rather than BentRoute at Bend::MidX. */
struct OrthogonalRoute {
  float cornerRadius = 0.0f;
  bool operator==(const OrthogonalRoute&) const = default;
  SkPath route(const SkRect& from, const SkRect& to) const {
    const float fx = from.centerX(), fy = from.centerY();
    const float tx = to.centerX(), ty = to.centerY();
    const float midX = (fx + tx) / 2;
    SkPathBuilder b;
    b.moveTo(fx, fy);
    b.lineTo(midX, fy);
    b.lineTo(midX, ty);
    b.lineTo(tx, ty);
    SkPath path = b.detach();
    if (cornerRadius <= 0) return path;
    SkPathBuilder roundedPath;
    SkStrokeRec rec(SkStrokeRec::kFill_InitStyle);
    if (sk_sp<SkPathEffect> effect = SkCornerPathEffect::Make(cornerRadius);
        effect && effect->filterPath(&roundedPath, path, &rec))
      return roundedPath.detach();
    return path;
  }
};

struct BentRoute {
  Bend bend = Bend::MidX;
  float cornerRadius = 0.0f;
  float chamferCut = 0.0f;
  Stamp stamp;
  bool operator==(const BentRoute&) const = default;
  SkPath route(const SkRect& from, const SkRect& to) const {
    const SkPoint ends[2] = {{from.centerX(), from.centerY()},
                             {to.centerX(), to.centerY()}};
    return manhattanPath(ends, bend, cornerRadius, chamferCut, stamp);
  }
};

struct ArcRoute {
  float bulge = 0.25f;
  bool operator==(const ArcRoute&) const = default;
  SkPath route(const SkRect& from, const SkRect& to) const {
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
  }
};

struct ManhattanRail {
  Bend bend = Bend::MidX;
  float cornerRadius = 0.0f;
  float chamferCut = 0.0f;
  Stamp stamp;
  bool operator==(const ManhattanRail&) const = default;
  SkPath route(std::span<const SkPoint> pts) const {
    return manhattanPath(pts, bend, cornerRadius, chamferCut, stamp);
  }
};

/** The pairwise adaptor's identity is the Router it wraps, which is a
 *  comparable value in its own right — so an adapted stock route prunes
 *  and an adapted raw callable does not, exactly as it would unwrapped. */
struct PairwiseRail {
  Router router;
  bool operator==(const PairwiseRail&) const = default;
  SkPath route(std::span<const SkPoint> pts) const;
};

struct PolylineRail {
  float cornerRadius = 0.0f;
  bool operator==(const PolylineRail&) const = default;
  SkPath route(std::span<const SkPoint> pts) const {
    SkPathBuilder b;
    if (pts.empty()) return b.detach();
    b.moveTo(pts.front());
    for (size_t i = 1; i < pts.size(); ++i) b.lineTo(pts[i]);
    SkPath path = b.detach();
    if (cornerRadius <= 0) return path;
    SkPathBuilder roundedPath;
    SkStrokeRec rec(SkStrokeRec::kFill_InitStyle);
    if (sk_sp<SkPathEffect> effect = SkCornerPathEffect::Make(cornerRadius);
        effect && effect->filterPath(&roundedPath, path, &rec))
      return roundedPath.detach();
    return path;
  }
};

struct OctilinearRail {
  float cornerRadius = 8.0f;
  bool operator==(const OctilinearRail&) const = default;
  SkPath route(std::span<const SkPoint> pts) const {
    SkPathBuilder b;
    if (pts.empty()) return b.detach();
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
    if (cornerRadius <= 0) return path;
    SkPathBuilder roundedPath;
    SkStrokeRec rec(SkStrokeRec::kFill_InitStyle);
    if (sk_sp<SkPathEffect> effect = SkCornerPathEffect::Make(cornerRadius);
        effect && effect->filterPath(&roundedPath, path, &rec))
      return roundedPath.detach();
    return path;
  }
};

struct OrbitRail {
  SkPoint center = {0, 0};
  float tolerance = 0.05f;
  bool operator==(const OrbitRail&) const = default;
  SkPath route(std::span<const SkPoint> pts) const {
    SkPathBuilder b;
    if (pts.empty()) return b.detach();
    b.moveTo(pts.front());
    for (size_t i = 1; i < pts.size(); ++i) {
      const SkPoint from = pts[i - 1], to = pts[i];
      const float r1 = SkPoint::Distance(from, center);
      const float r2 = SkPoint::Distance(to, center);
      const float r = (r1 + r2) * 0.5f;
      if (r > 1.0f && std::abs(r1 - r2) <= tolerance * r) {
        const float a1 = geometry::path::degrees(
            std::atan2(from.y() - center.y(), from.x() - center.x()));
        const float a2 = geometry::path::degrees(
            std::atan2(to.y() - center.y(), to.x() - center.x()));
        float sweep = a2 - a1;
        while (sweep > 180.0f) sweep -= 360.0f;
        while (sweep <= -180.0f) sweep += 360.0f;
        b.arcTo(SkRect::MakeLTRB(center.x() - r, center.y() - r, center.x() + r,
                                 center.y() + r),
                a1, sweep, false);
      } else {
        b.lineTo(to);
      }
    }
    return b.detach();
  }
};

SkPath PairwiseRail::route(std::span<const SkPoint> pts) const {
  SkPathBuilder b;
  if (pts.size() < 2 || !router) {
    if (!pts.empty()) b.moveTo(pts.front());
    for (size_t i = 1; i < pts.size(); ++i) b.lineTo(pts[i]);
    return b.detach();
  }
  // Collect into an op list so a collinear lineTo can extend the
  // previous one (SkPathBuilder cannot rewrite its tail).
  struct Op {
    SkPath::Verb verb;
    SkPoint p[3];
    float w = 0;  // conic weight
  };
  std::vector<Op> ops;
  SkPoint cur = pts.front();
  ops.push_back({SkPath::kMove_Verb, {cur}});
  const auto pushLine = [&](SkPoint p) {
    if (p == cur) return;  // zero-length — collapse
    if (ops.back().verb == SkPath::kLine_Verb) {
      // Merge an exactly-forward-collinear run: the last segment's own
      // start is the endpoint of the op before it, whatever its verb.
      const Op& before = ops[ops.size() - 2];
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
        default:  // routes are open; close verbs do not stitch
          break;
      }
    }
  }
  for (const Op& op : ops) {
    switch (op.verb) {
      case SkPath::kMove_Verb:
        b.moveTo(op.p[0]);
        break;
      case SkPath::kLine_Verb:
        b.lineTo(op.p[0]);
        break;
      case SkPath::kQuad_Verb:
        b.quadTo(op.p[0], op.p[1]);
        break;
      case SkPath::kConic_Verb:
        b.conicTo(op.p[0], op.p[1], op.w);
        break;
      case SkPath::kCubic_Verb:
        b.cubicTo(op.p[0], op.p[1], op.p[2]);
        break;
      default:
        break;
    }
  }
  return b.detach();
}

}  // namespace

Router straight() { return StraightRoute{}; }

Router orthogonal(float cornerRadius) { return OrthogonalRoute{cornerRadius}; }

Router orthogonal(Bend bend, float cornerRadius, float chamferCut,
                  Stamp stamp) {
  return BentRoute{bend, cornerRadius, chamferCut, stamp};
}

RailRouter manhattan(Bend bend, float cornerRadius, float chamferCut,
                     Stamp stamp) {
  return ManhattanRail{bend, cornerRadius, chamferCut, stamp};
}

RailRouter fromPairwise(Router router) {
  return PairwiseRail{std::move(router)};
}

RailRouter polyline(float cornerRadius) { return PolylineRail{cornerRadius}; }

RailRouter octilinear(float cornerRadius) {
  return OctilinearRail{cornerRadius};
}

RailRouter orbit(SkPoint center, float tolerance) {
  return OrbitRail{center, tolerance};
}

Router arc(float bulge) { return ArcRoute{bulge}; }

}  // namespace sigil::compose::routers
