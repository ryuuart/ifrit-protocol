/** @file
 * The corner-scan diagnostics: what the library says, once, when a corner
 * treatment finds no corners on a shape that plainly has vertices.
 */

#include <include/core/SkContourMeasure.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkShader.h>
#include <include/core/SkTypes.h>  // SkDebugf — the slot-rename diagnostic
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/pathops/SkPathOps.h>

#include <algorithm>
#include <cmath>   // std::isfinite — the profileOffset non-finite guard
#include <cstdio>  // std::snprintf — variationDrive's effect key
#include <set>

#include "ComposeInternal.h"
#include "sigilgeometry/path/Contour.h"

namespace sigil::compose {

namespace detail {

/** Says so when a corner scan found nothing but the shape clearly has
 *  vertices — the alternative being a bracket set that renders blank while
 *  the API does exactly what it was told.
 *
 *  Why this is a diagnostic and NOT an adaptive default: the threshold's
 *  whole job is to tell a VERTEX from a finely-sampled CURVE, and a
 *  rounded corner is meant to take no bracket (stated at `cornerAngleDeg`).
 *  The scan steps 2 px, so an arc of radius r turns ~114/r degrees per
 *  sample — about 11° at r = 10. Any auto-lowered threshold low enough to
 *  catch a 20-gon's 18° vertices is also low enough to shatter a small
 *  rounded corner into a run of false ones. So the number stays the
 *  author's, and the library explains what to pass. */
void warnNoCornersFound(float sharpestDeg, float angleDeg) {
  // A SET of seen shapes, not the last one: two different failing shapes in
  // one frame alternate, and a last-seen guard then prints both on every
  // frame forever — a diagnostic that floods is a diagnostic people turn
  // off. Capped so a procedurally varied scene cannot grow it without
  // bound.
  static std::vector<int> seen;
  const int key = (int)std::lround(sharpestDeg);
  for (int k : seen)
    if (k == key) return;
  if (seen.size() >= 16) return;
  seen.push_back(key);
  SkDebugf(
      "compose: no corner cleared the %.1f\xc2\xb0 threshold, but the "
      "sharpest tangent break on this contour is %.1f\xc2\xb0 — so "
      "weightedCorners and spans::corners() will "
      "draw nothing here, and spans::edges() (their complement) will "
      "claim the WHOLE boundary instead of stopping short of "
      "anything. A "
      "regular n-gon turns 360/n per vertex, which puts EVERY polygon "
      "above 12 sides under the 30\xc2\xb0 default (a 20-gon turns "
      "18\xc2\xb0). Pass a smaller angleDeg, e.g. %.0ff.\n",
      angleDeg, sharpestDeg, std::max(4.0f, sharpestDeg * 0.6f));
}

/** The corner scan every decoration shares, with the diagnostic above
 *  attached: `geometry::Contour::corners` reports the sharpest turn it
 *  saw, and a scan that found nothing on a contour whose sharpest turn
 *  is above the noise a smooth curve produces at this step (4°) says so
 *  once. */
std::vector<geometry::Contour::Corner> cornersOrWarn(
    const geometry::Contour& contour, float angleDeg, float minSpacing,
    float step) {
  float sharpestDeg = 0.0f;
  std::vector<geometry::Contour::Corner> corners =
      contour.corners(angleDeg, minSpacing, step, &sharpestDeg);
  if (corners.empty() && sharpestDeg >= 4.0f)
    warnNoCornersFound(sharpestDeg, angleDeg);
  return corners;
}

/** The same diagnostic for a whole path, ahead of a corner window
 *  construction that reports nothing itself. */
void warnIfNoCorners(const SkPath& src, float angleDeg) {
  for (const geometry::Contour& contour : geometry::Contour::of(src))
    (void)cornersOrWarn(contour, angleDeg);
}

}  // namespace detail

}  // namespace sigil::compose
