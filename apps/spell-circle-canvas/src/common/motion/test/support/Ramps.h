#pragma once

/** @file
 * The transitioned value every test that drives a motion starts from: a
 * linear ramp to a target over a stated number of milliseconds. Linear
 * because the assertions read the value halfway through and want to name
 * the number without evaluating a curve.
 */

#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Keyframes.h>

#include <chrono>

namespace sigil::motion::test {

/** An animatable that ramps linearly to @p to over @p ms. */
inline Animatable<float> ramped(float to, int ms) {
  Transitioned<float> t;
  t.value = to;
  t.spec.duration = std::chrono::milliseconds(ms);
  t.spec.ease = &choreograph::easeNone;
  return t;
}

}  // namespace sigil::motion::test
