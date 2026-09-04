/** @file
 * Reusable geometry for brush strokes and polygonal effects.
 */

#include <sigildraw/Math.h>
#include <sigildraw/kit/Engine.h>
#include <sigildraw/kit/Geometry.h>

#include <algorithm>
#include <cmath>

namespace sigil::draw::brush {

namespace {

float cross(SkPoint a, SkPoint b) { return a.fX * b.fY - a.fY * b.fX; }

SkPoint subtract(SkPoint a, SkPoint b) { return {a.fX - b.fX, a.fY - b.fY}; }

SkPoint add(SkPoint a, SkPoint b) { return {a.fX + b.fX, a.fY + b.fY}; }

SkPoint multiply(SkPoint point, float scale) {
  return {point.fX * scale, point.fY * scale};
}

}  // namespace

Polygon::Polygon(std::vector<SkPoint> points) : vertices(std::move(points)) {
  rebuildSides();
}

void Polygon::rebuildSides() {
  sides.clear();
  if (vertices.size() < 2) return;
  sides.reserve(vertices.size());
  for (size_t i = 0; i < vertices.size(); ++i)
    sides.push_back({vertices[i], vertices[(i + 1) % vertices.size()]});
}

std::vector<SkPoint> Polygon::intersect(const Line& line) const {
  std::vector<SkPoint> result;
  const SkPoint ray = subtract(line.point2, line.point1);
  for (const Line& side : sides) {
    const SkPoint edge = subtract(side.point2, side.point1);
    const float denominator = cross(ray, edge);
    if (std::abs(denominator) < 0.000001f) continue;
    const SkPoint between = subtract(side.point1, line.point1);
    const float alongRay = cross(between, edge) / denominator;
    const float alongEdge = cross(between, ray) / denominator;
    if (alongRay >= 0.0f && alongRay <= 1.0f && alongEdge >= 0.0f &&
        alongEdge <= 1.0f)
      result.push_back(add(line.point1, multiply(ray, alongRay)));
  }
  std::ranges::sort(result, [&](SkPoint a, SkPoint b) {
    return std::hypot(a.fX - line.point1.fX, a.fY - line.point1.fY) <
           std::hypot(b.fX - line.point1.fX, b.fY - line.point1.fY);
  });
  return result;
}

void Polygon::draw(Pen& pen, const Brush& brush) const {
  if (empty()) return;
  Stroke outline;
  outline.reserve(vertices.size() + 1);
  for (SkPoint point : vertices) outline.push_back({point, 1.0f});
  outline.push_back(outline.front());
  paint(pen, brush, outline);
}

void Polygon::fill(Pen& pen, const Wash& style) const { wash(pen, style); }

void Polygon::wash(Pen& pen, const Wash& style) const {
  brush::wash(pen, style, vertices);
}

void Polygon::hatch(Pen& pen, const Brush& tool, const Hatch& style) const {
  brush::hatch(pen, tool, vertices, style);
}

void Polygon::mass(Pen& pen, const Brush& tool, const Mass& style) const {
  brush::mass(pen, tool, vertices, style);
}

const Polygon& Polygon::draw(Pen& pen, const Engine& engine) const {
  engine.draw(pen, *this);
  return *this;
}

const Polygon& Polygon::fill(Pen& pen, const Engine& engine) const {
  engine.fill(pen, *this);
  return *this;
}

const Polygon& Polygon::wash(Pen& pen, const Engine& engine) const {
  engine.wash(pen, *this);
  return *this;
}

const Polygon& Polygon::hatch(Pen& pen, const Engine& engine) const {
  engine.hatch(pen, *this);
  return *this;
}

const Polygon& Polygon::mass(Pen& pen, const Engine& engine) const {
  engine.mass(pen, *this);
  return *this;
}

const Polygon& Polygon::show(Pen& pen, const Engine& engine) const {
  engine.wash(pen, *this);
  engine.fill(pen, *this);
  engine.mass(pen, *this);
  engine.hatch(pen, *this);
  engine.draw(pen, *this);
  return *this;
}

Polygon Polygon::translated(float x, float y) const {
  std::vector<SkPoint> moved = vertices;
  for (SkPoint& point : moved) {
    point.fX += x;
    point.fY += y;
  }
  return Polygon(std::move(moved));
}

Plot::Plot(PlotType type, Constant units)
    : type(type), m_angleMode(units == DEGREES ? DEGREES : RADIANS) {}

float Plot::normalized(float angle) const {
  return m_angleMode == DEGREES ? radians(angle) : angle;
}

void Plot::addSegment(float angle, float length, float pressure) {
  angles.push_back(normalized(angle));
  segments.push_back(std::max(0.0f, length));
  press.push_back(std::max(0.0f, pressure));
}

void Plot::endPlot(float angle, float pressure) {
  m_endAngle = normalized(angle);
  m_endPressure = std::max(0.0f, pressure);
}

void Plot::rotate(float angle) { rotate(angle, m_angleMode); }

void Plot::rotate(float angle, Constant units) {
  if (units == DEGREES) angle = radians(angle);
  m_rotation = angle;
}

float Plot::length() const {
  float result = 0.0f;
  for (float segment : segments) result += segment;
  return result;
}

float Plot::angle(float distance) const {
  if (angles.empty()) return m_endAngle + m_rotation;
  distance = std::max(0.0f, distance);
  float travelled = 0.0f;
  for (size_t i = 0; i < segments.size(); ++i) {
    const float next = travelled + segments[i];
    if (distance <= next || i + 1 == segments.size()) {
      if (type == PlotType::Segments || !(segments[i] > 0.0f))
        return angles[std::min(i, angles.size() - 1)] + m_rotation;
      const float from = angles[std::min(i, angles.size() - 1)];
      const float to = i + 1 < angles.size() ? angles[i + 1] : m_endAngle;
      const float delta = std::remainder(to - from, TWO_PI);
      return from +
             delta *
                 std::clamp((distance - travelled) / segments[i], 0.0f, 1.0f) +
             m_rotation;
    }
    travelled = next;
  }
  return m_endAngle + m_rotation;
}

float Plot::pressure(float distance) const {
  if (press.empty()) return m_endPressure;
  distance = std::max(0.0f, distance);
  float travelled = 0.0f;
  for (size_t i = 0; i < segments.size(); ++i) {
    const float next = travelled + segments[i];
    if (distance <= next || i + 1 == segments.size()) {
      const float from = press[std::min(i, press.size() - 1)];
      const float to = i + 1 < press.size() ? press[i + 1] : m_endPressure;
      if (!(segments[i] > 0.0f)) return to;
      return from +
             (to - from) *
                 std::clamp((distance - travelled) / segments[i], 0.0f, 1.0f);
    }
    travelled = next;
  }
  return m_endPressure;
}

Stroke Plot::path(SkPoint origin, float spacing, float curvature,
                  float scale) const {
  if (this->origin) {
    origin = *this->origin;
    scale = 1.0f;
  }
  scale = std::abs(scale);
  Stroke controls{{origin, press.empty() ? 1.0f : press.front()}};
  SkPoint position = origin;
  for (size_t i = 0; i < segments.size(); ++i) {
    const float direction =
        (i < angles.size() ? angles[i] : m_endAngle) + m_rotation;
    position = {position.fX + std::cos(direction) * segments[i] * scale,
                position.fY - std::sin(direction) * segments[i] * scale};
    const float pressure = i + 1 < press.size() ? press[i + 1] : m_endPressure;
    controls.push_back({position, pressure});
  }
  if (type == PlotType::Segments) return controls;
  return spline(controls, spacing, curvature);
}

Polygon Plot::genPol(float x, float y, float spacing, float curvature,
                     float scale) const {
  const Stroke samples = path({x, y}, spacing, curvature, scale);
  std::vector<SkPoint> points;
  points.reserve(samples.size());
  for (const Sample& sample : samples) points.push_back(sample.position);
  Polygon result(std::move(points));
  pol = result;
  return result;
}

void Plot::draw(Pen& pen, const Brush& tool, float x, float y,
                float scale) const {
  paint(pen, tool, path({x, y}, tool.spacing, 0.5f, scale));
}

void Plot::fill(Pen& pen, const Wash& style, float x, float y,
                float scale) const {
  genPol(x, y, 1.0f, 0.5f, scale).fill(pen, style);
}

void Plot::wash(Pen& pen, const Wash& style, float x, float y,
                float scale) const {
  genPol(x, y, 1.0f, 0.5f, scale).wash(pen, style);
}

void Plot::hatch(Pen& pen, const Brush& tool, const Hatch& style, float x,
                 float y, float scale) const {
  genPol(x, y, tool.spacing, 0.5f, scale).hatch(pen, tool, style);
}

void Plot::mass(Pen& pen, const Brush& tool, const Mass& style, float x,
                float y, float scale) const {
  genPol(x, y, tool.spacing, 0.5f, scale).mass(pen, tool, style);
}

const Plot& Plot::draw(Pen& pen, const Engine& engine, float x, float y,
                       float scale) const {
  engine.draw(pen, *this, x, y, scale);
  return *this;
}

const Plot& Plot::fill(Pen& pen, const Engine& engine, float x, float y,
                       float scale) const {
  engine.fill(pen, *this, x, y, scale);
  return *this;
}

const Plot& Plot::wash(Pen& pen, const Engine& engine, float x, float y,
                       float scale) const {
  engine.wash(pen, *this, x, y, scale);
  return *this;
}

const Plot& Plot::hatch(Pen& pen, const Engine& engine, float x, float y,
                        float scale) const {
  engine.hatch(pen, *this, x, y, scale);
  return *this;
}

const Plot& Plot::mass(Pen& pen, const Engine& engine, float x, float y,
                       float scale) const {
  engine.mass(pen, *this, x, y, scale);
  return *this;
}

const Plot& Plot::show(Pen& pen, const Engine& engine, float x, float y,
                       float scale) const {
  engine.wash(pen, *this, x, y, scale);
  engine.fill(pen, *this, x, y, scale);
  engine.mass(pen, *this, x, y, scale);
  engine.hatch(pen, *this, x, y, scale);
  engine.draw(pen, *this, x, y, scale);
  return *this;
}

Plot Plot::fromStroke(std::span<const Sample> stroke, PlotType type) {
  Plot result(type);
  if (stroke.size() < 2) return result;
  result.origin = stroke.front().position;
  float lastAngle = 0.0f;
  for (size_t i = 1; i < stroke.size(); ++i) {
    const SkPoint from = stroke[i - 1].position;
    const SkPoint to = stroke[i].position;
    const float dx = to.fX - from.fX;
    const float dy = to.fY - from.fY;
    const float length = std::hypot(dx, dy);
    lastAngle = length > 0.0f ? std::atan2(-dy, dx) : lastAngle;
    result.addSegment(lastAngle, length, stroke[i - 1].pressure);
  }
  result.endPlot(lastAngle, stroke.back().pressure);
  return result;
}

Position::Position(float x, float y, Field field, float seconds, Constant units,
                   std::optional<SkRect> bounds)
    : x(x),
      y(y),
      m_field(std::move(field)),
      m_seconds(seconds),
      m_angleMode(units == DEGREES ? DEGREES : RADIANS),
      m_bounds(bounds ? std::optional(bounds->makeSorted()) : std::nullopt) {}

Stroke Position::moveTo(float direction, float length, float stepLength,
                        const Field& field, float seconds) {
  Stroke result{{{x, y}, 1.0f}};
  if (!(length > 0.0f)) return result;
  if (m_angleMode == DEGREES) direction = radians(direction);
  stepLength = std::max(0.125f, stepLength);
  if (!isIn()) {
    plotted += stepLength;
    return result;
  }
  float travelled = 0.0f;
  while (travelled < length) {
    const float step = std::min(stepLength, length - travelled);
    const Field& active = field ? field : m_field;
    const float activeSeconds = field ? seconds : m_seconds;
    const float heading =
        active ? active({x, y}, activeSeconds) - direction : -direction;
    update(x + std::cos(heading) * step, y + std::sin(heading) * step);
    plotted += step;
    travelled += step;
    result.push_back({{x, y}, 1.0f});
  }
  return result;
}

Stroke Position::plotTo(const Plot& plot, float length, float stepLength,
                        float scale) {
  Stroke result{{{x, y}, plot.pressure(plotted)}};
  if (!(length > 0.0f) || !plot) return result;
  stepLength = std::max(0.125f, stepLength);
  scale = std::max(0.0001f, std::abs(scale));
  if (!isIn()) {
    plotted += stepLength / scale;
    return result;
  }
  float travelled = 0.0f;
  while (travelled < length) {
    const float step = std::min(stepLength, length - travelled);
    const float heading = angle() - plot.angle(plotted);
    update(x + std::cos(heading) * step, y + std::sin(heading) * step);
    plotted += step / scale;
    travelled += step;
    result.push_back({{x, y}, plot.pressure(plotted)});
  }
  return result;
}

float Position::angle(const Field& field, float seconds) const {
  return field ? field({x, y}, seconds) : 0.0f;
}

float Position::angle() const {
  return m_field ? m_field({x, y}, m_seconds) : 0.0f;
}

bool Position::isIn() const { return isInCanvas(); }

bool Position::isInCanvas() const {
  if (!m_bounds) return std::isfinite(x) && std::isfinite(y);
  SkRect region = *m_bounds;
  region.outset(region.width() * 0.5f, region.height() * 0.5f);
  return region.contains(x, y);
}

void Position::update(float nextX, float nextY) {
  x = nextX;
  y = nextY;
}

void Position::reset() { plotted = 0.0f; }

void Position::field(Field value, float seconds) {
  m_field = std::move(value);
  m_seconds = seconds;
}

void hatchArray(Pen& pen, const Brush& tool, std::span<const Polygon> polygons,
                const Hatch& style) {
  if (polygons.empty() || polygons.front().empty()) return;
  std::vector<std::span<const SkPoint>> contours;
  contours.reserve(polygons.size());
  for (const Polygon& polygon : polygons) contours.push_back(polygon.vertices);
  brush::hatch(pen, tool, contours, style);
}

void massArray(Pen& pen, const Brush& tool, std::span<const Polygon> polygons,
               const Mass& style) {
  if (polygons.empty() || polygons.front().empty()) return;
  std::vector<std::span<const SkPoint>> contours;
  contours.reserve(polygons.size());
  for (const Polygon& polygon : polygons) contours.push_back(polygon.vertices);
  brush::mass(pen, tool, contours, style);
}

}  // namespace sigil::draw::brush
