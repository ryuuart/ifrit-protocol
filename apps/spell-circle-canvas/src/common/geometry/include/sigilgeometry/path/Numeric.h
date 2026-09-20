#pragma once
/** @file
 * @ingroup geometry-path
 *
 * Small numeric routines every geometry tool reaches for and none should
 * spell twice.
 */
#include <sigilcore/compute/Angle.h>

#include <cmath>
#include <glm/vec2.hpp>
#include <numbers>

namespace sigil::geometry::path {

/** π in the float the whole library computes in — an angle that detours
 *  through a double constant and back rounds twice. */
inline constexpr float kPi = std::numbers::pi_v<float>;
/** A full turn: the period every angular `wrap` is taken against. */
inline constexpr float kTau = 2.0f * kPi;
/** Degrees to radians, and back — the ratios and the two verbs a caller
 *  spells, which are `core::angle`'s. They stand here because an angle
 *  is geometry's own word and a caller measuring one should not have to
 *  reach past this header for it; they carry no arithmetic, so a contour
 *  and a pen scale an angle by the same float. */
inline constexpr float kDegToRad = core::angle::kDegToRad;
/** Radians to degrees, the same single rounding. */
inline constexpr float kRadToDeg = core::angle::kRadToDeg;

/** Degrees → radians, as the verb that reads at a call site:
 *  `std::cos(radians(bearingInDegrees))`. */
inline constexpr float radians(float degrees) {
  return core::angle::radians(degrees);
}
/** Radians to degrees, through the same single rounding. */
inline constexpr float degrees(float radians) {
  return core::angle::degrees(radians);
}

/** Locates the boundary in [lo, hi] where a predicate stops holding, by
 *  bisection: `stillNear(x)` is true on the `lo` side and false on the
 *  `hi` side. Returns the first `hi` the search could not push any
 *  closer — the far side of the transition, never a point that still
 *  satisfies the predicate. */
template <typename Pred>
float bisect(float lo, float hi, Pred stillNear, int iterations = 8) {
  for (int i = 0; i < iterations; ++i) {
    const float mid = (lo + hi) * 0.5f;
    if (stillNear(mid))
      lo = mid;
    else
      hi = mid;
  }
  return hi;
}

/** Euclidean length, written as the single expression `sqrt(x*x + y*y)`
 *  so it rounds exactly as Skia's own point length does — a polyline
 *  measured here and a contour Skia measures must agree to the bit, or
 *  the two disagree about where a resampled point lands. */
inline float length(glm::vec2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }

/** Distance between two points, rounding as `length` does. */
inline float distance(glm::vec2 a, glm::vec2 b) { return length(b - a); }

/** `t` wrapped into [0, period). */
inline float wrap(float t, float period) {
  const float r = std::fmod(t, period);
  return r < 0 ? r + period : r;
}

}  // namespace sigil::geometry::path
