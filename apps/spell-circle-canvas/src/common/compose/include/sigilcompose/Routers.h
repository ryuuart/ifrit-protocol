#pragma once

/** @file
 * SigilCompose connector routers — Router values for connector().
 * Routers are plain functions of the two endpoint rects returning the
 * routed path (API.md: "routers are values/fns, not an enum — write
 * your own"); these are the stock ones. The routed path arrives as the
 * connector's PaintContext::outline, so any PathFormat/ContourWalk
 * foreground dresses it.
 */

#include "sigilcompose/Compose.h"

#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathEffect.h>
#include <include/core/SkStrokeRec.h>
#include <include/effects/SkCornerPathEffect.h>

#include <algorithm>
#include <cmath>

namespace sigil::compose::routers {

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

// ---------------------------------------------------------------------------
// Rail routers (rail(): an ordered run of anchor points → the line's path)

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

/** The orbit router (REFERENCES.md §5 — PoE's ring travel): when two
 *  consecutive anchors sit at (nearly) the same radius from `center`, the
 *  leg follows the CIRCLE between them — the short way around — instead
 *  of chording across; radius-changing legs stay straight spokes. This is
 *  how passive-tree edges read: nodes live on orbit rings {82, 162, 335,
 *  493} and their in-ring connections are arcs. `tolerance` is the
 *  radius-match slack as a fraction of the radius. */
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
