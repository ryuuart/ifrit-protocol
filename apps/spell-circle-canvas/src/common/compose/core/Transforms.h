#pragma once

/** @file
 * Internal to the kernel — the small transform arithmetic the phases share:
 * the largest local scale a matrix applies over a rect, a transform origin
 * resolved against a box, the 4x4 pieces the depth lanes compose and the
 * two questions asked of a composed one (which side faces the viewer,
 * where a rect lands), a corner set as a rounded rect, and the Yoga text
 * measure callbacks the reconciler installs.
 */

#include <include/core/SkM44.h>
#include <include/core/SkScalar.h>

#include <cmath>
#include <optional>

#include "Instance.h"

namespace sigil::compose::detail {

// ---------------------------------------------------------------------------
// The depth frame
//
// The 4x4 a node composes is written in CSS's frame — x right, y down and
// +z TOWARD the viewer — which is the frame the verbs are documented in and
// the one every CSS transform reference states its matrices in. It is NOT
// Skia's SkM44 frame, whose +z runs into the screen; none of Skia's own
// rotation, look-at or perspective constructors are used here for that
// reason, and nothing hands a 4x4 to the canvas either. What the canvas
// draws is the FLATTENING of the 4x4 — the 3x3 with a perspective row that
// remains when the z row and column are dropped, `SkM44::asM33()` — which
// is frame-independent and keeps every consumer of a node's matrix (the
// bakes, the local-scale estimate, the node→root accumulation, the hit
// test) on the one matrix type they already read.

constexpr float kDegreesToRadians = 0.017453293f;

// The sine and cosine of a lane's angle SNAP TO ZERO within Skia's own
// tolerance, exactly as SkMatrix::setRotate snaps them for the 2D rotate:
// a quarter turn is then a plane that is edge-on in fact — its flattening
// singular, so it draws nothing and answers no hit — rather than one
// 4e-8 wide, which would still hold a hit down its centre line.

/** CSS `rotateX(deg)`: positive tips the bottom edge toward the viewer. */
inline SkM44 rotateXMatrix(float degrees) {
  const float r = degrees * kDegreesToRadians;
  const float c = SkScalarCosSnapToZero(r), s = SkScalarSinSnapToZero(r);
  return SkM44(1, 0, 0, 0,  //
               0, c, -s, 0,  //
               0, s, c, 0,   //
               0, 0, 0, 1);
}

/** CSS `rotateY(deg)`: positive tips the left edge toward the viewer. */
inline SkM44 rotateYMatrix(float degrees) {
  const float r = degrees * kDegreesToRadians;
  const float c = SkScalarCosSnapToZero(r), s = SkScalarSinSnapToZero(r);
  return SkM44(c, 0, s, 0,   //
               0, 1, 0, 0,   //
               -s, 0, c, 0,  //
               0, 0, 0, 1);
}

/** The rotation about the viewing axis — `rotate()`'s own, as a 4x4: the
 *  same [cos −sin; sin cos] SkMatrix::setRotate writes, so the 2D lane
 *  turns the same way whichever producer builds it. */
inline SkM44 rotateZMatrix(float degrees) {
  const float r = degrees * kDegreesToRadians;
  const float c = SkScalarCosSnapToZero(r), s = SkScalarSinSnapToZero(r);
  return SkM44(c, -s, 0, 0,  //
               s, c, 0, 0,   //
               0, 0, 1, 0,   //
               0, 0, 0, 1);
}

/** The shear the 2D lanes apply, as a 4x4: `[1 kx; ky 1]`, the same two
 *  tangents SkMatrix::preSkew takes. */
inline SkM44 skewMatrix(float kx, float ky) {
  return SkM44(1, kx, 0, 0,  //
               ky, 1, 0, 0,  //
               0, 0, 1, 0,   //
               0, 0, 0, 1);
}

/** CSS `perspective(d)` about `origin`: the viewer stands `distance` in
 *  front of the plane over that point, so a point at depth z is divided
 *  by `1 − z/distance` — nearer grows, farther shrinks — and a point in
 *  the plane itself is untouched. A non-positive distance is no
 *  perspective: the identity, an orthographic projection. */
inline SkM44 perspectiveMatrix(float distance, SkPoint origin) {
  if (!(distance > 0)) return SkM44();
  SkM44 m;  // identity
  m.setRC(3, 2, -1.0f / distance);
  return SkM44::Translate(origin.x(), origin.y()) * m *
         SkM44::Translate(-origin.x(), -origin.y());
}

/** Is the viewer looking at the BACK of the plane this 4x4 places? The
 *  plane's normal is the z axis carried through the matrix, and a normal
 *  is carried by the inverse transpose, so its z component is the
 *  inverse's (2, 2) entry: negative means the plane has turned away. That
 *  entry is a cofactor over the determinant, which is what makes a 2D
 *  mirror stay visible — `scaleX(−1)` negates both and the sign holds —
 *  while a half turn about x or y flips exactly one. A matrix with no
 *  inverse has no facing, and is reported as away: such a plane is edge-on
 *  or collapsed and draws nothing either way. */
inline bool facesAway(const SkM44& m) {
  SkM44 inverse;
  if (!m.invert(&inverse)) return true;
  return inverse.rc(2, 2) < 0;
}

/** Where a local point lands in the plane a flattened 4x4 projects onto,
 *  or nothing when the point is at or behind the viewer — a point whose
 *  homogeneous w is not positive has no place on the plane, and dividing
 *  by it would put one anywhere. */
inline std::optional<SkPoint> projectPoint(const SkMatrix& flat, SkPoint p) {
  const float w = flat.getPerspX() * p.x() + flat.getPerspY() * p.y() +
                  flat.get(SkMatrix::kMPersp2);
  if (!std::isfinite(w) || w <= 1e-8f) return std::nullopt;
  const SkPoint q = flat.mapPoint(p);
  if (!q.isFinite()) return std::nullopt;
  return q;
}

/** The bounds of a local rect projected through a flattened 4x4: the box
 *  around whichever of its four corners land in front of the viewer, or
 *  an empty rect when none does. SkMatrix::mapRect maps corners behind
 *  the viewer to arbitrary places, so a projected rect is taken corner by
 *  corner instead. An affine matrix projects every corner and answers
 *  exactly what mapRect would. */
inline SkRect projectRect(const SkMatrix& flat, const SkRect& local) {
  if (!flat.hasPerspective()) return flat.mapRect(local);
  SkRect out = SkRect::MakeEmpty();
  bool any = false;
  for (SkPoint corner : {SkPoint{local.left(), local.top()},
                         SkPoint{local.right(), local.top()},
                         SkPoint{local.right(), local.bottom()},
                         SkPoint{local.left(), local.bottom()}}) {
    const std::optional<SkPoint> q = projectPoint(flat, corner);
    if (!q) continue;
    if (!any) {
      out = SkRect::MakeXYWH(q->x(), q->y(), 0, 0);
      any = true;
    } else {
      out.join(SkRect::MakeXYWH(q->x(), q->y(), 0, 0));
    }
  }
  return out;
}

/** The matrix's true maximum geometric scale over `local` — how many device
 *  pixels one local unit can span, whatever the rotation.
 *
 *  NOT max(|getScaleX()|, |getScaleY()|). Those are the matrix DIAGONAL, and
 *  a quarter turn moves the whole scale into the SKEW terms: at ±90° the
 *  diagonal is exactly (0, 0), because Skia's setRotate snaps cos(90°) to
 *  zero. A raster-target decision reading the diagonal therefore sees
 *  "scale 0" for a quarter-turned node, clamps to the caller's floor, and
 *  bakes at a fraction of device resolution to be linearly upscaled by the
 *  blit — a visible softening of everything the node contains. Singular
 *  values instead; a pure rotation reports 1.
 *
 *  Under PERSPECTIVE (getMinMaxScales refuses) the scale is
 *  position-dependent, so there is no one number for the whole plane, and
 *  falling back to the diagonal is wrong twice over: a rotation empties it
 *  exactly as above, and it reads no position at all while a projected
 *  quad's near edge magnifies well past it. The honest local answer is the
 *  JACOBIAN of the projective map, evaluated where the node actually is:
 *  the largest singular value of
 *  J(p) = (1/w)·[[a−gX, b−hX], [d−gY, e−hY]], taken at the center and four
 *  corners of `local` and maxed — for a plane the extremum over a convex
 *  quad sits at a corner (the one nearest the horizon), and max-over-
 *  samples errs in the CONSERVATIVE direction for every consumer: an
 *  overestimate steps a bake finer (memory, never wrong pixels), an
 *  underestimate ships a stale, blurry bake. Samples at or behind the
 *  horizon (w ≤ 0) have no finite local scale and are skipped; if every
 *  sample is degenerate the diagonal stands, bounded by the callers'
 *  clamps. */
inline float maxScaleOf(const SkMatrix& m, const SkRect& local) {
  SkScalar s[2];
  if (m.getMinMaxScales(s) && s[1] > 0) return s[1];
  if (m.hasPerspective()) {
    const auto sigmaMaxAt = [&m](SkPoint pt) -> float {
      const float w = m.getPerspX() * pt.x() + m.getPerspY() * pt.y() +
                      m.get(SkMatrix::kMPersp2);
      if (!std::isfinite(w) || w <= 1e-8f)
        return -1.0f;  // at/behind the horizon: no finite local scale
      const SkPoint q = m.mapPoint(pt);
      const float inv = 1.0f / w;
      const float j00 = (m.getScaleX() - m.getPerspX() * q.x()) * inv;
      const float j01 = (m.getSkewX() - m.getPerspY() * q.x()) * inv;
      const float j10 = (m.getSkewY() - m.getPerspX() * q.y()) * inv;
      const float j11 = (m.getScaleY() - m.getPerspY() * q.y()) * inv;
      // Largest singular value of the 2×2, closed form.
      const float e = j00 + j11, f = j00 - j11;
      const float g = j10 + j01, h = j10 - j01;
      const float sig =
          0.5f * (std::sqrt(e * e + h * h) + std::sqrt(f * f + g * g));
      return std::isfinite(sig) ? sig : -1.0f;
    };
    const SkPoint samples[5] = {{local.centerX(), local.centerY()},
                                {local.left(), local.top()},
                                {local.right(), local.top()},
                                {local.right(), local.bottom()},
                                {local.left(), local.bottom()}};
    float best = -1.0f;
    for (const SkPoint& pt : samples) best = std::max(best, sigmaMaxAt(pt));
    if (best > 0) return best;
  }
  // Degenerate (a zero matrix; a horizon through every sample): the
  // diagonal is all there is, and the callers' clamps bound it.
  return std::max(std::abs(m.getScaleX()), std::abs(m.getScaleY()));
}

/** The paint-transform pivot: fractional by default, node-local px under
 *  transformOriginPx(). One definition for paint(), recordBounds(), and
 *  the hit-test inverse. */
inline SkPoint resolveOrigin(const PaintProps& p, float w, float h) {
  return p.originPx ? SkPoint{p.originX, p.originY}
                    : SkPoint{w * p.originX, h * p.originY};
}

inline SkRRect cornersRRect(const SkRect& bounds, const Corners& c) {
  const SkVector radii[4] = {{c.topLeft, c.topLeft},
                             {c.topRight, c.topRight},
                             {c.bottomRight, c.bottomRight},
                             {c.bottomLeft, c.bottomLeft}};
  SkRRect rrect;
  rrect.setRectRadii(bounds, radii);
  return rrect;
}

// Yoga measure/baseline callbacks (defined in Layout.cpp; referenced by
// Reconcile.cpp when it installs them on a text leaf's YGNode).
YGSize measureTextNode(YGNodeConstRef node, float width,
                       YGMeasureMode widthMode, float heightHint,
                       YGMeasureMode heightMode);
float baselineOfTextNode(YGNodeConstRef node, float width, float height);

}  // namespace sigil::compose::detail
