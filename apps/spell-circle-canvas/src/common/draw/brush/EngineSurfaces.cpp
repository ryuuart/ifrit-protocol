/** @file
 * The engine's surfaces: polygons and the primitives that become one, the
 * interiors over stored geometry, and the cursor.
 */

#include "PenUnits.h"
#include "PolygonMath.h"

#include <sigildraw/Pen.h>
#include <sigildraw/brush/Engine.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace sigil::draw::brush {

namespace {

std::vector<SkPoint> circlePoints(float x, float y, float radius, float start,
                                  float stop, int count) {
  std::vector<SkPoint> result;
  result.reserve((size_t)count + 1);
  for (int i = 0; i <= count; ++i) {
    const float t = (float)i / (float)count;
    const float angle = start + (stop - start) * t;
    result.push_back(
        {x + std::cos(angle) * radius, y + std::sin(angle) * radius});
  }
  return result;
}

std::vector<SkPoint> positions(std::span<const Sample> samples) {
  std::vector<SkPoint> result;
  result.reserve(samples.size());
  for (const Sample& sample : samples) result.push_back(sample.position);
  return result;
}

}  // namespace

// ---- one boundary, every interior --------------------------------------------

void Engine::paintWash(Pen& pen, std::span<const SkPoint> points) const {
  SkColor4f color = m_washColor;
  color.fA *= m_washOpacity;
  pen.noStroke();
  pen.fill(color);
  pen.beginShape();
  for (SkPoint point : points) pen.vertex(point.fX, point.fY);
  pen.endShape(CLOSE);
}

void Engine::paintBoundary(Pen& pen, std::span<const Sample> corners,
                           bool applyField, bool paintOutline) const {
  if (corners.size() < 3) return;
  // One shaped boundary for the interiors and the outline, so a field
  // bends both the same way.
  const Stroke boundary = shapedBoundary(pen, corners, applyField);
  if (boundary.size() < 4) return;
  const std::vector<SkPoint> interior =
      positions(std::span(boundary).first(boundary.size() - 1));

  pen.push();
  applyClip(pen);
  if (m_washActive) paintWash(pen, interior);
  if (m_fillActive) brush::wash(pen, m_fill, interior);
  if (m_mass) brush::mass(pen, m_massTool, interior, *m_mass);
  if (m_hatch) brush::hatch(pen, hatchTool(), interior, *m_hatch);
  if (paintOutline && hasStroke()) paintStroke(pen, tool(), boundary, false);
  pen.pop();
}

void Engine::paintPolygon(Pen& pen, std::span<const SkPoint> points,
                          bool applyField) const {
  paintBoundary(pen, closedOutline(points), applyField, true);
}

// ---- polygons ---------------------------------------------------------------

Polygon Engine::polygon(Pen& pen, std::span<const SkPoint> points) const {
  Polygon result(std::vector<SkPoint>(points.begin(), points.end()));
  paintPolygon(pen, result.vertices, false);
  return result;
}

void Engine::polygon(Pen& pen, const Polygon& stored) const {
  paintPolygon(pen, stored.vertices, false);
}

void Engine::hatchArray(Pen& pen, std::span<const Polygon> polygons) const {
  if (!m_hatch) return;
  pen.push();
  applyClip(pen);
  brush::hatchArray(pen, hatchTool(), polygons, *m_hatch);
  pen.pop();
}

void Engine::hatchArray(Pen& pen, const Polygon& polygon) const {
  hatchArray(pen, std::span<const Polygon>(&polygon, 1));
}

void Engine::massArray(Pen& pen, std::span<const Polygon> polygons) const {
  if (!m_mass) return;
  pen.push();
  applyClip(pen);
  brush::massArray(pen, m_massTool, polygons, *m_mass);
  pen.pop();
}

void Engine::massArray(Pen& pen, const Polygon& polygon) const {
  massArray(pen, std::span<const Polygon>(&polygon, 1));
}

