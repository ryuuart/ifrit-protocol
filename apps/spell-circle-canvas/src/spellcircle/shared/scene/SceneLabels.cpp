#include "SceneLabels.h"

#include <include/core/SkPath.h>

namespace spellcircle {

float centeredBaselineOffset(const SkFontMetrics& metrics) {
  return -(metrics.fAscent + metrics.fDescent) * 0.5f;
}

const sk_sp<SkContourMeasure>& RingLabelGeometryCache::ringForRadius(
    float radius) {
  const int quantizedRadius = static_cast<int>(radius * 4.0f);
  auto measuredRing = m_rings.find(quantizedRadius);
  if (measuredRing == m_rings.end()) {
    SkContourMeasureIter contourIterator(SkPath::Circle(0, 0, radius),
                                         /*forceClosed=*/false);
    sk_sp<SkContourMeasure> ring = contourIterator.next();
    // A radius that yields no contour produces nothing worth keeping: storing
    // the null would pin a permanently useless entry in its bucket, so
    // degenerate requests are answered without touching the cache — they
    // neither occupy a slot nor trigger the overflow flush below.
    if (!ring) {
      static const sk_sp<SkContourMeasure> kNoRing;
      return kNoRing;
    }
    // Overflow drops every measurement at once rather than evicting a least
    // recently used entry: no use order is tracked, and re-measuring is a
    // contour walk over four conics. Rings already handed out survive, because
    // callers hold sk_sp copies.
    if (m_rings.size() >= m_maximumEntries) m_rings.clear();
    measuredRing = m_rings.emplace(quantizedRadius, std::move(ring)).first;
  }
  return measuredRing->second;
}

sigil::weave::LineInterval makeRingLabelInterval(
    RingLabelGeometryCache& ringCache, const SkFontMetrics& metrics,
    float opticalMiddleRadius, float anchorFraction) {
  sigil::weave::LineInterval interval;

  // The pen rides the baseline, which sits inward of the ring the glyphs'
  // optical centers are meant to follow.
  const float baselineRadius =
      opticalMiddleRadius - centeredBaselineOffset(metrics);
  // Both radii have to be large enough to measure and to divide by below.
  if (baselineRadius <= 1.0f || opticalMiddleRadius <= 1.0f) return interval;

  interval.contour = ringCache.ringForRadius(baselineRadius);
  if (!interval.contour) return interval;

  // `circumference` is the baseline ring's. Shrinking each advance by the
  // radius ratio is what makes glyph advances measured for the wider optical
  // ring land correctly on this shorter one; dividing back out gives a length
  // in unscaled advance units, i.e. the optical circumference, which offers the
  // whole ring to the line.
  const float circumference = interval.contour->length();
  interval.advanceScale = baselineRadius / opticalMiddleRadius;
  interval.length = circumference / interval.advanceScale;
  // Center alignment starts the text half an interval in, so entering the
  // contour half a circumference before `anchorFraction` puts the middle of the
  // label there. Closed contours wrap, so the negative start this produces for
  // anchors in the first half of the ring is intended, not a clamp waiting to
  // happen.
  interval.contourStart = anchorFraction * circumference - circumference * 0.5f;
  return interval;
}

}  // namespace spellcircle
