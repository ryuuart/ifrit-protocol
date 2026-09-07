#pragma once

/** @file
 * p5's calculation functions that hold no state: free functions, so a
 * pasted `map(...)` needs no pen in front of it. What reads or writes a
 * pen — `random`, `noise`, `millis` — is a verb on the pen instead.
 */

#include <sigildraw/Constants.h>

#include <algorithm>
#include <cmath>

namespace sigil::draw {

/** @p value re-mapped from the range [start1, stop1] to [start2, stop2];
 *  @p withinBounds constrains the result to the target range. */
inline float map(float value, float start1, float stop1, float start2,
                 float stop2, bool withinBounds = false) {
  const float out =
      (value - start1) / (stop1 - start1) * (stop2 - start2) + start2;
  if (!withinBounds) return out;
  return start2 < stop2 ? std::clamp(out, start2, stop2)
                        : std::clamp(out, stop2, start2);
}

/** The point @p amount of the way from @p start to @p stop. */
inline float lerp(float start, float stop, float amount) {
  return start + (stop - start) * amount;
}

/** @p n held inside [low, high]. */
inline float constrain(float n, float low, float high) {
  return std::clamp(n, low, high);
}

/** The distance between two points. */
inline float dist(float x1, float y1, float x2, float y2) {
  return std::hypot(x2 - x1, y2 - y1);
}

/** The length of a vector. */
inline float mag(float x, float y) { return std::hypot(x, y); }

/** @p value re-mapped from [start, stop] to [0, 1], unconstrained. */
inline float norm(float value, float start, float stop) {
  return map(value, start, stop, 0.0f, 1.0f);
}

/** @p n squared. */
inline float sq(float n) { return n * n; }

/** Degrees to radians. */
inline float radians(float degrees) { return degrees * (PI / 180.0f); }

/** Radians to degrees. */
inline float degrees(float radians) { return radians * (180.0f / PI); }

}  // namespace sigil::draw
