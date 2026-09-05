/** @file
 * The relative plot: turns and lengths replayed anywhere.
 */

#include <sigildraw/Math.h>
#include <sigildraw/brush/Deposit.h>
#include <sigildraw/brush/Engine.h>
#include <sigildraw/brush/Hatch.h>
#include <sigildraw/brush/Mass.h>
#include <sigildraw/brush/Plot.h>
#include <sigildraw/brush/Wash.h>

#include <algorithm>
#include <cmath>

namespace sigil::draw::brush {

void Plot::addSegment(float angle, float length, float pressure) {
  m_angles.push_back(angle);
  m_segments.push_back(std::max(0.0f, length));
  m_pressures.push_back(std::max(0.0f, pressure));
}

void Plot::endPlot(float angle, float pressure) {
  m_endAngle = angle;
  m_endPressure = std::max(0.0f, pressure);
}

float Plot::length() const {
  float result = 0.0f;
  for (float segment : m_segments) result += segment;
  return result;
}

float Plot::angle(float distance) const {
  if (m_angles.empty()) return m_endAngle + m_rotation;
  distance = std::max(0.0f, distance);
  float travelled = 0.0f;
  for (size_t i = 0; i < m_segments.size(); ++i) {
    const float next = travelled + m_segments[i];
    if (distance <= next || i + 1 == m_segments.size()) {
      if (m_type == PlotType::Segments || !(m_segments[i] > 0.0f))
        return m_angles[std::min(i, m_angles.size() - 1)] + m_rotation;
      const float from = m_angles[std::min(i, m_angles.size() - 1)];
      const float to = i + 1 < m_angles.size() ? m_angles[i + 1] : m_endAngle;
      const float delta = std::remainder(to - from, TWO_PI);
      return from +
             delta * std::clamp((distance - travelled) / m_segments[i], 0.0f,
                                1.0f) +
             m_rotation;
    }
    travelled = next;
  }
  return m_endAngle + m_rotation;
}

float Plot::pressure(float distance) const {
  if (m_pressures.empty()) return m_endPressure;
  distance = std::max(0.0f, distance);
  float travelled = 0.0f;
  for (size_t i = 0; i < m_segments.size(); ++i) {
    const float next = travelled + m_segments[i];
    if (distance <= next || i + 1 == m_segments.size()) {
      const float from = m_pressures[std::min(i, m_pressures.size() - 1)];
      const float to =
          i + 1 < m_pressures.size() ? m_pressures[i + 1] : m_endPressure;
      if (!(m_segments[i] > 0.0f)) return to;
      return lerp(from, to,
                  std::clamp((distance - travelled) / m_segments[i], 0.0f,
                             1.0f));
    }
    travelled = next;
  }
  return m_endPressure;
}

Stroke Plot::path(SkPoint origin, float spacing, float curvature,
                  float scale) const {
  scale = std::abs(scale);
  Stroke controls{
      {origin, m_pressures.empty() ? 1.0f : m_pressures.front()}};
  SkPoint position = origin;
  for (size_t i = 0; i < m_segments.size(); ++i) {
    const float direction =
        (i < m_angles.size() ? m_angles[i] : m_endAngle) + m_rotation;
    position = {position.fX + std::cos(direction) * m_segments[i] * scale,
                position.fY + std::sin(direction) * m_segments[i] * scale};
    const float pressure =
        i + 1 < m_pressures.size() ? m_pressures[i + 1] : m_endPressure;
    controls.push_back({position, pressure});
  }
  if (m_type == PlotType::Segments) return controls;
  return spline(controls, spacing, curvature);
}

Polygon Plot::polygon(float x, float y, float spacing, float curvature,
                      float scale) const {
  const Stroke samples = path({x, y}, spacing, curvature, scale);
  std::vector<SkPoint> points;
  points.reserve(samples.size());
  for (const Sample& sample : samples) points.push_back(sample.position);
  return Polygon(std::move(points));
}

void Plot::draw(Pen& pen, const Tool& tool, float x, float y,
                float scale) const {
  paint(pen, tool, path({x, y}, tool.spacing, 0.5f, scale));
}

void Plot::fill(Pen& pen, const Wash& style, float x, float y,
                float scale) const {
  polygon(x, y, 1.0f, 0.5f, scale).fill(pen, style);
}

void Plot::wash(Pen& pen, const Wash& style, float x, float y,
                float scale) const {
  polygon(x, y, 1.0f, 0.5f, scale).wash(pen, style);
}

void Plot::hatch(Pen& pen, const Tool& tool, const Hatch& style, float x,
                 float y, float scale) const {
  polygon(x, y, tool.spacing, 0.5f, scale).hatch(pen, tool, style);
}

void Plot::mass(Pen& pen, const Tool& tool, const Mass& style, float x,
                float y, float scale) const {
  polygon(x, y, tool.spacing, 0.5f, scale).mass(pen, tool, style);
}

void Plot::draw(Pen& pen, const Engine& engine, float x, float y,
                float scale) const {
  engine.draw(pen, *this, x, y, scale);
}

void Plot::fill(Pen& pen, const Engine& engine, float x, float y,
                float scale) const {
  engine.fill(pen, *this, x, y, scale);
}

void Plot::wash(Pen& pen, const Engine& engine, float x, float y,
                float scale) const {
  engine.wash(pen, *this, x, y, scale);
}

void Plot::hatch(Pen& pen, const Engine& engine, float x, float y,
                 float scale) const {
  engine.hatch(pen, *this, x, y, scale);
}

void Plot::mass(Pen& pen, const Engine& engine, float x, float y,
                float scale) const {
  engine.mass(pen, *this, x, y, scale);
}

void Plot::show(Pen& pen, const Engine& engine, float x, float y,
                float scale) const {
  engine.wash(pen, *this, x, y, scale);
  engine.fill(pen, *this, x, y, scale);
  engine.mass(pen, *this, x, y, scale);
  engine.hatch(pen, *this, x, y, scale);
  engine.draw(pen, *this, x, y, scale);
}

Plot Plot::fromStroke(std::span<const Sample> stroke, PlotType type) {
  Plot result(type);
  if (stroke.size() < 2) return result;
  float lastAngle = 0.0f;
  for (size_t i = 1; i < stroke.size(); ++i) {
    const SkPoint from = stroke[i - 1].position;
    const SkPoint to = stroke[i].position;
    const float dx = to.fX - from.fX;
    const float dy = to.fY - from.fY;
    const float length = std::hypot(dx, dy);
    lastAngle = length > 0.0f ? std::atan2(dy, dx) : lastAngle;
    result.addSegment(lastAngle, length, stroke[i - 1].pressure);
  }
  result.endPlot(lastAngle, stroke.back().pressure);
  return result;
}

}  // namespace sigil::draw::brush
