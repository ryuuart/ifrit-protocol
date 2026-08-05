#pragma once

// Label geometry for SpellCircle's ring diagrams: the measured circular paths a
// circle's name is drawn along, and the optical correction that keeps that text
// evenly spaced around them.
//
// This is the product-specific half of the split. Shaping, line breaking, and
// straight single-line layout belong to the general text library (SigilWeave),
// which knows how to run a pen along an arbitrary contour; only the business of
// deciding WHICH contour a spell-circle label follows lives here.

#include <absl/container/flat_hash_map.h>
#include <include/core/SkContourMeasure.h>
#include <include/core/SkFontMetrics.h>
#include <sigilweave/Flow.h>

#include <cstddef>

namespace spellcircle {

/** Returns the distance to add to a y coordinate to get the baseline that
 *  centers the font's ascent-to-descent band on that y. Positive for typical
 *  fonts, because Skia reports ascent as a negative number. */
float centeredBaselineOffset(const SkFontMetrics& metrics);

/** Measures circle contours once and hands them back on request, keyed by
 *  radius bucketed to quarter pixels: two radii inside the same quarter pixel
 *  share one measurement, a difference too small to move a glyph anywhere
 *  visible, and one that scaling a scene into canvas pixels produces
 * constantly.
 *
 *  Every cached ring is centered on the ORIGIN, not on its circle: the caller
 *  translates the canvas to the real center before drawing, so circles of equal
 *  size anywhere on the canvas share one entry. */
class RingLabelGeometryCache {
 public:
  explicit RingLabelGeometryCache(size_t maximumEntries = 256)
      : m_maximumEntries(maximumEntries) {}

  /** Returns the measured origin-centered ring for `radius`, measuring it on
   *  first request. Null for a radius of zero or less, which yields no path to
   *  measure; null results are never stored, so a degenerate request neither
   *  occupies a cache slot nor evicts anything. The reference is invalidated
   *  by clear() and by any later call that overflows the cache; copy the
   *  sk_sp to keep a measurement alive across calls. */
  const sk_sp<SkContourMeasure>& ringForRadius(float radius);

  /** Drops every measured ring. References previously returned by
   *  ringForRadius() dangle afterwards; sk_sp copies stay valid. */
  void clear() { m_rings.clear(); }

 private:
  absl::flat_hash_map<int, sk_sp<SkContourMeasure>> m_rings;
  size_t m_maximumEntries;
};

/**
 * Builds the interval a circle's label is laid out along: one closed contour,
 * offering the whole ring, anchored so that centered text reads around the
 * circle from `anchorFraction`.
 *
 * Glyph optical centers ride `opticalMiddleRadius`, but the pen has to travel a
 * contour at the BASELINE, which sits inward of that by the font's centered
 * baseline offset. Laid out naively, each advance would then subtend more angle
 * than intended and the label would read loose — worse the smaller the circle —
 * so the interval reports the optical ring's circumference as its length and
 * sets advanceScale to the baseline/optical radius ratio, leaving fitting and
 * alignment in unscaled advance units and scaling only the pen-to-arc mapping.
 *
 * `anchorFraction` is where the middle of the text lands, as a fraction of the
 * way around the ring from the contour's own start point. A circle contour
 * begins at 3 o'clock and winds clockwise, so 0.75 centers a label at the top.
 * Anchoring assumes the caller lays the paragraph out with center alignment.
 *
 * Returns an interval with a null contour — nothing to draw — when either
 * radius degenerates to a pixel or less.
 */
sigil::weave::LineInterval makeRingLabelInterval(
    RingLabelGeometryCache& ringCache, const SkFontMetrics& metrics,
    float opticalMiddleRadius, float anchorFraction);

}  // namespace spellcircle
