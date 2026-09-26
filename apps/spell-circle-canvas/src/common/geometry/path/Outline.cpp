/** @file
 * The outline value over the Skia path that executes it.
 */

#include "sigilgeometry/path/Outline.h"

#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/utils/SkParsePath.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include "OutlineInternal.h"
#include "sigilgeometry/path/Operations.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Pose.h"
#include "sigilgeometry/path/Scatter.h"
#include "sigilgeometry/path/Segments.h"
#include "sigilgeometry/path/Skia.h"
#include "sigilgeometry/path/Transform.h"

namespace sigil::geometry::path {

namespace {
const std::shared_ptr<const detail::OutlineBody>& emptyBody() {
  static const auto body = std::make_shared<const detail::OutlineBody>();
  return body;
}
}  // namespace

Outline::Outline() : m_body(emptyBody()) {}

Outline::Outline(std::shared_ptr<const detail::OutlineBody> body)
    : m_body(body ? std::move(body) : emptyBody()) {}

Outline Outline::svg(std::string_view data) {
  const std::string text(data);
  if (auto parsed = SkParsePath::FromSVGString(text.c_str()))
    return fromSk(std::move(*parsed));
  return {};
}

Outline Outline::rectangle(const Rect& rect) {
  return fromSk(SkPath::Rect(toSk(rect)));
}

bool Outline::empty() const { return m_body->path.isEmpty(); }

FillRule Outline::fillRule() const {
  return OutlineAccess::ruleOf(m_body->path.getFillType());
}

Outline Outline::withFillRule(FillRule rule) const {
  SkPath path = m_body->path;
  path.setFillType(rule == FillRule::EvenOdd ? SkPathFillType::kEvenOdd
                                             : SkPathFillType::kWinding);
  return fromSk(std::move(path));
}

Rect Outline::bounds() const {
  if (m_body->path.isEmpty()) return {};
  return fromSk(m_body->path.computeTightBounds());
}

float Outline::length() const {
  const detail::OutlineBody& body = *m_body;
  body.contours();
  return body.measuredLength;
}

Pose Outline::poseAt(float distance, Wrap wrap) const {
  return poseAlong(m_body->contours(), distance, wrap);
}

glm::vec2 Outline::pointAt(float distance, Wrap wrap) const {
  return poseAt(distance, wrap).position;
}

glm::vec2 Outline::tangentAt(float distance, Wrap wrap) const {
  return poseAt(distance, wrap).tangent;
}

glm::vec2 Outline::normalAt(float distance, Wrap wrap) const {
  return poseAt(distance, wrap).normal;
}

Outline Outline::segment(float from, float to) const {
  SkPathBuilder out;
  if (to <= from) return {};
  float start = 0;
  for (const Contour& contour : m_body->contours()) {
    const float length = contour.length();
    const float lower = std::max(from, start), upper = std::min(to, start + length);
    if (upper > lower) contour.appendSegment(out, lower - start, upper - start);
    start += length;
  }
  return fromSk(out.detach());
}

std::pair<Outline, Outline> Outline::split(float distance) const {
  const float total = length();
  const float at = std::clamp(distance, 0.0f, total);
  return {segment(0, at), segment(at, total)};
}

Nearest Outline::nearest(glm::vec2 point) const {
  Nearest best{.gap = std::numeric_limits<float>::infinity()};
  float start = 0;
  for (const Contour& contour : m_body->contours()) {
    Nearest candidate = contour.nearest(point);
    if (candidate.gap < best.gap) {
      candidate.distance += start;
      best = candidate;
    }
    start += contour.length();
  }
  if (!std::isfinite(best.gap)) return {};
  return best;
}

std::vector<Polyline> Outline::resampled(ResampleOptions options) const {
  std::vector<Polyline> lines = flatten(m_body->path, options.tolerance);
  if (options.count > 0) {
    for (Polyline& line : lines) {
      const Sampled even = resample(line, options.count);
      line = Polyline{.points = even.points, .closed = even.closed};
    }
  } else if (options.spacing > 0) {
    for (Polyline& line : lines) line = subdivide(line, options.spacing);
  }
  return lines;
}

bool Outline::contains(glm::vec2 point) const {
  return m_body->path.contains(point.x, point.y);
}

float Outline::area() const {
  // Resolved into contours that do not overlap, the even-odd reading of
  // the rings is the area the fill rule encloses, whichever rule it is.
  return Region::of(operations::simplify(m_body->path)).area();
}

Winding Outline::winding() const {
  for (const Polyline& ring : flatten(m_body->path)) {
    if (!ring.closed || ring.points.size() < 3) continue;
    return ring.signedArea() >= 0 ? Winding::OutersClockwise
                                  : Winding::OutersCounterClockwise;
  }
  return Winding::OutersClockwise;
}

Outline Outline::united(const Outline& other) const {
  return fromSk(operations::unite(m_body->path, other.m_body->path));
}

Outline Outline::subtracted(const Outline& other) const {
  return fromSk(operations::subtract(m_body->path, other.m_body->path));
}

Outline Outline::intersected(const Outline& other) const {
  return fromSk(operations::intersect(m_body->path, other.m_body->path));
}

Outline Outline::excluded(const Outline& other) const {
  return fromSk(operations::exclude(m_body->path, other.m_body->path));
}

Outline Outline::simplified() const {
  return fromSk(operations::simplify(m_body->path));
}

Outline Outline::reversed() const { return fromSk(reverse(m_body->path)); }

Outline Outline::joined(const Outline& other) const {
  SkPathBuilder out(m_body->path);
  out.addPath(other.m_body->path);
  return fromSk(out.detach());
}

Outline Outline::transformed(const Transform& transform) const {
  return fromSk(m_body->path.makeTransform(toSk(transform)));
}

bool Outline::operator==(const Outline& other) const {
  return m_body == other.m_body || m_body->path == other.m_body->path;
}

SkPath toSk(const Outline& outline) { return OutlineAccess::path(outline); }

Outline fromSk(SkPath path) { return OutlineAccess::make(std::move(path)); }

}  // namespace sigil::geometry::path
