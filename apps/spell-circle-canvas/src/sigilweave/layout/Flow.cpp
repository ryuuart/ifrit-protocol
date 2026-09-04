/** @file
 * The ready-made flow geometries: a block, a block minus exclusion shapes
 * flattened by generation ID and scanned band by band — in lines or in
 * columns, one scan turned a quarter turn by its FlowAxis — vertical
 * columns, an explicit line set, and one line per path contour, and the
 * placement of a pen coordinate on a contour interval with its tangent
 * snapped.
 */

#include "sigilweave/layout/Flow.h"

#include <include/core/SkPathTypes.h>

#include <algorithm>
#include <boost/unordered/unordered_flat_map.hpp>
#include <cmath>
#include <glm/vec2.hpp>
#include <numbers>
#include <utility>

#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::weave {

namespace {

constexpr float kEps = 0.01f;

// Snaps a unit tangent to one of `steps` directions (512 → 0.7° steps —
// invisible). Continuously varying per-glyph rotations would otherwise mint
// a fresh glyph-atlas strike every frame for every glyph on a moving path,
// turning animated curved text into a per-frame mask-rasterization storm.
SkVector quantizeTangent(SkVector tangent, int directionCount) {
  if (directionCount <= 0) return tangent;
  constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
  const float angle = std::atan2(tangent.fY, tangent.fX);
  int directionIndex =
      static_cast<int>(std::lround(angle / kTwoPi * directionCount)) %
      directionCount;
  if (directionIndex < 0) directionIndex += directionCount;
  const float snapped =
      static_cast<float>(directionIndex) * kTwoPi / directionCount;
  return {std::cos(snapped), std::sin(snapped)};
}

// THE TWO COORDINATES EVERY BAND SCAN WORKS IN. `along` is the way the pen
// travels on this flow's lines; `across` is the way its bands stack. A line
// flow reads x along and y across, a column flow reads y along and x
// across, and that swap is the whole of the difference between wrapping
// around a shape in lines and wrapping around it in columns.
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

// Removes [excludedStart, excludedEnd] from every sorted, disjoint interval.
void subtractSpan(std::vector<std::pair<float, float>>& availableSpans,
                  float excludedStart, float excludedEnd) {
  std::vector<std::pair<float, float>> remainingSpans;
  remainingSpans.reserve(availableSpans.size() + 1);
  for (const auto& [spanStart, spanEnd] : availableSpans) {
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

// Occupied ALONG-intervals of a flattened polygon set within the band
// [bandStart, bandEnd] measured ACROSS: fill intervals sampled at three
// scanlines (respecting the fill rule, so holes and concave gaps stay open)
// unioned with every edge's along-travel through the band (conservative —
// catches features that fall between the samples, like a star tip). Appends
// unmerged occupied spans. `axis` names which coordinate is which, so a
// column meets a silhouette through this same scan.
void bandOccupancy(const std::vector<std::vector<glm::vec2>>& contours,
                   bool evenOdd, FlowAxis axis, float bandStart, float bandEnd,
                   std::vector<std::pair<float, float>>& occupiedSpans) {
  static thread_local std::vector<std::pair<float, int>> crossings;
  const float scanlines[3] = {bandStart + kEps, (bandStart + bandEnd) * 0.5f,
                              bandEnd - kEps};
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

}  // namespace

bool LineInterval::placeAt(float pen, float phase, int rotationSteps,
                           SkPoint* position, SkVector* tangent) const {
  if (!contour.valid()) {
    // Straight: the pen simply travels along the interval's own direction.
    // Nothing to run off the end of, so the phase is a plain shift.
    const float travel = pen + phase;
    *position =
        origin + SkVector{direction.x() * travel, direction.y() * travel};
    *tangent = quantizeTangent(direction, rotationSteps);
    return true;
  }
  const float contourLength = contour.length();
  float contourPosition = contourStart + (pen * advanceScale) + phase;
  bool inside = true;
  if (contour.closed() || wrapContour) {
    // Closed contours wrap: text can march around the loop forever
    // (shift the phase for an infinite marquee). `wrapContour` wraps a
    // contour the geometry would clamp, so the wrap is spelled here rather
    // than left to `around`.
    contourPosition = geometry::path::wrap(contourPosition, contourLength);
  } else {
    inside = contourPosition >= 0 && contourPosition <= contourLength;
    contourPosition = std::clamp(contourPosition, 0.0f, contourLength);
  }
  const std::optional<geometry::path::Contour::Sample> sample =
      contour.at(contourPosition);
  if (!sample) {
    *position = {0, 0};
    *tangent = {1, 0};
    return false;
  }
  *position = geometry::path::toSk(sample->position);
  *tangent = geometry::path::toSk(sample->tangent);
  // Walking backwards faces the other way — turned before the snap, so the
  // reversed direction lands on a ladder step rather than beside one.
  if (advanceScale < 0) *tangent = {-tangent->fX, -tangent->fY};
  // Rotation snaps; position stays exact.
  *tangent = quantizeTangent(*tangent, rotationSteps);
  return inside;
}

// Flattened-polygon form of an exclusion SkPath, cached by generation ID.
struct ExclusionFlow::FlatPath {
  std::vector<std::vector<glm::vec2>> contours;  // closed polylines
  SkRect bounds = SkRect::MakeEmpty();
  bool evenOdd = false;
};

// Private container definition: keeps the hash-map dependency out of the
// public layout/Flow.h. unique_ptr values keep FlatPath addresses stable across
// rehashes.
struct ExclusionFlow::PathCache {
  boost::unordered_flat_map<uint32_t, std::unique_ptr<FlatPath>> entries;
};

ExclusionFlow::ExclusionFlow(const SkRect& bounds, FlowAxis axis)
    : m_bounds(bounds),
      m_axis(axis),
      m_pathCache(std::make_unique<PathCache>()) {}
ExclusionFlow::~ExclusionFlow() = default;

const ExclusionFlow::FlatPath& ExclusionFlow::flattenedPathFor(
    const SkPath& path) {
  if (!m_pathCache)  // re-arm a moved-from flow instead of dereferencing null
    m_pathCache = std::make_unique<PathCache>();
  auto& cache = m_pathCache->entries;
  const uint32_t generationId = path.getGenerationID();
  auto cachedPath = cache.find(generationId);
  if (cachedPath != cache.end()) return *cachedPath->second;
  if (cache.size() > 64) cache.clear();  // Bound animated path churn.

  auto flattenedPath = std::make_unique<FlatPath>();
  const SkPathFillType fill = path.getFillType();
  flattenedPath->evenOdd = fill == SkPathFillType::kEvenOdd ||
                           fill == SkPathFillType::kInverseEvenOdd;
  flattenedPath->bounds = path.computeTightBounds();

  // Layout avoidance needs a couple of pixels of fidelity, not rendering
  // accuracy, and every contour is treated as closed: an open sub-path of
  // an exclusion is filled as if its ends were joined, exactly as the fill
  // rule fills it.
  constexpr float kFlattenTolerance = 0.5f;
  for (geometry::path::Polyline& polyline :
       geometry::path::flatten(path, kFlattenTolerance)) {
    if (polyline.points.size() >= 3)
      flattenedPath->contours.push_back(std::move(polyline.points));
  }

  auto cacheEntry = cache.emplace(generationId, std::move(flattenedPath)).first;
  return *cacheEntry->second;
}

bool BlockFlow::lineIntervals(const LineRequest& request,
                              std::vector<LineInterval>& intervals) {
  intervals.clear();
  const float lineHeight = request.lineHeight;
  const float top = m_bounds.top() + request.bandStart;
  if (top + lineHeight > m_bounds.bottom() + kEps) return false;
  LineInterval interval;
  interval.origin = {m_bounds.left(), top + request.ascent};
  interval.direction = {1, 0};
  interval.length = m_bounds.width();
  intervals.push_back(interval);
  return true;
}

bool ExclusionFlow::lineIntervals(const LineRequest& request,
                                  std::vector<LineInterval>& intervals) {
  intervals.clear();
  const float lineHeight = request.lineHeight;
  const FlowAxis axis = m_axis;
  const bool columns = axis == FlowAxis::kColumns;

  // The band this line occupies across the stack, and where its pen sits
  // inside it. Lines stack DOWN from the top and put the pen on a baseline
  // `ascent` below the band's near edge; columns advance RIGHT TO LEFT from
  // the right edge and put the pen on the band's central axis, which is
  // what a vertical-shaped glyph centres itself on.
  float bandStart = 0;
  float bandEnd = 0;
  float penAxis = 0;
  if (columns) {
    bandEnd = m_bounds.right() - request.bandStart;
    bandStart = bandEnd - lineHeight;
    if (bandStart < m_bounds.left() - kEps) return false;
    penAxis = bandEnd - lineHeight * 0.5f;
  } else {
    bandStart = m_bounds.top() + request.bandStart;
    bandEnd = bandStart + lineHeight;
    if (bandEnd > m_bounds.bottom() + kEps) return false;
    penAxis = bandStart + request.ascent;
  }

  std::vector<std::pair<float, float>> availableSpans = {
      {alongMin(axis, m_bounds), alongMax(axis, m_bounds)}};

  static thread_local std::vector<std::pair<float, float>> occupiedSpans;
  for (const Shape& shape : m_shapes) {
    if (shape.kind == Shape::kPath) {
      const FlatPath& flattenedPath = flattenedPathFor(shape.path);
      if (flattenedPath.contours.empty()) continue;
      const glm::vec2 offset = {shape.pathOffset.x(), shape.pathOffset.y()};
      const float offsetAlong = alongOf(axis, offset);
      const float offsetAcross = acrossOf(axis, offset);
      const float padding = shape.padding;
      if (acrossMax(axis, flattenedPath.bounds) + offsetAcross + padding <=
              bandStart ||
          acrossMin(axis, flattenedPath.bounds) + offsetAcross - padding >=
              bandEnd)
        continue;
      // Band and results are in path-local space (shifted by the offset);
      // padding widens the band across and each span along.
      occupiedSpans.clear();
      bandOccupancy(flattenedPath.contours, flattenedPath.evenOdd, axis,
                    bandStart - offsetAcross - padding,
                    bandEnd - offsetAcross + padding, occupiedSpans);
      mergeSpans(occupiedSpans);
      for (const auto& [spanStart, spanEnd] : occupiedSpans)
        subtractSpan(availableSpans, spanStart + offsetAlong - padding,
                     spanEnd + offsetAlong + padding);
    } else if (shape.kind == Shape::kCircle) {
      const float radius =
          std::min(shape.bounds.width(), shape.bounds.height()) * 0.5f +
          shape.padding;
      const glm::vec2 center = {shape.bounds.centerX(), shape.bounds.centerY()};
      const float centerAlong = alongOf(axis, center);
      const float centerAcross = acrossOf(axis, center);
      if (centerAcross + radius <= bandStart ||
          centerAcross - radius >= bandEnd)
        continue;
      // Widest chord of the circle within the band: at the centre when the
      // band contains it, else at the nearest band edge.
      const float distanceFromCenter =
          centerAcross < bandStart
              ? bandStart - centerAcross
              : (centerAcross > bandEnd ? centerAcross - bandEnd : 0);
      if (distanceFromCenter >= radius) continue;
      const float halfChord =
          std::sqrt(radius * radius - distanceFromCenter * distanceFromCenter);
      subtractSpan(availableSpans, centerAlong - halfChord,
                   centerAlong + halfChord);
    } else {
      const SkRect paddedBounds =
          shape.bounds.makeOutset(shape.padding, shape.padding);
      if (acrossMax(axis, paddedBounds) <= bandStart ||
          acrossMin(axis, paddedBounds) >= bandEnd)
        continue;
      subtractSpan(availableSpans, alongMin(axis, paddedBounds),
                   alongMax(axis, paddedBounds));
    }
    if (availableSpans.empty()) break;
  }

  for (const auto& [spanStart, spanEnd] : availableSpans) {
    if (spanEnd - spanStart < m_minIntervalWidth) continue;
    LineInterval interval;
    interval.origin =
        columns ? SkPoint{penAxis, spanStart} : SkPoint{spanStart, penAxis};
    interval.direction = columns ? SkVector{0, 1} : SkVector{1, 0};
    interval.length = spanEnd - spanStart;
    intervals.push_back(interval);
  }
  return true;
}

bool VerticalBlockFlow::lineIntervals(const LineRequest& request,
                                      std::vector<LineInterval>& intervals) {
  intervals.clear();
  const float lineHeight = request.lineHeight;
  const float right = m_bounds.right() - request.bandStart;
  if (right - lineHeight < m_bounds.left() - kEps) return false;
  LineInterval interval;
  interval.origin = {right - lineHeight * 0.5f, m_bounds.top()};
  interval.direction = {0, 1};
  interval.length = m_bounds.height();
  intervals.push_back(interval);
  return true;
}

bool LineSetFlow::lineIntervals(const LineRequest& request,
                                std::vector<LineInterval>& intervals) {
  intervals.clear();
  const int index = request.index;
  if (index < 0 || static_cast<size_t>(index) >= m_lines.size()) return false;
  intervals = m_lines[static_cast<size_t>(index)];
  return true;
}

PathFlow::PathFlow(const SkPath& path) { addPath(path); }

void PathFlow::addPath(const SkPath& path) {
  for (geometry::path::Contour& contour : geometry::path::Contour::of(path))
    m_contours.push_back(std::move(contour));
}

bool PathFlow::lineIntervals(const LineRequest& request,
                             std::vector<LineInterval>& intervals) {
  intervals.clear();
  const int index = request.index;
  if (index < 0 || static_cast<size_t>(index) >= m_contours.size())
    return false;
  LineInterval interval;
  interval.contour = m_contours[static_cast<size_t>(index)];
  interval.contourStart = 0;
  interval.length = interval.contour.length();
  intervals.push_back(interval);
  return true;
}

}  // namespace sigil::weave