// ---- primitives -------------------------------------------------------------

void Engine::rect(Pen& pen, float x, float y, float width, float height,
                  Constant mode) const {
  if (mode == CENTER) {
    x -= width * 0.5f;
    y -= height * 0.5f;
  } else if (mode == CORNERS) {
    width -= x;
    height -= y;
  }
  const std::array<SkPoint, 4> points{
      {{x, y}, {x + width, y}, {x + width, y + height}, {x, y + height}}};
  paintPolygon(pen, points, true);
}

void Engine::rect(Pen& pen, float x, float y, float width, float height,
                  float radius) const {
  radius = std::clamp(radius, 0.0f,
                      std::min(std::abs(width), std::abs(height)) * 0.5f);
  if (!(radius > 0.0f)) {
    rect(pen, x, y, width, height);
    return;
  }
  std::vector<SkPoint> points;
  points.reserve(32);
  const std::array<SkPoint, 4> centers{{
      {x + width - radius, y + radius},
      {x + width - radius, y + height - radius},
      {x + radius, y + height - radius},
      {x + radius, y + radius},
  }};
  for (int corner = 0; corner < 4; ++corner) {
    const float start = (float)corner * HALF_PI - HALF_PI;
    for (int step = 0; step < 8; ++step) {
      const float angle = start + HALF_PI * (float)step / 7.0f;
      points.push_back({centers[corner].fX + std::cos(angle) * radius,
                        centers[corner].fY + std::sin(angle) * radius});
    }
  }
  paintPolygon(pen, points, true);
}

PlacedPlot Engine::circle(Pen& pen, float x, float y, float radius) const {
  return circle(pen, x, y, radius, 0.0f);
}

PlacedPlot Engine::circle(Pen& pen, float x, float y, float radius,
                          float irregularity) const {
  if (!(radius > 0.0f)) return PlacedPlot{};
  std::vector<SkPoint> points = circlePoints(x, y, radius, 0.0f, TWO_PI, 96);
  irregularity = std::max(0.0f, irregularity);
  if (irregularity > 0.0f) {
    for (SkPoint& point : points) {
      const float dx = point.fX - x;
      const float dy = point.fY - y;
      const float scale =
          1.0f + pen.randomGaussian(0.0f, irregularity * 0.018f);
      point = {x + dx * scale, y + dy * scale};
    }
  }
  paintPolygon(pen, points, true);
  return {Plot::fromStroke(closedOutline(points), PlotType::Segments),
          points.front()};
}

std::optional<PlacedPlot> Engine::arc(Pen& pen, float x, float y, float radius,
                                      float start, float stop) const {
  start = toRadians(pen, start);
  stop = toRadians(pen, stop);
  if (!(radius > 0.0f)) return std::nullopt;
  float sweep = std::fmod(stop - start, TWO_PI);
  if (sweep < 0.0f) sweep += TWO_PI;
  if (std::abs(sweep) < 0.000001f) return std::nullopt;
  stop = start + sweep;
  const std::vector<SkPoint> points =
      circlePoints(x, y, radius, start, stop, 64);
  Stroke path;
  path.reserve(points.size());
  for (SkPoint point : points) path.push_back({point, 1.0f});
  paint(pen, path);
  return PlacedPlot{Plot::fromStroke(path, PlotType::Segments), points.front()};
}

void Engine::beginShape(float curvature) {
  m_shape.clear();
  m_shapeCurvature = std::clamp(curvature, 0.0f, 1.0f);
}

void Engine::vertex(float x, float y, float pressure) {
  m_shape.push_back({{x, y}, std::max(0.0f, pressure)});
}

