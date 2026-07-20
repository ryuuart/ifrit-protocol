#include "sigilweave/Flow.h"

#include <include/core/SkPathTypes.h>

#include <absl/container/flat_hash_map.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace sigil::weave {

namespace {

constexpr float kEps = 0.01f;

// Removes [excludedStart, excludedEnd] from every sorted, disjoint interval.
void subtractSpan(std::vector<std::pair<float, float>> &availableSpans,
                  float excludedStart, float excludedEnd) {
  std::vector<std::pair<float, float>> remainingSpans;
  remainingSpans.reserve(availableSpans.size() + 1);
  for (const auto &[spanStart, spanEnd] : availableSpans) {
    if (excludedEnd <= spanStart || excludedStart >= spanEnd) {
      remainingSpans.emplace_back(spanStart, spanEnd);
      continue;
    }
    if (excludedStart > spanStart)
      remainingSpans.emplace_back(spanStart, excludedStart);
    if (excludedEnd < spanEnd)
      remainingSpans.emplace_back(excludedEnd, spanEnd);
  }
  availableSpans = std::move(remainingSpans);
}

// Occupied x-intervals of a flattened polygon set within the band
// [top, bottom]: fill intervals sampled at three scanlines (respecting the
// fill rule, so holes and concave gaps stay open) unioned with every edge's
// x-travel through the band (conservative — catches features that fall
// between the samples, like a star tip). Appends unmerged occupied spans.
void bandOccupancy(const std::vector<std::vector<SkPoint>> &contours,
                   bool evenOdd, float top, float bottom,
                   std::vector<std::pair<float, float>> &occupiedSpans) {
  static thread_local std::vector<std::pair<float, int>> crossings;
  const float scanlines[3] = {top + kEps, (top + bottom) * 0.5f, bottom - kEps};
  for (const float scanlineY : scanlines) {
    crossings.clear();
    for (const std::vector<SkPoint> &polygon : contours) {
      for (size_t pointIndex = 0; pointIndex < polygon.size(); ++pointIndex) {
        const SkPoint &startPoint = polygon[pointIndex];
        const SkPoint &endPoint = polygon[(pointIndex + 1) % polygon.size()];
        const float startY = startPoint.y();
        const float endY = endPoint.y();
        if (startY == endY)
          continue;
        // Half-open [min, max) so shared vertices count exactly once.
        const bool travelsUp = endY > startY;
        if (travelsUp ? (scanlineY < startY || scanlineY >= endY)
                      : (scanlineY < endY || scanlineY >= startY))
          continue;
        const float interpolation = (scanlineY - startY) / (endY - startY);
        crossings.emplace_back(
            startPoint.x() + interpolation * (endPoint.x() - startPoint.x()),
            travelsUp ? 1 : -1);
      }
    }
    std::sort(crossings.begin(), crossings.end());
    int winding = 0, parity = 0;
    bool inside = false;
    float openX = 0;
    for (const auto &[crossingX, windingDelta] : crossings) {
      winding += windingDelta;
      parity ^= 1;
      const bool nowInside = evenOdd ? parity != 0 : winding != 0;
      if (nowInside && !inside) {
        openX = crossingX;
        inside = true;
      } else if (!nowInside && inside) {
        occupiedSpans.emplace_back(openX, crossingX);
        inside = false;
      }
    }
  }

  for (const std::vector<SkPoint> &polygon : contours) {
    for (size_t pointIndex = 0; pointIndex < polygon.size(); ++pointIndex) {
      const SkPoint &startPoint = polygon[pointIndex];
      const SkPoint &endPoint = polygon[(pointIndex + 1) % polygon.size()];
      const float edgeTop = std::min(startPoint.y(), endPoint.y());
      const float edgeBottom = std::max(startPoint.y(), endPoint.y());
      if (edgeBottom <= top || edgeTop >= bottom)
        continue;
      float startFraction = 0;
      float endFraction = 1;
      if (startPoint.y() != endPoint.y()) {
        const float topFraction =
            (top - startPoint.y()) / (endPoint.y() - startPoint.y());
        const float bottomFraction =
            (bottom - startPoint.y()) / (endPoint.y() - startPoint.y());
        startFraction =
            std::clamp(std::min(topFraction, bottomFraction), 0.0f, 1.0f);
        endFraction =
            std::clamp(std::max(topFraction, bottomFraction), 0.0f, 1.0f);
      }
      const float startX =
          startPoint.x() + startFraction * (endPoint.x() - startPoint.x());
      const float endX =
          startPoint.x() + endFraction * (endPoint.x() - startPoint.x());
      occupiedSpans.emplace_back(std::min(startX, endX),
                                 std::max(startX, endX));
    }
  }
}

void mergeSpans(std::vector<std::pair<float, float>> &spans) {
  std::sort(spans.begin(), spans.end());
  size_t mergedCount = 0;
  for (const auto &span : spans) {
    if (mergedCount > 0 && span.first <= spans[mergedCount - 1].second)
      spans[mergedCount - 1].second =
          std::max(spans[mergedCount - 1].second, span.second);
    else
      spans[mergedCount++] = span;
  }
  spans.resize(mergedCount);
}

} // namespace

