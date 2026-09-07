/** @file
 * The field-aware cursor.
 */

#include <sigildraw/brush/Plot.h>
#include <sigildraw/brush/Position.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace sigil::draw::brush {

Position::Position(float x, float y, Direction field, float seconds,
                   std::optional<SkRect> bounds)
    : m_x(x),
      m_y(y),
      m_field(std::move(field)),
      m_seconds(seconds),
      m_bounds(bounds ? std::optional(bounds->makeSorted()) : std::nullopt) {}

Stroke Position::moveTo(float direction, float length, float stepLength,
                        const Direction& field, float seconds) {
  Stroke result{{{m_x, m_y}, 1.0f}};
  if (!(length > 0.0f)) return result;
  stepLength = std::max(0.125f, stepLength);
  if (!isIn()) {
    m_plotted += stepLength;
    return result;
  }
  const Direction& active = field ? field : m_field;
  const float activeSeconds = field ? seconds : m_seconds;
  float travelled = 0.0f;
  while (travelled < length) {
    const float step = std::min(stepLength, length - travelled);
    const float heading =
        direction + (active ? active({m_x, m_y}, activeSeconds) : 0.0f);
    place(m_x + std::cos(heading) * step, m_y + std::sin(heading) * step);
    m_plotted += step;
    travelled += step;
    result.push_back({{m_x, m_y}, 1.0f});
  }
  return result;
}

Stroke Position::plotTo(const Plot& plot, float length, float stepLength,
                        float scale) {
  Stroke result{{{m_x, m_y}, plot.pressure(m_plotted)}};
  if (!(length > 0.0f) || plot.empty()) return result;
  stepLength = std::max(0.125f, stepLength);
  scale = std::max(0.0001f, std::abs(scale));
  if (!isIn()) {
    m_plotted += stepLength / scale;
    return result;
  }
  float travelled = 0.0f;
  while (travelled < length) {
    const float step = std::min(stepLength, length - travelled);
    const float heading = angle() + plot.angle(m_plotted);
    place(m_x + std::cos(heading) * step, m_y + std::sin(heading) * step);
    m_plotted += step / scale;
    travelled += step;
    result.push_back({{m_x, m_y}, plot.pressure(m_plotted)});
  }
  return result;
}

float Position::angle(const Direction& field, float seconds) const {
  return field ? field({m_x, m_y}, seconds) : 0.0f;
}

float Position::angle() const {
  return m_field ? m_field({m_x, m_y}, m_seconds) : 0.0f;
}

bool Position::isIn() const { return isInCanvas(); }

bool Position::isInCanvas() const {
  if (!m_bounds) return std::isfinite(m_x) && std::isfinite(m_y);
  SkRect region = *m_bounds;
  region.outset(region.width() * 0.5f, region.height() * 0.5f);
  return region.contains(m_x, m_y);
}

void Position::place(float x, float y) {
  m_x = x;
  m_y = y;
}

void Position::field(Direction value, float seconds) {
  m_field = std::move(value);
  m_seconds = seconds;
}

}  // namespace sigil::draw::brush
