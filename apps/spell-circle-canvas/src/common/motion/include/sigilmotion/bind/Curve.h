#pragma once

/** @file
 * A SHAPED EASING CURVE AS A COMPARABLE VALUE — the shape plus the
 * numbers that shape it, so a curve carrying parameters can still be
 * proved the same curve.
 */

#include <choreograph/Choreograph.h>

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
 *  This is the shape and the numbers kept side by side instead. The shape
 *  is a captureless function — a pointer, therefore comparable — and the
 *  numbers are four floats beside it, so two curves are equal when they
 *  are the same shape at the same settings. Every parameterised curve in
 *  `ease::` hands one back, and `easeEqual` reads it.
 *
 *  A CALLER'S OWN CURVE fits the same way: write the body as a
 *  captureless lambda over the parameter block and it is comparable for
 *  free. A body that must capture is a lambda again, and unequal to
 *  everything — which is correct, since nothing can prove two of them
 *  alike. */
struct Curve {
  /** The shape, as a captureless function over the parameter block: a
   *  pointer, so two of them can be compared. Null answers `t` unchanged,
   *  which is the identity ramp. */
  float (*shape)(float t, const float* parameters) = nullptr;
  /** What the shape reads. Four because the widest house curve — a cubic
   *  Bezier's two control points — takes four; the rest leave the tail at
   *  zero, and a zero is as comparable as any other number. */
  float parameters[4]{};

  float operator()(float t) const { return shape ? shape(t, parameters) : t; }
  bool operator==(const Curve&) const = default;
};

}  // namespace sigil::motion::ease
