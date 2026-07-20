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
