/** @file
 * Stock direction fields.
 */

#include <sigildraw/Constants.h>
#include <sigildraw/brush/Fields.h>

#include <algorithm>
#include <cmath>

namespace sigil::draw::brush {

Curl::Curl(uint32_t seed, float scale, float drift)
    : m_noise(seed), m_scale(std::max(0.000001f, scale)), m_drift(drift) {
  m_noise.detail(4, 0.5f);
}

float Curl::operator()(SkPoint point, float seconds) const {
  constexpr float kDerivativeStep = 0.35f;
  const float x = point.fX * m_scale;
  const float y = point.fY * m_scale;
  const float z = seconds * m_drift;
  const float dx = m_noise.at(x + kDerivativeStep, y, z) -
                   m_noise.at(x - kDerivativeStep, y, z);
  const float dy = m_noise.at(x, y + kDerivativeStep, z) -
                   m_noise.at(x, y - kDerivativeStep, z);
  if (std::abs(dx) + std::abs(dy) < 0.000001f) return 0.0f;
  return std::atan2(-dx, dy);
}

float Vortex::operator()(SkPoint point, float) const {
  const float radial = std::atan2(point.fY - center.fY, point.fX - center.fX);
  const float turn = direction < 0.0f ? -HALF_PI : HALF_PI;
  return radial + turn - std::clamp(pull, -1.0f, 1.0f) * HALF_PI;
}

float Wave::operator()(SkPoint point, float seconds) const {
  const float period = std::max(0.001f, wavelength);
  return direction +
         std::sin(point.fX / period * TWO_PI + seconds * speed) * amplitude;
}

std::vector<std::pair<std::string, Direction>> stockFields() {
  return {
      {"hand",
       [](SkPoint point, float seconds) {
         return std::sin(point.fX * 0.045f + point.fY * 0.021f + seconds) *
                0.18f;
       }},
      {"curved",
       [](SkPoint point, float) {
         return std::atan2(point.fY, point.fX) + HALF_PI;
       }},
      {"zigzag",
       [](SkPoint point, float) {
         return std::sin(point.fX * 0.08f) >= 0.0f ? 0.55f : -0.55f;
       }},
      {"waves",
       [](SkPoint point, float seconds) {
         return std::sin(point.fX * 0.018f + seconds) * 0.42f;
       }},
      {"seabed",
       [](SkPoint point, float seconds) {
         return -0.18f +
                std::sin(point.fX * 0.011f + point.fY * 0.006f + seconds) *
                    0.24f;
       }},
      {"spiral",
       [](SkPoint point, float) {
         return std::atan2(point.fY, point.fX) + PI * 0.42f;
       }},
      {"columns",
       [](SkPoint point, float) {
         return HALF_PI + std::sin(point.fY * 0.025f) * 0.12f;
       }},
  };
}

}  // namespace sigil::draw::brush
