#pragma once

/** @file
 * Internal to the brush tier — the span arithmetic and the band region
 * behind the stroke grammar: the normal form every span answer is in, the
 * intersection law, the complement, the sub-path a span set covers, and
 * the region a spine sweeps at a width profile.
 */

#include <include/core/SkPath.h>
#include <sigilcompose/core/Mask.h>
#include <sigilcompose/core/Stroke.h>

#include <sigilcore/compute/Intervals.h>

#include <optional>
#include <vector>

namespace sigil::core {

/** A span's endpoints, for the interval algebra one library up. */
template <>
struct IntervalEnds<sigil::compose::Span> {
  using Value = float;
  static float& low(sigil::compose::Span& s) { return s.begin; }
  static float& high(sigil::compose::Span& s) { return s.end; }
  static const float& low(const sigil::compose::Span& s) { return s.begin; }
  static const float& high(const sigil::compose::Span& s) { return s.end; }
};

}  // namespace sigil::core

namespace sigil::compose::detail {

/** Clamp to [0,1], drop empties, sort and merge — the one normal form
 *  every span answer is in, so overlap tests and complements are honest
 *  interval arithmetic and not a pile of special cases. */
std::vector<Span> normalizeSpans(const std::vector<Span>& spans);
/** Everything in [0,1] the input does not cover (already normalized). */
std::vector<Span> complementSpans(const std::vector<Span>& spans);
/** THE INTERSECTION LAW, as arithmetic: the runs BOTH sets cover. Two
 *  masks on one target must both pass, and a span-qualified pass under a
 *  span-gated mask claims `where ∩ gate` — so the sweep that lights up a
 *  set of reticle brackets is one line and no re-authoring. Both inputs
 *  are normalized; the answer is too. */
std::vector<Span> intersectSpans(const std::vector<Span>& a,
                                 const std::vector<Span>& b);
/** Do these two normalized sets share more than float noise? Returns the
 *  first shared run, or nullopt. */
std::optional<Span> spansOverlap(const std::vector<Span>& a,
                                 const std::vector<Span>& b);
/** The sub-geometry of `src` covered by `spans` (fractions of the path's
 *  TOTAL arc length — SkTrimPathEffect's coordinate, so a span reveal and
 *  a trim of the same numbers describe the same run). */
SkPath spanPath(const SkPath& src, const std::vector<Span>& spans);
/** THE ONE STROKE ENGINE VALUE — what `stroke(spans, …)`,
 *  `background(spans, …)` and `across()` install on a description. */
const StrokeResolver& strokeResolver();
/** THE ONE MASK ENGINE VALUE — what every `by::` constructor installs on
 *  its gate. */
const MaskResolver& maskResolver();

}  // namespace sigil::compose::detail
