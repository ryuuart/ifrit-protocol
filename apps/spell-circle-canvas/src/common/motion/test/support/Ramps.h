#pragma once

/** @file
 * The described motion every test that drives a held value starts from: a
 * linear tween to a target over a stated length. Linear because the
 * assertions read the value partway through and want to name the number
 * without evaluating a curve.
 */

#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/time/Duration.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Tween.h>

namespace sigil::motion::test {

/** A value that eases linearly to @p to over @p length when it changes. */
inline Animatable<float> ramped(float to, Duration length) {
  return animate({.to = to, .duration = length, .ease = ease::linear});
}

}  // namespace sigil::motion::test
