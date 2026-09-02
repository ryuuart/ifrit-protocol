/** @file
 * What the library says, once, when a declaration resolves to nothing: a
 * corner treatment that finds no corners on a shape that plainly has
 * vertices, a writing mode a path run cannot honour, a paragraph style
 * name no set in scope carries.
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
#include <cmath>  // std::isfinite — the geometry::path::profileOffset non-finite guard
#include <cstdio>  // std::snprintf — variationDrive's effect key
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>  // the once-per-name diagnostics' seen sets

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
 *  attached: `geometry::path::Contour::corners` reports the sharpest turn it
 *  saw, and a scan that found nothing on a contour whose sharpest turn
 *  is above the noise a smooth curve produces at this step (4°) says so
 *  once. */
std::vector<geometry::path::Contour::Corner> cornersOrWarn(
    const geometry::path::Contour& contour, float angleDeg, float minSpacing,
    float step) {
  float sharpestDeg = 0.0f;
  std::vector<geometry::path::Contour::Corner> corners =
      contour.corners(angleDeg, minSpacing, step, &sharpestDeg);
  if (corners.empty() && sharpestDeg >= 4.0f)
    warnNoCornersFound(sharpestDeg, angleDeg);
  return corners;
}

/** The same diagnostic for a whole path, ahead of a corner window
 *  construction that reports nothing itself. */
void warnIfNoCorners(const SkPath& src, float angleDeg) {
  for (const geometry::path::Contour& contour :
       geometry::path::Contour::of(src))
    (void)cornersOrWarn(contour, angleDeg);
}

void warnWritingModeOnPath() {
  static thread_local bool warned = false;
  if (warned) return;
  warned = true;
  std::fprintf(stderr,
               "SigilCompose: onPath() and writingMode() on one text leaf — "
               "a path run's baseline IS its geometry and has no columns to "
               "advance, so the path stands and the writing mode is dropped\n");
}

void warnNoSuchParagraphStyle(std::string_view name, bool anySetInScope) {
  // Once per distinct name: a description re-runs every frame and a name
  // that is wrong is wrong every time.
  static thread_local std::unordered_set<std::string> seen;
  if (!seen.insert(std::string(name)).second) return;
  std::fprintf(
      stderr,
      "SigilCompose: paragraphs(\"%.*s\") — %s, so this block is set in a "
      "plain default. Register it with ParagraphStyleSet::set() and provide "
      "the set above this element (env::Provide<weave::ParagraphStyleSet>), "
      "or pass the style itself.\n",
      (int)name.size(), name.data(),
      anySetInScope ? "the paragraph style set in scope carries no such name"
                    : "no paragraph style set is in scope");
}

}  // namespace detail

}  // namespace sigil::compose
