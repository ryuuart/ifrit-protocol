#pragma once

/** @file
 * Stock direction fields for procedural brush paths.
 */

#include <include/core/SkPoint.h>
#include <sigildraw/Noise.h>

#include <cstdint>

namespace sigil::draw::brush::fields {

/** A divergence-free direction taken from the finite derivatives of one
 *  smooth noise field. Scale sets the size of its eddies and drift moves the
 *  field through its third dimension over time. */
class Curl {
 public:
  explicit Curl(uint32_t seed = 0, float scale = 0.004f, float drift = 0.08f);

  [[nodiscard]] float operator()(SkPoint point, float seconds) const;
  [[nodiscard]] uint32_t seed() const { return m_noise.seed(); }
  [[nodiscard]] float scale() const { return m_scale; }
  [[nodiscard]] float drift() const { return m_drift; }

  bool operator==(const Curl& other) const {
    return seed() == other.seed() && m_scale == other.m_scale &&
           m_drift == other.m_drift;
  }

 private:
  NoiseField m_noise;
  float m_scale;
  float m_drift;
};

/** Tangential flow around one centre. Pull bends the tangent inward when
 *  positive and outward when negative; direction is one for clockwise and
 *  minus one for anticlockwise. */
struct Vortex {
  SkPoint center{0, 0};
  float direction = 1.0f;
  float pull = 0.0f;

  [[nodiscard]] float operator()(SkPoint point, float seconds) const;
  bool operator==(const Vortex&) const = default;
};

/** Parallel flow whose direction oscillates across the canvas and in time. */
struct Wave {
  float direction = 0.0f;
  float amplitude = 0.35f;
  float wavelength = 180.0f;
  float speed = 0.5f;

  [[nodiscard]] float operator()(SkPoint point, float seconds) const;
  bool operator==(const Wave&) const = default;
};

}  // namespace sigil::draw::brush::fields
