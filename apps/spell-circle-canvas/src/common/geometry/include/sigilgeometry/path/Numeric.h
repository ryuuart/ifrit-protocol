#pragma once
/** @file
 * Small numeric routines every geometry tool reaches for and none should
 * spell twice.
 */
#include <cmath>
#include <glm/vec2.hpp>
#include <numbers>

namespace sigil::geometry::path {

inline constexpr float kPi = std::numbers::pi_v<float>;
inline constexpr float kTau = 2.0f * kPi;
// Each written as the correctly rounded float of the exact value, not as a
// quotient of the rounded kPi: 180 / kPi lands one ulp below the nearest
// float to 180 / π, and an angle scaled by it drifts by that ulp.
inline constexpr float kDegToRad = 0.017453293f;
inline constexpr float kRadToDeg = 57.29578f;

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
