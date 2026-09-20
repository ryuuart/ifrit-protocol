#pragma once

/** @file
 * @ingroup measure-stats
 * The straight line a run of points is closest to, and how far they stand
 * off it — the fit a study reports when it claims one quantity is
 * proportional to another.
 */

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <span>

namespace sigil::measure {

/** A LINE FITTED BY LEAST SQUARES, and the residuals that say whether
 *  the claim it makes is worth printing. The slope and the intercept
 *  are the answer; `r2`, `maxResidual` and `rmsResidual` are what turns
 *  a drawn line into evidence. */
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

  /** HOW STRONGLY THE TWO RUN TOGETHER, -1 to 1: the square root of
   *  `r2` carrying the slope's sign, so how much was explained and the
   *  DIRECTION are read off one number. */
  [[nodiscard]] T correlation() const {
    const T magnitude = std::sqrt(r2);
    return slope < 0 ? -magnitude : magnitude;
  }

  /** The fitted y at @p x — the line, evaluated. */
  [[nodiscard]] constexpr T at(T x) const { return intercept + slope * x; }
  /** How far (@p x, @p y) stands off the line, signed. */
  [[nodiscard]] constexpr T residual(T x, T y) const { return y - at(x); }
};

/** THE FIT OF @p ys AGAINST @p xs: ordinary least squares in the
 *  ARGUMENT'S OWN precision, the sums accumulated in `T`, reading the
 *  shorter of the two spans. The spread of the abscissae is ACCUMULATED
 *  and not subtracted, so large, close abscissae keep theirs.
 *  @silent fewer than two points, or every point at one abscissa: a
 *  zero slope through the mean, `r2` at 0, residuals still measured. */
template <std::floating_point T>
[[nodiscard]] LineFit<T> lineFit(std::span<const T> xs, std::span<const T> ys) {
  LineFit<T> fit;
  const size_t n = xs.size() < ys.size() ? xs.size() : ys.size();
  fit.samples = n;
  if (n == 0) return fit;

  T mx = 0, my = 0, sxx = 0, sxy = 0;
  for (size_t i = 0; i < n; ++i) {
    const T count = (T)(i + 1);
    const T dx = xs[i] - mx;
    const T dy = ys[i] - my;
    mx += dx / count;
    my += dy / count;
    sxx += dx * (xs[i] - mx);
    sxy += dx * (ys[i] - my);
  }
  if (n < 2 || !(sxx > 0)) {
    // Not a line: the flat answer through the mean, and the residuals off
    // that answer, which are the spread of the ordinates and not zero.
    fit.intercept = my;
    T ssRes = 0;
    for (size_t i = 0; i < n; ++i) {
      const T e = ys[i] - fit.intercept;
      ssRes += e * e;
      fit.maxResidual = std::max(fit.maxResidual, std::abs(e));
    }
    fit.rmsResidual = std::sqrt(ssRes / (T)n);
    return fit;
  }
  fit.slope = sxy / sxx;
  fit.intercept = my - fit.slope * mx;

  T ssRes = 0, ssTot = 0;
  for (size_t i = 0; i < n; ++i) {
    const T e = ys[i] - fit.at(xs[i]);
    ssRes += e * e;
    ssTot += (ys[i] - my) * (ys[i] - my);
    fit.maxResidual = std::max(fit.maxResidual, std::abs(e));
  }
  fit.rmsResidual = std::sqrt(ssRes / (T)n);
  // A run that is already one value has no variance to explain, and the
  // fit through it is exact — which reads as 1 rather than as 0/0.
  fit.r2 = ssTot > 0 ? (T)1 - ssRes / ssTot : (T)1;
  return fit;
}

}  // namespace sigil::measure
