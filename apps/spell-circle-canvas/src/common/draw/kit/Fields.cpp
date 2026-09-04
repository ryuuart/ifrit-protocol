/** @file
 * Stock direction fields for procedural brush paths.
 */

#include <sigildraw/kit/Fields.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace sigil::draw::brush::fields {

Curl::Curl(uint32_t seed, float scale, float drift)
    : m_noise(seed), m_scale(std::max(0.000001f, scale)), m_drift(drift) {
  m_noise.detail(4, 0.5f);
}

float Curl::operator()(SkPoint point, float seconds) const {
  constexpr float derivative = 0.35f;
  const float x = point.fX * m_scale;
  const float y = point.fY * m_scale;
  const float z = seconds * m_drift;
  const float dx =
      m_noise.at(x + derivative, y, z) - m_noise.at(x - derivative, y, z);
  const float dy =
      m_noise.at(x, y + derivative, z) - m_noise.at(x, y - derivative, z);
  if (std::abs(dx) + std::abs(dy) < 0.000001f) return 0.0f;
  return std::atan2(-dx, dy);
}

float Vortex::operator()(SkPoint point, float) const {
  const float radial = std::atan2(point.fY - center.fY, point.fX - center.fX);
  const float turn = direction < 0.0f ? -std::numbers::pi_v<float> * 0.5f
                                      : std::numbers::pi_v<float> * 0.5f;
  return radial + turn -
         std::clamp(pull, -1.0f, 1.0f) * std::numbers::pi_v<float> * 0.5f;
}

float Wave::operator()(SkPoint point, float seconds) const {
  const float period = std::max(0.001f, wavelength);
  return direction +
         std::sin(point.fX / period * std::numbers::pi_v<float> * 2.0f +
                  seconds * speed) *
             amplitude;
}

}  // namespace sigil::draw::brush::fields
