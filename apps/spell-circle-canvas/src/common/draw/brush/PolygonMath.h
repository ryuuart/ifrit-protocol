#pragma once

/** @file
 * The polygon arithmetic the interiors share: a path, bounds, a centre,
 * the closed outline, and the even-odd point test. Private to the
 * library.
 */

#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <sigildraw/brush/Stroke.h>

#include <algorithm>
#include <limits>
#include <span>

namespace sigil::draw::brush {

inline SkPath polygonPath(std::span<const SkPoint> polygon) {
  SkPathBuilder path;
  if (polygon.empty()) return path.detach();
  path.moveTo(polygon.front());
  for (size_t i = 1; i < polygon.size(); ++i) path.lineTo(polygon[i]);
  path.close();
  return path.detach();
}

/** The bounds of every point in every contour; empty contours give an
 *  empty rect. */
inline SkRect contourBounds(std::span<const std::span<const SkPoint>> contours) {
  float left = std::numeric_limits<float>::infinity();
  float top = std::numeric_limits<float>::infinity();
  float right = -std::numeric_limits<float>::infinity();
  float bottom = -std::numeric_limits<float>::infinity();
  for (const std::span<const SkPoint> contour : contours) {
    for (const SkPoint point : contour) {
      left = std::min(left, point.fX);
      top = std::min(top, point.fY);
      right = std::max(right, point.fX);
      bottom = std::max(bottom, point.fY);
    }
  }
  if (!(left <= right) || !(top <= bottom)) return SkRect::MakeEmpty();
  return SkRect::MakeLTRB(left, top, right, bottom);
}

inline SkRect polygonBounds(std::span<const SkPoint> polygon) {
  const std::span<const SkPoint> contours[1] = {polygon};
  return contourBounds(contours);
}

/** The plain average of the vertices. */
inline SkPoint polygonCenter(std::span<const SkPoint> polygon) {
  SkPoint center{0, 0};
  if (polygon.empty()) return center;
  for (SkPoint point : polygon) {
    center.fX += point.fX;
    center.fY += point.fY;
  }
  const float count = (float)polygon.size();
  return {center.fX / count, center.fY / count};
}

/** The polygon as a closed centreline at one pressure: its vertices, then
 *  its first vertex again. */
inline Stroke closedOutline(std::span<const SkPoint> polygon,
                            float pressure = 1.0f) {
  Stroke outline;
  if (polygon.empty()) return outline;
  outline.reserve(polygon.size() + 1);
  for (SkPoint point : polygon) outline.push_back({point, pressure});
  outline.push_back(outline.front());
  return outline;
}

/** The ray test: whether @p point is inside one ring. */
inline bool pointInRing(std::span<const SkPoint> ring, SkPoint point) {
  if (ring.size() < 3) return false;
  bool inside = false;
  for (size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
    const SkPoint a = ring[i];
    const SkPoint b = ring[j];
    if (((a.fY > point.fY) != (b.fY > point.fY)) &&
        point.fX < (b.fX - a.fX) * (point.fY - a.fY) / (b.fY - a.fY) + a.fX)
      inside = !inside;
  }
  return inside;
}

/** Whether @p point is inside the even-odd union of the contours. */
inline bool pointInContours(std::span<const std::span<const SkPoint>> contours,
                            SkPoint point) {
  bool inside = false;
  for (const std::span<const SkPoint> contour : contours)
    if (contour.size() >= 3 && pointInRing(contour, point)) inside = !inside;
  return inside;
}

}  // namespace sigil::draw::brush
