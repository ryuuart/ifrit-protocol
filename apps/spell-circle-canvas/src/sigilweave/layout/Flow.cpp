/** @file
 * The ready-made flow geometries: the block, the rectangle minus its
 * exclusions in lines or in columns, the vertical block, the explicit line
 * set, one line per path contour, and the placement of a pen coordinate on
 * a contour interval with its tangent snapped. The silhouettes an
 * exclusion subtracts are beside this, in Silhouette.cpp.
 */

#include "sigilweave/layout/Flow.h"

#include <algorithm>
#include <cmath>
#include <glm/vec2.hpp>
#include <memory>
#include <numbers>
#include <utility>
#include <vector>

#include "BandScan.h"
#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::weave {

using detail::acrossMax;
using detail::acrossMin;
using detail::acrossOf;
using detail::alongMax;
using detail::alongMin;
using detail::alongOf;
using detail::kBandEpsilon;

namespace {

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

ExclusionFlow::ExclusionFlow(const SkRect& bounds, FlowAxis axis)
    : m_bounds(bounds), m_axis(axis) {}
ExclusionFlow::~ExclusionFlow() = default;

bool BlockFlow::lineIntervals(const LineRequest& request,
                              std::vector<LineInterval>& intervals) {
  intervals.clear();
  const float lineHeight = request.lineHeight;
  const float top = m_bounds.top() + request.bandStart;
  if (top + lineHeight > m_bounds.bottom() + kBandEpsilon) return false;
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
    if (bandStart < m_bounds.left() - kBandEpsilon) return false;
    penAxis = bandEnd - lineHeight * 0.5f;
  } else {
    bandStart = m_bounds.top() + request.bandStart;
    bandEnd = bandStart + lineHeight;
    if (bandEnd > m_bounds.bottom() + kBandEpsilon) return false;
    penAxis = bandStart + request.ascent;
  }

  std::vector<std::pair<float, float>> availableSpans = {
      {alongMin(axis, m_bounds), alongMax(axis, m_bounds)}};

  static thread_local std::vector<Span> occupiedSpans;
  for (const Exclusion& exclusion : m_exclusions) {
    if (!exclusion.shape) continue;
    // Every silhouette answers in ITS OWN SPACE, so rigid motion is two
    // subtractions and a shift and never touches whatever the shape
    // cached: the band arrives moved back by the offset and the spans come
    // out moved forward by it.
    const float offsetAlong =
        alongOf(axis, {exclusion.offset.x(), exclusion.offset.y()});
    const float offsetAcross =
        acrossOf(axis, {exclusion.offset.x(), exclusion.offset.y()});
    const SkRect shapeBounds = exclusion.shape->bounds();
    const float margin = std::max(exclusion.margin, 0.0f);
    if (acrossMax(axis, shapeBounds) + offsetAcross + margin <= bandStart ||
        acrossMin(axis, shapeBounds) + offsetAcross - margin >= bandEnd)
      continue;
    occupiedSpans.clear();
    exclusion.shape->bandSpans(
        axis, Band{bandStart - offsetAcross, bandEnd - offsetAcross}, margin,
        occupiedSpans);
    for (const Span& span : occupiedSpans)
      subtractSpan(availableSpans, span.start + offsetAlong,
                   span.end + offsetAlong);
    if (availableSpans.empty()) break;
  }

  for (const auto& [spanStart, spanEnd] : availableSpans) {
    if (spanEnd - spanStart < m_minimumIntervalWidth) continue;
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
  if (right - lineHeight < m_bounds.left() - kBandEpsilon) return false;
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
