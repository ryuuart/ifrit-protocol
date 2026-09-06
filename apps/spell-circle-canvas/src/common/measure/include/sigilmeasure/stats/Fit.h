#pragma once

/** @file
 * The straight line a run of points is closest to, and how far they stand
 * off it — the fit a study reports when it claims one quantity is
 * proportional to another.
 */

#include <cmath>
#include <concepts>
#include <cstddef>
#include <span>

namespace sigil::measure {

/** A LINE FITTED BY LEAST SQUARES, and the residuals that say whether
 *  the claim it makes is worth printing.
 *
 *  The slope and the intercept are the answer; `r2`, `maxResidual` and
 *  `rmsResidual` are what turns a drawn line into evidence. A study that
 *  quotes a slope without a residual has stated a preference, not a
 *  measurement. */
template <std::floating_point T>
struct LineFit {
  T slope = 0;
  T intercept = 0;
  /** The coefficient of determination, 0 to 1. A run whose abscissae are
   *  all one value has no line to fit and answers 0 with a zero slope. */
  T r2 = 0;
  /** The largest |y − (a + b·x)| over the points, in y's own units. */
  T maxResidual = 0;
  /** The root mean square of the same residuals. */
  T rmsResidual = 0;
  size_t samples = 0;

  /** HOW STRONGLY THE TWO RUN TOGETHER, -1 to 1: the correlation, which
   *  is the square root of `r2` carrying the slope's sign. It says the
   *  same thing `r2` says about how much was explained, and one thing
   *  more that a drawing usually wants stated — the DIRECTION, so that
   *  "they move together" and "one falls as the other rises" are told
   *  apart without reading the slope in the ordinate's own units. */
  [[nodiscard]] T correlation() const {
    const T magnitude = std::sqrt(r2);
    return slope < 0 ? -magnitude : magnitude;
  }

  /** The fitted y at @p x — the line, evaluated. */
  [[nodiscard]] constexpr T at(T x) const { return intercept + slope * x; }
  /** How far (@p x, @p y) stands off the line, signed. */
  [[nodiscard]] constexpr T residual(T x, T y) const { return y - at(x); }
};

/** THE FIT OF @p ys AGAINST @p xs.
 *
 *  Ordinary least squares in the ARGUMENT'S OWN precision: the sums are
 *  accumulated in `T`, so a caller that has always fitted in float gets
 *  the float answer it had rather than a double one rounded back. Fit in
 *  double where the answer is the finding and in float where it feeds a
 *  drawing that must not move.
 *
 *  Fewer than two points, or every point at one abscissa, is not a line:
 *  the answer is a zero slope through the mean, with `r2` at 0, which
 *  reads as "nothing was explained" rather than as a divide by zero.
 *
 *  The shorter of the two spans is what is read, so a caller with a
 *  ragged pair does not walk off the end of one of them. */
template <std::floating_point T>
[[nodiscard]] LineFit<T> lineFit(std::span<const T> xs,
                                 std::span<const T> ys) {
  LineFit<T> fit;
  const size_t n = xs.size() < ys.size() ? xs.size() : ys.size();
  fit.samples = n;
  if (n == 0) return fit;

  T sx = 0, sy = 0, sxx = 0, sxy = 0;
  for (size_t i = 0; i < n; ++i) {
    sx += xs[i];
    sy += ys[i];
    sxx += xs[i] * xs[i];
    sxy += xs[i] * ys[i];
  }
  const T nn = (T)n;
  const T den = nn * sxx - sx * sx;
  if (n < 2 || !(std::abs(den) > 0)) {
    fit.intercept = sy / nn;
    for (size_t i = 0; i < n; ++i)
      fit.maxResidual =
          std::max(fit.maxResidual, std::abs(ys[i] - fit.intercept));
    return fit;
  }
  fit.slope = (nn * sxy - sx * sy) / den;
  fit.intercept = (sy - fit.slope * sx) / nn;

  const T mean = sy / nn;
  T ssRes = 0, ssTot = 0;
  for (size_t i = 0; i < n; ++i) {
    const T e = ys[i] - fit.at(xs[i]);
    ssRes += e * e;
    ssTot += (ys[i] - mean) * (ys[i] - mean);
    fit.maxResidual = std::max(fit.maxResidual, std::abs(e));
  }
  fit.rmsResidual = std::sqrt(ssRes / nn);
  // A run that is already one value has no variance to explain, and the
  // fit through it is exact — which reads as 1 rather than as 0/0.
  fit.r2 = ssTot > 0 ? (T)1 - ssRes / ssTot : (T)1;
  return fit;
}

}  // namespace sigil::measure
