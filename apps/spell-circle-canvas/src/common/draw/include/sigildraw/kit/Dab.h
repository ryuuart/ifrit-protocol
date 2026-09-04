#pragma once

/** @file
 * Device input resampled into the evenly spaced dabs a brush deposits.
 */

#include <include/core/SkPoint.h>

#include <span>
#include <vector>

namespace sigil::draw::brush {

/** One device observation. Pressure is a unit value. Tilt is zero for an
 * upright stylus and one for a stylus flat against the surface. Barrel and
 * tilt-direction angles are radians, and seconds comes from the host's
 * monotonic sketch clock. */
struct Input {
  SkPoint position{0, 0};
  float pressure = 1.0f;
  float tilt = 0.0f;
  float barrelRotation = 0.0f;
  double seconds = 0.0;
  float tiltDirection = 0.0f;

  bool operator==(const Input&) const = default;
};

/** One evenly spaced deposition event. Direction is the centreline tangent in
 * radians, speed is canvas units per second, distance is measured from the
 * beginning and progress is the unit position within a completed stroke. */
struct Dab {
  SkPoint position{0, 0};
  float pressure = 1.0f;
  float tilt = 0.0f;
  float barrelRotation = 0.0f;
  float direction = 0.0f;
  float speed = 0.0f;
  float distance = 0.0f;
  float progress = 0.0f;
  float tiltDirection = 0.0f;

  bool operator==(const Dab&) const = default;
};

/** Stateful resampler for live device input. It carries the unspent fraction
 * of a spacing interval across input events, so event rate cannot change the
 * density of pigment. Velocity passes through a first-order low-pass filter. */
class Sampler {
 public:
  explicit Sampler(float velocitySmoothingSeconds = 0.04f);

  [[nodiscard]] std::vector<Dab> begin(Input input);
  [[nodiscard]] std::vector<Dab> move(Input input, float spacing);
  [[nodiscard]] std::vector<Dab> end(Input input, float spacing);
  void cancel();

  [[nodiscard]] bool active() const { return m_active; }
  [[nodiscard]] float distance() const { return m_distance; }

 private:
  Input m_previous;
  float m_velocitySmoothingSeconds = 0.04f;
  float m_filteredSpeed = 0.0f;
  float m_distance = 0.0f;
  float m_nextDistance = 0.0f;
  SkPoint m_lastDabPosition{0, 0};
  bool m_active = false;
};

/** Resamples a complete device path and assigns final unit progress. */
[[nodiscard]] std::vector<Dab> dabs(std::span<const Input> input, float spacing,
                                    float velocitySmoothingSeconds = 0.04f);

}  // namespace sigil::draw::brush
