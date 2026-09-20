#pragma once

/** @file
 * @ingroup core-compute
 *
 * DEGREES AND RADIANS, and the one rounding between them.
 *
 * An angle crosses this repository as a number a caller wrote in degrees
 * and a trigonometric function reads in radians, and every library that
 * carries it has to scale it the same way or the same drawing lands in
 * two places. The conversion is here, below all of them, for the same
 * reason the mixers are: it is arithmetic several libraries agree on to
 * the bit.
 */

/** DEGREES AND RADIANS, converted through one rounding. The constants are
 *  the correctly rounded floats of the exact ratios, and the two verbs
 *  are the only multiplication by them anything in this tree performs, so
 *  a pen, a contour and a shader's CPU twin place the same angle at the
 *  same point. */
namespace sigil::core::angle {

/** Degrees to radians, written as the correctly rounded float of the
 *  exact value and not as a quotient of a rounded pi: pi / 180 computed
 *  from a rounded pi lands an ulp away from the nearest float to the
 *  exact ratio, and an angle scaled by it drifts by that ulp. */
inline constexpr float kDegToRad = 0.017453293f;
/** Radians to degrees, rounded independently for the same reason. */
inline constexpr float kRadToDeg = 57.29578f;

/** Degrees → radians — the constant above as the verb that reads at a
 *  call site: `std::cos(angle::radians(bearing))`.
 *
 *  A pair of one-line functions rather than a note telling every caller
 *  to multiply, because the multiply IS the thing that gets respelled: a
 *  hand-written `degrees * 3.14159f / 180.0f` rounds twice, and a
 *  hand-written `degrees / 57.29578f` is a divide by a rounded
 *  reciprocal, which is a third answer again. These are the one
 *  rounding. */
inline constexpr float radians(float degrees) { return degrees * kDegToRad; }
/** Radians to degrees, through the same single rounding. */
inline constexpr float degrees(float radians) { return radians * kRadToDeg; }

}  // namespace sigil::core::angle
