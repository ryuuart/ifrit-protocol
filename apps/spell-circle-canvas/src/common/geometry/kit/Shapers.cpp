/** @file
 * The one stock shaper whose body is the renderer's own path effect: a
 * seeded discrete jitter over the mark.
 */

#include "sigilgeometry/kit/Shapers.h"

#include <include/core/SkPathBuilder.h>
#include <include/core/SkStrokeRec.h>
#include <include/effects/SkDiscretePathEffect.h>

#include "sigilgeometry/advanced/Skia.h"

namespace sigil::geometry::shapers {

path::Outline Jitter::shape(const path::Outline& outline) const {
  const SkPath p = path::toSk(outline);
  SkPathBuilder out;
  // HAIRLINE rec is required: under a fill rec SkDiscretePathEffect
  // force-CLOSES open contours, so an open mark gains a return chord
  // from its end back to its start — which then jitters away from the
  // real run and draws as a second, phantom line.
  SkStrokeRec rec(SkStrokeRec::kHairline_InitStyle);
  if (sk_sp<SkPathEffect> fx =
          SkDiscretePathEffect::Make(segmentLength, deviation, seed);
      fx && fx->filterPath(&out, p, &rec))
    return path::fromSk(out.detach());
  return outline;
}

}  // namespace sigil::geometry::shapers
