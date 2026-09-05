#pragma once

/** @file
 * Device input resampled at a spacing, whatever rate the device reports at.
 */

#include <sigilgeometry/path/Stride.h>
#include <sigildraw/brush/Dab.h>

#include <span>
#include <vector>

namespace sigil::draw::brush {

/** How long the speed filter remembers: a first-order low-pass with this
 *  time constant, so size and opacity dynamics do not chatter when a
 *  device reports uneven intervals. */
inline constexpr float kSpeedFilterSeconds = 0.04f;

/** Resamples live device input into dabs one spacing apart, on
 *  SigilGeometryPath's even-spacing walk, so the event rate cannot
 *  change the density of pigment.
 *
 *  The dab at the beginning of a stroke is held until the first movement
 *  supplies its direction, so a tip that follows the heading never stamps
 *  its first mark at angle zero; a stroke that ends without moving is one
 *  dab at direction zero. */
class Sampler {
 public:
  explicit Sampler(float speedFilterSeconds = kSpeedFilterSeconds);

  /** Starts a stroke. Answers no dabs: the first is emitted by the first
   *  `move` that travels. */
  [[nodiscard]] std::vector<Dab> begin(Input input);
  [[nodiscard]] std::vector<Dab> move(Input input, float spacing);
  [[nodiscard]] std::vector<Dab> end(Input input, float spacing);
  void cancel();

  [[nodiscard]] bool active() const { return m_active; }
  [[nodiscard]] float distance() const { return m_walk.travelled(); }

 private:
  Input m_previous;
  /** The even-spacing walk itself, which owns how far the stroke has run
   *  and how much of a spacing it still owes; what this class adds is
   *  what a device reports and the walk cannot know. */
  geometry::path::Stride m_walk;
  float m_speedFilterSeconds = kSpeedFilterSeconds;
  float m_filteredSpeed = 0.0f;
  SkPoint m_lastDabPosition{0, 0};
  bool m_active = false;
  bool m_beginPending = false;
};

/** Resamples a complete device path and assigns each dab its unit
 *  progress along it. */
[[nodiscard]] std::vector<Dab> dabs(std::span<const Input> input, float spacing,
                                    float speedFilterSeconds = kSpeedFilterSeconds);

}  // namespace sigil::draw::brush
