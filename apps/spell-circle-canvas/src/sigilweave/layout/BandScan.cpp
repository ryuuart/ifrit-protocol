/** @file
 * The band scan itself: the two coordinates, and the occupied stretches of
 * one band of a flattened outline.
 */

#include "BandScan.h"

#include <algorithm>

namespace sigil::weave::detail {

float alongOf(FlowAxis axis, const glm::vec2& point) {
  return axis == FlowAxis::kColumns ? point.y : point.x;
}
float acrossOf(FlowAxis axis, const glm::vec2& point) {
  return axis == FlowAxis::kColumns ? point.x : point.y;
}
float alongMin(FlowAxis axis, const SkRect& rect) {
  return axis == FlowAxis::kColumns ? rect.top() : rect.left();
}
float alongMax(FlowAxis axis, const SkRect& rect) {
  return axis == FlowAxis::kColumns ? rect.bottom() : rect.right();
}
float acrossMin(FlowAxis axis, const SkRect& rect) {
  return axis == FlowAxis::kColumns ? rect.left() : rect.top();
}
float acrossMax(FlowAxis axis, const SkRect& rect) {
  return axis == FlowAxis::kColumns ? rect.right() : rect.bottom();
}

void bandOccupancy(const std::vector<std::vector<glm::vec2>>& contours,
                   bool evenOdd, FlowAxis axis, float bandStart, float bandEnd,
                   std::vector<std::pair<float, float>>& occupiedSpans) {
  static thread_local std::vector<std::pair<float, int>> crossings;
  const float scanlines[3] = {bandStart + kBandEpsilon,
                              (bandStart + bandEnd) * 0.5f,
                              bandEnd - kBandEpsilon};
  for (const float scanline : scanlines) {
    crossings.clear();
    for (const std::vector<glm::vec2>& polygon : contours) {
      for (size_t pointIndex = 0; pointIndex < polygon.size(); ++pointIndex) {
        const glm::vec2& startPoint = polygon[pointIndex];
        const glm::vec2& endPoint = polygon[(pointIndex + 1) % polygon.size()];
        const float startAcross = acrossOf(axis, startPoint);
        const float endAcross = acrossOf(axis, endPoint);
        if (startAcross == endAcross) continue;
        // Half-open [min, max) so shared vertices count exactly once.
        const bool travelsUp = endAcross > startAcross;
        if (travelsUp ? (scanline < startAcross || scanline >= endAcross)
                      : (scanline < endAcross || scanline >= startAcross))
          continue;
        const float interpolation =
            (scanline - startAcross) / (endAcross - startAcross);
        crossings.emplace_back(alongOf(axis, startPoint) +
                                   interpolation * (alongOf(axis, endPoint) -
                                                    alongOf(axis, startPoint)),
                               travelsUp ? 1 : -1);
      }
    }
    std::sort(crossings.begin(), crossings.end());
    int winding = 0;
    unsigned parity = 0;
    bool inside = false;
    float openAlong = 0;
    for (const auto& [crossingAlong, windingDelta] : crossings) {
      winding += windingDelta;
      parity ^= 1u;
      const bool nowInside = evenOdd ? parity != 0 : winding != 0;
      if (nowInside && !inside) {
        openAlong = crossingAlong;
        inside = true;
      } else if (!nowInside && inside) {
        occupiedSpans.emplace_back(openAlong, crossingAlong);
        inside = false;
      }
    }
  }

  for (const std::vector<glm::vec2>& polygon : contours) {
    for (size_t pointIndex = 0; pointIndex < polygon.size(); ++pointIndex) {
      const glm::vec2& startPoint = polygon[pointIndex];
      const glm::vec2& endPoint = polygon[(pointIndex + 1) % polygon.size()];
      const float startAcross = acrossOf(axis, startPoint);
      const float endAcross = acrossOf(axis, endPoint);
      const float edgeNear = std::min(startAcross, endAcross);
      const float edgeFar = std::max(startAcross, endAcross);
      if (edgeFar <= bandStart || edgeNear >= bandEnd) continue;
      float startFraction = 0;
      float endFraction = 1;
      if (startAcross != endAcross) {
        const float nearFraction =
            (bandStart - startAcross) / (endAcross - startAcross);
        const float farFraction =
            (bandEnd - startAcross) / (endAcross - startAcross);
        startFraction =
            std::clamp(std::min(nearFraction, farFraction), 0.0f, 1.0f);
        endFraction =
            std::clamp(std::max(nearFraction, farFraction), 0.0f, 1.0f);
      }
      const float startAlong = alongOf(axis, startPoint);
      const float alongTravel = alongOf(axis, endPoint) - startAlong;
      const float spanStart = startAlong + startFraction * alongTravel;
      const float spanEnd = startAlong + endFraction * alongTravel;
      occupiedSpans.emplace_back(std::min(spanStart, spanEnd),
                                 std::max(spanStart, spanEnd));
    }
  }
}

void mergeSpans(std::vector<std::pair<float, float>>& spans) {
  std::sort(spans.begin(), spans.end());
  size_t mergedCount = 0;
  for (const auto& span : spans) {
    if (mergedCount > 0 && span.first <= spans[mergedCount - 1].second)
      spans[mergedCount - 1].second =
          std::max(spans[mergedCount - 1].second, span.second);
    else
      spans[mergedCount++] = span;
  }
  spans.resize(mergedCount);
}

}  // namespace sigil::weave::detail