// Flattened-polygon form of an exclusion SkPath, cached by generation ID.
struct ExclusionFlow::FlatPath {
  std::vector<std::vector<SkPoint>> contours; // closed polylines
  SkRect bounds = SkRect::MakeEmpty();
  bool evenOdd = false;
};

// Private container definition: keeps the hash-map dependency out of the
// public Flow.h. unique_ptr values keep FlatPath addresses stable across
// rehashes.
struct ExclusionFlow::PathCache {
  absl::flat_hash_map<uint32_t, std::unique_ptr<FlatPath>> entries;
};

ExclusionFlow::ExclusionFlow(const SkRect &bounds)
    : m_bounds(bounds), m_pathCache(std::make_unique<PathCache>()) {}
ExclusionFlow::~ExclusionFlow() = default;

const ExclusionFlow::FlatPath &
ExclusionFlow::flattenedPathFor(const SkPath &path) {
  if (!m_pathCache) // re-arm a moved-from flow instead of dereferencing null
    m_pathCache = std::make_unique<PathCache>();
  auto &cache = m_pathCache->entries;
  const uint32_t generationId = path.getGenerationID();
  auto cachedPath = cache.find(generationId);
  if (cachedPath != cache.end())
    return *cachedPath->second;
  if (cache.size() > 64)
    cache.clear(); // Bound animated path churn.

  auto flattenedPath = std::make_unique<FlatPath>();
  const SkPathFillType fill = path.getFillType();
  flattenedPath->evenOdd = fill == SkPathFillType::kEvenOdd ||
                           fill == SkPathFillType::kInverseEvenOdd;
  flattenedPath->bounds = path.computeTightBounds();

  // Fixed-count curve subdivision: layout avoidance needs a couple of pixels
  // of fidelity, not rendering accuracy.
  constexpr int kCurveSegs = 12;
  std::vector<SkPoint> polygon;
  auto flushPolygon = [&] {
    if (polygon.size() >= 3)
      flattenedPath->contours.push_back(std::move(polygon));
    polygon.clear();
  };

  SkPath::Iter pathIterator(path, /*forceClose=*/true);
  SkPoint controlPoints[4];
  for (;;) {
    const SkPath::Verb verb = pathIterator.next(controlPoints);
    if (verb == SkPath::kDone_Verb)
      break;
    switch (verb) {
    case SkPath::kMove_Verb:
      flushPolygon();
      polygon.push_back(controlPoints[0]);
      break;
    case SkPath::kLine_Verb:
      polygon.push_back(controlPoints[1]);
      break;
    case SkPath::kQuad_Verb:
      for (int segmentIndex = 1; segmentIndex <= kCurveSegs; ++segmentIndex) {
        const float fraction = static_cast<float>(segmentIndex) / kCurveSegs;
        const float complement = 1 - fraction;
        polygon.push_back(
            {complement * complement * controlPoints[0].x() +
                 2 * complement * fraction * controlPoints[1].x() +
                 fraction * fraction * controlPoints[2].x(),
             complement * complement * controlPoints[0].y() +
                 2 * complement * fraction * controlPoints[1].y() +
                 fraction * fraction * controlPoints[2].y()});
      }
      break;
    case SkPath::kConic_Verb: {
      const float conicWeight = pathIterator.conicWeight();
      for (int segmentIndex = 1; segmentIndex <= kCurveSegs; ++segmentIndex) {
        const float fraction = static_cast<float>(segmentIndex) / kCurveSegs;
        const float complement = 1 - fraction;
        const float denominator = complement * complement +
                                  2 * conicWeight * complement * fraction +
                                  fraction * fraction;
        polygon.push_back(
            {(complement * complement * controlPoints[0].x() +
              2 * conicWeight * complement * fraction * controlPoints[1].x() +
              fraction * fraction * controlPoints[2].x()) /
                 denominator,
             (complement * complement * controlPoints[0].y() +
              2 * conicWeight * complement * fraction * controlPoints[1].y() +
              fraction * fraction * controlPoints[2].y()) /
                 denominator});
      }
      break;
    }
    case SkPath::kCubic_Verb:
      for (int segmentIndex = 1; segmentIndex <= kCurveSegs; ++segmentIndex) {
        const float fraction = static_cast<float>(segmentIndex) / kCurveSegs;
        const float complement = 1 - fraction;
        polygon.push_back(
            {complement * complement * complement * controlPoints[0].x() +
                 3 * complement * complement * fraction * controlPoints[1].x() +
                 3 * complement * fraction * fraction * controlPoints[2].x() +
                 fraction * fraction * fraction * controlPoints[3].x(),
             complement * complement * complement * controlPoints[0].y() +
                 3 * complement * complement * fraction * controlPoints[1].y() +
                 3 * complement * fraction * fraction * controlPoints[2].y() +
                 fraction * fraction * fraction * controlPoints[3].y()});
      }
      break;
    case SkPath::kClose_Verb:
      flushPolygon();
      break;
    default:
      break;
    }
  }
  flushPolygon();

  auto cacheEntry =
      cache.emplace(generationId, std::move(flattenedPath)).first;
  return *cacheEntry->second;
}

