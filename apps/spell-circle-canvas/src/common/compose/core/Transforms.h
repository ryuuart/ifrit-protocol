#pragma once

/** @file
 * Internal to the kernel — the small transform arithmetic the phases share:
 * the largest local scale a matrix applies over a rect, a transform origin
 * resolved against a box, a corner set as a rounded rect, and the Yoga text
 * measure callbacks the reconciler installs.
 */

#include "Instance.h"

namespace sigil::compose::detail {

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