std::optional<PlacedPlot> Engine::endShape(Pen& pen, bool close) {
  if (m_shape.size() < 2) {
    m_shape.clear();
    return std::nullopt;
  }
  Stroke shapePath = brush::spline(
      m_shape, definition() ? definition()->spacing : 1.0f, m_shapeCurvature);
  PlacedPlot result{Plot::fromStroke(shapePath, PlotType::Segments),
                    shapePath.front().position};
  if (close) {
    if (shapePath.front().position != shapePath.back().position)
      shapePath.push_back(shapePath.front());
    paintBoundary(pen, shapePath, true, true);
  } else {
    paint(pen, shapePath);
  }
  m_shape.clear();
  return result;
}

// ---- interiors over stored geometry -----------------------------------------

void Engine::draw(Pen& pen, const Polygon& stored) const {
  if (!hasStroke() || stored.empty()) return;
  paintStroke(pen, tool(), closedOutline(stored.vertices), false);
}

void Engine::fill(Pen& pen, const Polygon& stored) const {
  if (!m_fillActive || stored.empty()) return;
  pen.push();
  applyClip(pen);
  brush::wash(pen, m_fill, stored.vertices);
  pen.pop();
}

void Engine::wash(Pen& pen, const Polygon& stored) const {
  if (!m_washActive || stored.empty()) return;
  pen.push();
  applyClip(pen);
  paintWash(pen, stored.vertices);
  pen.pop();
}

void Engine::hatch(Pen& pen, const Polygon& stored) const {
  if (!m_hatch || stored.empty()) return;
  pen.push();
  applyClip(pen);
  brush::hatch(pen, hatchTool(), stored.vertices, *m_hatch);
  pen.pop();
}

void Engine::mass(Pen& pen, const Polygon& stored) const {
  if (!m_mass || stored.empty()) return;
  pen.push();
  applyClip(pen);
  brush::mass(pen, m_massTool, stored.vertices, *m_mass);
  pen.pop();
}

void Engine::draw(Pen& pen, const Plot& stored, float x, float y,
                  float scale) const {
  if (!hasStroke() || stored.empty()) return;
  const Tool current = tool();
  paintStroke(pen, current, stored.path({x, y}, current.spacing, 0.5f, scale),
              true);
}

Polygon Engine::bent(const Pen& pen, std::span<const SkPoint> points) const {
  const Stroke boundary = shapedBoundary(pen, closedOutline(points), true);
  if (boundary.empty()) return {};
  return Polygon(
      positions(std::span(boundary).first(boundary.size() - 1)));
}

void Engine::fill(Pen& pen, const Plot& stored, float x, float y,
                  float scale) const {
  fill(pen, bent(pen, stored.polygon(x, y, 1.0f, 0.5f, scale).vertices));
}

void Engine::wash(Pen& pen, const Plot& stored, float x, float y,
                  float scale) const {
  wash(pen, bent(pen, stored.polygon(x, y, 1.0f, 0.5f, scale).vertices));
}

void Engine::hatch(Pen& pen, const Plot& stored, float x, float y,
                   float scale) const {
  hatch(pen, bent(pen, stored.polygon(x, y, 1.0f, 0.5f, scale).vertices));
}

void Engine::mass(Pen& pen, const Plot& stored, float x, float y,
                  float scale) const {
  mass(pen, bent(pen, stored.polygon(x, y, 1.0f, 0.5f, scale).vertices));
}

// ---- the cursor -------------------------------------------------------------

Position Engine::position(float x, float y) const {
  const Direction* found = activeField();
  if (!found) return Position(x, y);
  return Position(
      x, y,
      [field = *found, amount = m_fieldAmount](SkPoint point, float seconds) {
        return field(point, seconds) * amount;
      },
      0.0f);
}

Position Engine::position(const Pen& pen, float x, float y) const {
  const Direction* found = activeField();
  const SkRect bounds = SkRect::MakeWH(pen.width, pen.height);
  if (!found) return Position(x, y, {}, 0.0f, bounds);
  return Position(
      x, y,
      [field = *found, amount = m_fieldAmount](SkPoint point, float seconds) {
        return field(point, seconds) * amount;
      },
      fieldSeconds(pen), bounds);
}

}  // namespace sigil::draw::brush