bool BlockFlow::lineIntervals(int index, float lineHeight, float ascent,
                              std::vector<LineInterval> &intervals) {
  intervals.clear();
  const float top = m_bounds.top() + static_cast<float>(index) * lineHeight;
  if (top + lineHeight > m_bounds.bottom() + kEps)
    return false;
  LineInterval interval;
  interval.origin = {m_bounds.left(), top + ascent};
  interval.direction = {1, 0};
  interval.length = m_bounds.width();
  intervals.push_back(interval);
  return true;
}

bool ExclusionFlow::lineIntervals(int index, float lineHeight, float ascent,
                                  std::vector<LineInterval> &intervals) {
  intervals.clear();
  const float top = m_bounds.top() + static_cast<float>(index) * lineHeight;
  const float bottom = top + lineHeight;
  if (bottom > m_bounds.bottom() + kEps)
    return false;

  std::vector<std::pair<float, float>> availableSpans = {
      {m_bounds.left(), m_bounds.right()}};

  static thread_local std::vector<std::pair<float, float>> occupiedSpans;
  for (const Shape &shape : m_shapes) {
    if (shape.kind == Shape::kPath) {
      const FlatPath &flattenedPath = flattenedPathFor(shape.path);
      if (flattenedPath.contours.empty())
        continue;
      const float offsetX = shape.pathOffset.x();
      const float offsetY = shape.pathOffset.y();
      const float padding = shape.padding;
      if (flattenedPath.bounds.bottom() + offsetY + padding <= top ||
          flattenedPath.bounds.top() + offsetY - padding >= bottom)
        continue;
      // Band and results are in path-local space (shifted by the offset);
      // padding expands the band vertically and each span horizontally.
      occupiedSpans.clear();
      bandOccupancy(flattenedPath.contours, flattenedPath.evenOdd,
                    top - offsetY - padding, bottom - offsetY + padding,
                    occupiedSpans);
      mergeSpans(occupiedSpans);
      for (const auto &[spanStart, spanEnd] : occupiedSpans)
        subtractSpan(availableSpans, spanStart + offsetX - padding,
                     spanEnd + offsetX + padding);
    } else if (shape.kind == Shape::kCircle) {
      const float radius =
          std::min(shape.bounds.width(), shape.bounds.height()) * 0.5f +
          shape.padding;
      const float centerX = shape.bounds.centerX();
      const float centerY = shape.bounds.centerY();
      if (centerY + radius <= top || centerY - radius >= bottom)
        continue;
      // Widest chord of the circle within the band: at centerY if the band
      // contains it, else at the nearest band edge.
      const float distanceFromCenter =
          centerY < top ? top - centerY
                        : (centerY > bottom ? centerY - bottom : 0);
      if (distanceFromCenter >= radius)
        continue;
      const float halfChord =
          std::sqrt(radius * radius - distanceFromCenter * distanceFromCenter);
      subtractSpan(availableSpans, centerX - halfChord, centerX + halfChord);
    } else {
      const SkRect paddedBounds =
          shape.bounds.makeOutset(shape.padding, shape.padding);
      if (paddedBounds.bottom() <= top || paddedBounds.top() >= bottom)
        continue;
      subtractSpan(availableSpans, paddedBounds.left(), paddedBounds.right());
    }
    if (availableSpans.empty())
      break;
  }

  for (const auto &[spanStart, spanEnd] : availableSpans) {
    if (spanEnd - spanStart < m_minIntervalWidth)
      continue;
    LineInterval interval;
    interval.origin = {spanStart, top + ascent};
    interval.direction = {1, 0};
    interval.length = spanEnd - spanStart;
    intervals.push_back(interval);
  }
  return true;
}

