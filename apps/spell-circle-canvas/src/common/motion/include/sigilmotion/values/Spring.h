#pragma once

/** @file
 * @ingroup motion-values
 *
 * The damped-overshoot stepper: a value flying at a target under a
 * spring, carrying its own velocity, stated as a period and a damping
 * ratio.
 */

#include <sigilmotion/time/Duration.h>

#include <chrono>
#include <cmath>

namespace sigil::motion {

/** HOW a spring settles, in the two numbers a designer picks.
 *
 *  `period` is how long the spring would take to oscillate once with no
 *  damping at all — how FAST, independent of how bouncy. `damping` is
 *  the ratio: below 1 the value overshoots and rings, at 1 it arrives as
 *  fast as it can without ever crossing the target, above 1 it crawls in
 *  from one side. 0 is a bell that never stops.
 *
 *  Stated this way because the two are independent: changing the period
 *  re-times the same shape, changing the damping reshapes the same
 *  timing. The physical pair (stiffness, viscosity) is not, and a
 *  consumer tuning one of those has to retune the other to keep the
 *  look. */
struct SpringParameters {
  Duration period = std::chrono::milliseconds(400);
  float damping = 0.5f;

  bool operator==(const SpringParameters&) const = default;
};

/** WHERE a spring is: the value it holds and the velocity it holds it
 *  with, in the caller's units per second.
 *
 *  The velocity is the whole reason this is a state rather than a curve.
 *  An `ease::` curve is a function of a normalised progress between two
 *  fixed endpoints, so a target that moves mid-flight can only restart
 *  it; a spring carries the motion it already has into the new target
 *  and bends. */
struct Spring {
  float value = 0.0f;
  float velocity = 0.0f;

  bool operator==(const Spring&) const = default;

  /** THE SPRING @p elapsed LATER, flying at @p target.
   *
   *  Value in, value out: `state = state.step(target, elapsed)`. The target
   *  is a per-step argument rather than a member, because a spring's whole
   *  point is that the target is allowed to move between steps and the
   *  motion carries.
   *
   *  Solved in closed form rather than integrated, so ONE STEP OF ANY SIZE
   *  IS EXACT: fifty steps of a frame and one step of fifty frames land on
   *  the same value. That is what makes it safe on the delta a frame clock
   *  actually hands over — a stalled frame cannot blow the spring up, and
   *  it takes no substepping to stop it — and it is what lets a caller
   *  with no state to keep ask for the whole flight at once, stepping a
   *  spring at rest by the age of the thing it animates.
   *
   *  A non-positive period answers the target at rest — the spelling of
   *  "instant". A non-positive @p elapsed answers the spring unchanged. A
   *  negative damping is read as 0. */
  [[nodiscard]] Spring step(float target, Duration elapsed,
                            SpringParameters parameters = {}) const {
    const float seconds = (float)elapsed.count();
    if (!(seconds > 0.0f)) return *this;
    const float period = (float)parameters.period.count();
    if (!(period > 0.0f)) return {target, 0.0f};

    const float omega = 6.2831853071795864769f / period;
    const float zeta = parameters.damping > 0.0f ? parameters.damping : 0.0f;

    // Solve about the target: the displacement decays to 0.
    const float displacement = value - target;
    const float speed = velocity;

    float settledDisplacement = 0.0f;
    float settledSpeed = 0.0f;
    if (zeta < 1.0f) {
      // Under-damped: it rings. The overshoot the caller came for.
      const float ringing = omega * std::sqrt(1.0f - zeta * zeta);
      const float envelope = std::exp(-zeta * omega * seconds);
      const float cosine = std::cos(ringing * seconds);
      const float sine = std::sin(ringing * seconds);
      const float along = displacement;
      const float across = (speed + zeta * omega * displacement) / ringing;
      settledDisplacement = envelope * (along * cosine + across * sine);
      settledSpeed = -zeta * omega * settledDisplacement +
                     envelope * ringing * (across * cosine - along * sine);
    } else if (zeta == 1.0f) {
      // Critically damped: the fastest arrival that never crosses.
      const float envelope = std::exp(-omega * seconds);
      const float slope = speed + omega * displacement;
      settledDisplacement = envelope * (displacement + slope * seconds);
      settledSpeed = envelope * (speed - omega * slope * seconds);
    } else {
      // Over-damped: two real rates, the slower one deciding the tail.
      const float spread = omega * std::sqrt(zeta * zeta - 1.0f);
      const float slower = -omega * zeta + spread;
      const float faster = -omega * zeta - spread;
      const float fasterPart = (speed - slower * displacement) / (faster - slower);
      const float slowerPart = displacement - fasterPart;
      const float slowerDecay = std::exp(slower * seconds);
      const float fasterDecay = std::exp(faster * seconds);
      settledDisplacement = slowerPart * slowerDecay + fasterPart * fasterDecay;
      settledSpeed = slowerPart * slower * slowerDecay +
                     fasterPart * faster * fasterDecay;
    }
    return {target + settledDisplacement, settledSpeed};
  }

  /** HAS IT SETTLED: within @p slack of @p target and slower than @p slack
   *  per second, in the caller's own units.
   *
   *  A spring approaches exponentially and so never exactly arrives,
   *  which makes "has it finished" a question about a tolerance rather
   *  than a fact — the caller states the distance and the rate at which
   *  its own units stop showing a difference. Both are the same number
   *  because the answer wanted is "smaller than a pixel, and not about to
   *  be bigger than one in the next frame". */
  [[nodiscard]] bool isSettled(float target, float slack = 0.5f) const {
    return std::abs(value - target) <= slack && std::abs(velocity) <= slack;
  }
};

}  // namespace sigil::motion
