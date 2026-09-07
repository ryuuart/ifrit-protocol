/** @file
 * Rate-independent sampling of pointer and stylus input: the spacing is
 * walked by SigilGeometryPath, and what is added here is everything the
 * device reports and the walk cannot know — the speed filter, the tilt
 * and rotation carried between events, and the heading a first mark
 * waits for.
 */

#include <sigildraw/Math.h>
#include <sigildraw/brush/Sampler.h>

#include <algorithm>
#include <cmath>

namespace sigil::draw::brush {

namespace {

float lerpAngle(float a, float b, float t) {
  return a + std::remainder(b - a, TWO_PI) * t;
}

Input interpolate(const Input& a, const Input& b, float t) {
  return {.position = {lerp(a.position.fX, b.position.fX, t),
                       lerp(a.position.fY, b.position.fY, t)},
          .pressure = lerp(a.pressure, b.pressure, t),
          .tilt = lerp(a.tilt, b.tilt, t),
          .barrelRotation = lerpAngle(a.barrelRotation, b.barrelRotation, t),
          .seconds = a.seconds + (b.seconds - a.seconds) * t,
          .tiltDirection = lerpAngle(a.tiltDirection, b.tiltDirection, t)};
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

Sampler::Sampler(float speedFilterSeconds)
    : m_speedFilterSeconds(std::max(0.0f, speedFilterSeconds)) {}

std::vector<Dab> Sampler::begin(Input input) {
  m_previous = input;
  m_filteredSpeed = 0.0f;
  m_walk.restart();
  m_lastDabPosition = input.position;
  m_active = true;
  m_beginPending = true;
  return {};
}

std::vector<Dab> Sampler::move(Input input, float spacing) {
  if (!m_active) return begin(input);
  spacing = std::max(0.125f, spacing);

  const float dx = input.position.fX - m_previous.position.fX;
  const float dy = input.position.fY - m_previous.position.fY;
  const float length = std::hypot(dx, dy);
  const double elapsed = input.seconds - m_previous.seconds;
  const float targetSpeed = elapsed > 0.0 ? length / (float)elapsed : 0.0f;
  if (elapsed > 0.0 && m_speedFilterSeconds > 0.0f) {
    const float retained = std::exp(-(float)elapsed / m_speedFilterSeconds);
    m_filteredSpeed =
        retained * m_filteredSpeed + (1.0f - retained) * targetSpeed;
  } else if (elapsed > 0.0) {
    m_filteredSpeed = targetSpeed;
  }

  std::vector<Dab> result;
  if (length > 0.0f) {
    const float direction = std::atan2(dy, dx);
    if (m_beginPending) {
      result.push_back(makeDab(m_previous, direction, 0.0f, 0.0f));
      m_beginPending = false;
    }
    m_walk.advance(length, spacing, [&](geometry::path::Stride::Step step) {
      const Input at = interpolate(m_previous, input, step.fraction);
      result.push_back(makeDab(at, direction, m_filteredSpeed, step.distance));
      m_lastDabPosition = at.position;
    });
  }
  m_previous = input;
  return result;
}

std::vector<Dab> Sampler::end(Input input, float spacing) {
  if (!m_active) return {};
  std::vector<Dab> result = move(input, spacing);
  if (m_beginPending) {
    result.push_back(makeDab(input, 0.0f, 0.0f, 0.0f));
    m_beginPending = false;
  } else if (dist(m_lastDabPosition.fX, m_lastDabPosition.fY, input.position.fX,
                  input.position.fY) > 0.001f) {
    const float direction =
        std::atan2(input.position.fY - m_lastDabPosition.fY,
                   input.position.fX - m_lastDabPosition.fX);
    result.push_back(
        makeDab(input, direction, m_filteredSpeed, m_walk.travelled()));
  }
  m_active = false;
  return result;
}

void Sampler::cancel() {
  m_active = false;
  m_beginPending = false;
}

std::vector<Dab> dabs(std::span<const Input> input, float spacing,
                      float speedFilterSeconds) {
  if (input.empty()) return {};
  Sampler sampler(speedFilterSeconds);
  std::vector<Dab> result = sampler.begin(input.front());
  for (size_t i = 1; i + 1 < input.size(); ++i) {
    std::vector<Dab> next = sampler.move(input[i], spacing);
    result.insert(result.end(), next.begin(), next.end());
  }
  std::vector<Dab> last = sampler.end(input.back(), spacing);
  result.insert(result.end(), last.begin(), last.end());
  const float total = result.empty() ? 0.0f : result.back().distance;
  if (total > 0.0f)
    for (Dab& dab : result) dab.progress = dab.distance / total;
  return result;
}

}  // namespace sigil::draw::brush