bool VerticalBlockFlow::lineIntervals(int index, float lineHeight,
                                      float /*ascent*/,
                                      std::vector<LineInterval> &intervals) {
  intervals.clear();
  const float right = m_bounds.right() - static_cast<float>(index) * lineHeight;
  if (right - lineHeight < m_bounds.left() - kEps)
    return false;
  LineInterval interval;
  interval.origin = {right - lineHeight * 0.5f, m_bounds.top()};
  interval.direction = {0, 1};
  interval.length = m_bounds.height();
  intervals.push_back(interval);
  return true;
}

bool LineSetFlow::lineIntervals(int index, float /*lineHeight*/,
                                float /*ascent*/,
                                std::vector<LineInterval> &intervals) {
  intervals.clear();
  if (index < 0 || static_cast<size_t>(index) >= m_lines.size())
    return false;
  intervals = m_lines[static_cast<size_t>(index)];
  return true;
}

PathFlow::PathFlow(const SkPath &path) { addPath(path); }

void PathFlow::addPath(const SkPath &path) {
  SkContourMeasureIter contourIterator(path, /*forceClosed=*/false);
  while (sk_sp<SkContourMeasure> contour = contourIterator.next())
    m_contours.push_back(std::move(contour));
}

bool PathFlow::lineIntervals(int index, float /*lineHeight*/, float /*ascent*/,
                             std::vector<LineInterval> &intervals) {
  intervals.clear();
  if (index < 0 || static_cast<size_t>(index) >= m_contours.size())
    return false;
  LineInterval interval;
  interval.contour = m_contours[static_cast<size_t>(index)];
  interval.contourStart = 0;
  interval.length = interval.contour->length();
  intervals.push_back(interval);
  return true;
}

} // namespace sigil::weave
