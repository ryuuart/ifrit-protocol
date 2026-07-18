#include "SceneLabels.h"

#include <include/core/SkPath.h>

namespace spellcircle {

float centeredBaselineOffset(const SkFontMetrics &metrics) {
  return -(metrics.fAscent + metrics.fDescent) * 0.5f;
}

const sk_sp<SkContourMeasure> &
RingLabelGeometryCache::ringForRadius(float radius) {
  const int quantizedRadius = static_cast<int>(radius * 4.0f);
  auto measuredRing = m_rings.find(quantizedRadius);
  if (measuredRing == m_rings.end()) {
    if (m_rings.size() >= m_maximumEntries)
      m_rings.clear();
    SkContourMeasureIter contourIterator(SkPath::Circle(0, 0, radius),
                                         /*forceClosed=*/false);
    measuredRing =
        m_rings.emplace(quantizedRadius, contourIterator.next()).first;
  }
  return measuredRing->second;
}

textflow::LineInterval makeRingLabelInterval(RingLabelGeometryCache &ringCache,
                                             const SkFontMetrics &metrics,
                                             float opticalMiddleRadius,
                                             float anchorFraction) {
  textflow::LineInterval interval;

  // Baselines sit inward of the optical-middle ring. Without the scale
  // correction the same glyph advances consume more angle at the smaller
  // baseline radius, making labels increasingly loose on small circles.
  const float baselineRadius =
      opticalMiddleRadius - centeredBaselineOffset(metrics);
  if (baselineRadius <= 1.0f || opticalMiddleRadius <= 1.0f)
    return interval;

  interval.contour = ringCache.ringForRadius(baselineRadius);
  if (!interval.contour)
    return interval;

  const float circumference = interval.contour->length();
  interval.advanceScale = baselineRadius / opticalMiddleRadius;
  interval.length = circumference / interval.advanceScale;
  // Closed contours wrap, so the center anchor may intentionally be negative.
  interval.contourStart = anchorFraction * circumference - circumference * 0.5f;
  return interval;
}

} // namespace spellcircle
