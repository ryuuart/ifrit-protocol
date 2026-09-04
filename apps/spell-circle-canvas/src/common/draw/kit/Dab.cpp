/** @file
 * Rate-independent sampling of pointer and stylus input.
 */

#include <sigildraw/Math.h>
#include <sigildraw/kit/Dab.h>

#include <algorithm>
#include <cmath>

namespace sigil::draw::brush {

namespace {

float distanceBetween(SkPoint a, SkPoint b) {
  return std::hypot(b.fX - a.fX, b.fY - a.fY);
}

float lerp(float a, float b, float t) { return a + (b - a) * t; }

float lerpAngle(float a, float b, float t) {
  return a + std::remainder(b - a, TWO_PI) * t;
}

Input interpolate(const Input& a, const Input& b, float t) {
  return {{lerp(a.position.fX, b.position.fX, t),
           lerp(a.position.fY, b.position.fY, t)},
          lerp(a.pressure, b.pressure, t),
          lerp(a.tilt, b.tilt, t),
          lerpAngle(a.barrelRotation, b.barrelRotation, t),
          a.seconds + (b.seconds - a.seconds) * t,
          lerpAngle(a.tiltDirection, b.tiltDirection, t)};
}

Dab makeDab(const Input& input, float direction, float speed, float distance) {
  return {.position = input.position,
          .pressure = std::max(0.0f, input.pressure),
          .tilt = std::clamp(input.tilt, 0.0f, 1.0f),
          .barrelRotation = input.barrelRotation,
          .direction = direction,
          .speed = std::max(0.0f, speed),
          .distance = distance,
          .tiltDirection = input.tiltDirection};
}

}  // namespace

Sampler::Sampler(float velocitySmoothingSeconds)
    : m_velocitySmoothingSeconds(std::max(0.0f, velocitySmoothingSeconds)) {}

std::vector<Dab> Sampler::begin(Input input) {
  m_previous = input;
  m_filteredSpeed = 0.0f;
  m_distance = 0.0f;
  m_nextDistance = 0.0f;
  m_lastDabPosition = input.position;
  m_active = true;
  return {makeDab(input, 0.0f, 0.0f, 0.0f)};
}

std::vector<Dab> Sampler::move(Input input, float spacing) {
  if (!m_active) return begin(input);
  spacing = std::max(0.125f, spacing);

  const float dx = input.position.fX - m_previous.position.fX;
  const float dy = input.position.fY - m_previous.position.fY;
  const float length = std::hypot(dx, dy);
  const double elapsed = input.seconds - m_previous.seconds;
  const float targetSpeed = elapsed > 0.0 ? length / (float)elapsed : 0.0f;
  if (elapsed > 0.0 && m_velocitySmoothingSeconds > 0.0f) {
    const float retained =
        std::exp(-(float)elapsed / m_velocitySmoothingSeconds);
    m_filteredSpeed =
        retained * m_filteredSpeed + (1.0f - retained) * targetSpeed;
  } else if (elapsed > 0.0) {
    m_filteredSpeed = targetSpeed;
  }

  std::vector<Dab> result;
  if (length > 0.0f) {
    const float direction = std::atan2(dy, dx);
    if (m_nextDistance <= m_distance) m_nextDistance = m_distance + spacing;
    while (m_nextDistance <= m_distance + length) {
      const float t = (m_nextDistance - m_distance) / length;
      const Input at = interpolate(m_previous, input, t);
      result.push_back(makeDab(at, direction, m_filteredSpeed, m_nextDistance));
      m_lastDabPosition = at.position;
      m_nextDistance += spacing;
    }
    m_distance += length;
  }
  m_previous = input;
  return result;
}

std::vector<Dab> Sampler::end(Input input, float spacing) {
  if (!m_active) return {};
  std::vector<Dab> result = move(input, spacing);
  if (distanceBetween(m_lastDabPosition, input.position) > 0.001f) {
    const float direction =
        std::atan2(input.position.fY - m_lastDabPosition.fY,
                   input.position.fX - m_lastDabPosition.fX);
    result.push_back(makeDab(input, direction, m_filteredSpeed, m_distance));
  }
  m_active = false;
  return result;
}

void Sampler::cancel() { m_active = false; }

std::vector<Dab> dabs(std::span<const Input> input, float spacing,
                      float velocitySmoothingSeconds) {
  if (input.empty()) return {};
  Sampler sampler(velocitySmoothingSeconds);
  std::vector<Dab> result = sampler.begin(input.front());
  for (size_t i = 1; i + 1 < input.size(); ++i) {
    std::vector<Dab> next = sampler.move(input[i], spacing);
    result.insert(result.end(), next.begin(), next.end());
  }
  if (input.size() > 1) {
    std::vector<Dab> last = sampler.end(input.back(), spacing);
    result.insert(result.end(), last.begin(), last.end());
  }
  const float total = result.empty() ? 0.0f : result.back().distance;
  if (result.size() > 1) result.front().direction = result[1].direction;
  if (total > 0.0f)
    for (Dab& dab : result) dab.progress = dab.distance / total;
  return result;
}

}  // namespace sigil::draw::brush
