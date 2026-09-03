#pragma once

/** @file
 * The damped-overshoot stepper: a value flying at a target under a
 * spring, carrying its own velocity, stated in seconds of period and a
 * damping ratio.
 */

#include <cmath>

namespace sigil::motion {

/** HOW a spring settles, in the two numbers a designer picks.
 *
 *  `periodSeconds` is the period the spring would oscillate at with no
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
struct SpringParams {
  float periodSeconds = 0.4f;
  float damping = 0.5f;
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
};

/** Advance a spring `dt` seconds towards `target`.
 *
 *  Value in, value out: `s = spring(s, target, dt)`. The target is a
 *  per-step argument rather than a member, because a spring's whole
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
 *  "instant". A non-positive `dt` answers the spring unchanged. A
 *  negative damping is read as 0. */
inline Spring spring(Spring from, float target, float dt,
                     SpringParams p = {}) {
  if (!(dt > 0.0f)) return from;
  if (!(p.periodSeconds > 0.0f)) return {target, 0.0f};

  const float omega = 6.2831853071795864769f / p.periodSeconds;
  const float zeta = p.damping > 0.0f ? p.damping : 0.0f;

  // Solve about the target: x is the displacement, which decays to 0.
  const float x = from.value - target;
  const float v = from.velocity;

  float xt = 0.0f;
  float vt = 0.0f;
  if (zeta < 1.0f) {
    // Under-damped: it rings. The overshoot the caller came for.
    const float wd = omega * std::sqrt(1.0f - zeta * zeta);
    const float e = std::exp(-zeta * omega * dt);
    const float c = std::cos(wd * dt);
    const float s = std::sin(wd * dt);
    const float a = x;
    const float b = (v + zeta * omega * x) / wd;
    xt = e * (a * c + b * s);
    vt = -zeta * omega * xt + e * wd * (b * c - a * s);
  } else if (zeta == 1.0f) {
    // Critically damped: the fastest arrival that never crosses.
    const float e = std::exp(-omega * dt);
    const float b = v + omega * x;
    xt = e * (x + b * dt);
    vt = e * (v - omega * b * dt);
  } else {
    // Over-damped: two real rates, the slower one deciding the tail.
    const float r = omega * std::sqrt(zeta * zeta - 1.0f);
    const float r1 = -omega * zeta + r;
    const float r2 = -omega * zeta - r;
    const float c2 = (v - r1 * x) / (r2 - r1);
    const float c1 = x - c2;
    const float e1 = std::exp(r1 * dt);
    const float e2 = std::exp(r2 * dt);
    xt = c1 * e1 + c2 * e2;
    vt = c1 * r1 * e1 + c2 * r2 * e2;
  }
  return {target + xt, vt};
}

/** IS IT STILL MOVING: within `slack` of the target and slower than
 *  `slack` per second, in the caller's own units.
 *
 *  A spring approaches exponentially and so never exactly arrives, which
 *  makes "has it finished" a question about a tolerance rather than a
 *  fact — the caller states the distance and the rate at which its own
 *  units stop showing a difference. Both are the same number because
 *  the answer wanted is "smaller than a pixel, and not about to be
 *  bigger than one in the next frame".
 *
 *  This is the *running* question, asked of a spring: it says the
 *  machinery is done, not that the value has provably held still. */
inline bool springMoving(const Spring& s, float target, float slack = 0.5f) {
  return std::abs(s.value - target) > slack || std::abs(s.velocity) > slack;
}

}  // namespace sigil::motion
