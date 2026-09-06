#pragma once

/** @file
 * THE SHAPED CURVE A BINDING, A TRANSITION AND A KEYED STEP READ — the
 * comparable curve value SigilCore's compute leaf owns, named here under
 * the word an animation reaches for it by.
 */

#include <sigilcore/compute/Curve.h>

namespace sigil::motion::ease {

/** A CURVE THAT CARRIES ITS OWN PARAMETERS, and can still be compared.
 *
 *  A `choreograph::EaseFn` is a `std::function<float(float)>`, and the
 *  only thing that can be read back out of one is a plain function
 *  pointer. So a parameterless curve compares (both are the same
 *  pointer), and a curve built by binding a shape parameter into a lambda
 *  compares equal to NOTHING — including to an identical one built a
 *  frame later. Every value that holds a curve reads the same rule, so
 *  such a curve makes its whole record incomparable: the node that holds
 *  it never prunes, and worse, a comparator that decides two records are
 *  unequal on the curve alone re-patches for as long as the curve exists.
 *
 *  This is the shape and the numbers kept side by side instead — the
 *  colour leaf's ramp and a keyed track walk the same value, which is why
 *  it lives below every one of them. Every parameterised curve named
 *  below hands one back, and `easeEqual` reads it. */
using Curve = core::curve::Curve;

/** THE HOUSE CURVES. `outBack` and `inBack` overshoot the cubic way,
 *  `inOutBack` does it at both ends, `outElastic` and `inElastic` ring
 *  down under a decaying sine, `outBounce` lands in parabolic rebounds,
 *  `cubicBezier` is a CSS timing function spelled as it was written, and
 *  `smoothstep` is the plain Hermite S with no parameters at all.
 *
 *  Each shaped one is a FACTORY handing back a `Curve` rather than a
 *  lambda, so two calls with the same argument compare EQUAL and the
 *  value holding one prunes:
 *
 *      .scale(animate(from(0.86f).to(1.0f), {520ms, ease::outBack()}))
 *
 *  A caller's own curve fits the same way — write the body as a
 *  captureless lambda over the parameter block. */
using core::curve::cubicBezier;
using core::curve::inBack;
using core::curve::inElastic;
using core::curve::inOutBack;
using core::curve::outBack;
using core::curve::outBounce;
using core::curve::outElastic;
using core::curve::smoothstep;

}  // namespace sigil::motion::ease
