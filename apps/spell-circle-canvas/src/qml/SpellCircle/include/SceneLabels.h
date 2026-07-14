#pragma once

// SpellCircle-specific label geometry. Text shaping and straight single-line
// layout remain in TextFlow; this layer owns only the measured circular paths
// and optical compensation needed by the scene renderer.

#include <textflow/Flow.h>

#include <absl/container/flat_hash_map.h>

#include <include/core/SkContourMeasure.h>
#include <include/core/SkFontMetrics.h>

#include <cstddef>

namespace spellcircle {

/** Returns the baseline offset that optically centers the font on a y value. */
float centeredBaselineOffset(const SkFontMetrics &metrics);

/** Caches origin-centered contour measurements by quarter-pixel radius. */
class RingLabelGeometryCache {
public:
  explicit RingLabelGeometryCache(size_t maximumEntries = 256)
      : m_maximumEntries(maximumEntries) {}

  /** Returns a measured origin-centered ring for `radius`. */
  const sk_sp<SkContourMeasure> &ringForRadius(float radius);

  /** Removes all measured rings and invalidates returned references. */
  void clear() { m_rings.clear(); }

private:
  absl::flat_hash_map<int, sk_sp<SkContourMeasure>> m_rings;
  size_t m_maximumEntries;
};

/**
 * Creates a center-anchored interval for text following a circle.
 *
 * Glyph optical centers ride `opticalMiddleRadius`. The measured baseline is
 * moved inward using the font metrics and advanceScale compensates for the
 * resulting difference in circumference. A degenerate radius returns an
 * empty interval with no contour.
 */
textflow::LineInterval makeRingLabelInterval(RingLabelGeometryCache &ringCache,
                                             const SkFontMetrics &metrics,
                                             float opticalMiddleRadius,
                                             float anchorFraction);

} // namespace spellcircle
