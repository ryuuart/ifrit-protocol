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
 *  are the answer; `explained`, `maxResidual` and `rmsResidual` are what turns
 *  a drawn line into evidence. */
template <std::floating_point T>
struct LineFit {
  T slope = 0;
  T intercept = 0;
  /** How much of the ordinates' spread the line explains, 0 to 1 — the
   *  coefficient of determination. A run whose abscissae are all one
   *  value has no line to fit and answers 0 with a zero slope. */
  T explained = 0;
  /** The largest |y − (a + b·x)| over the points, in y's own units. */
  T maxResidual = 0;
  /** The root mean square of the same residuals. */
  T rmsResidual = 0;
  size_t samples = 0;

  /** HOW STRONGLY THE TWO RUN TOGETHER, -1 to 1: the square root of
   *  `explained` carrying the slope's sign, so how much was explained and the
   *  DIRECTION are read off one number. */
  [[nodiscard]] T correlation() const {
    const T magnitude = std::sqrt(explained);
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
 *  zero slope through the mean, `explained` at 0, residuals still
 *  measured. */
template <std::floating_point T>
[[nodiscard]] LineFit<T> lineFit(std::span<const T> xs, std::span<const T> ys) {
  LineFit<T> fit;
  const size_t count = xs.size() < ys.size() ? xs.size() : ys.size();
  fit.samples = count;
  if (count == 0) return fit;

  T meanX = 0, meanY = 0, spreadX = 0, spreadXY = 0;
  for (size_t index = 0; index < count; ++index) {
    const T seen = (T)(index + 1);
    const T fromMeanX = xs[index] - meanX;
    const T fromMeanY = ys[index] - meanY;
    meanX += fromMeanX / seen;
    meanY += fromMeanY / seen;
    spreadX += fromMeanX * (xs[index] - meanX);
    spreadXY += fromMeanX * (ys[index] - meanY);
  }
  if (count < 2 || !(spreadX > 0)) {
    // Not a line: the flat answer through the mean, and the residuals off
    // that answer, which are the spread of the ordinates and not zero.
    fit.intercept = meanY;
    T squaredResiduals = 0;
    for (size_t index = 0; index < count; ++index) {
      const T residual = ys[index] - fit.intercept;
      squaredResiduals += residual * residual;
      fit.maxResidual = std::max(fit.maxResidual, std::abs(residual));
    }
    fit.rmsResidual = std::sqrt(squaredResiduals / (T)count);
    return fit;
  }
  fit.slope = spreadXY / spreadX;
  fit.intercept = meanY - fit.slope * meanX;

  T squaredResiduals = 0, squaredSpread = 0;
  for (size_t index = 0; index < count; ++index) {
    const T residual = ys[index] - fit.at(xs[index]);
    squaredResiduals += residual * residual;
    squaredSpread += (ys[index] - meanY) * (ys[index] - meanY);
    fit.maxResidual = std::max(fit.maxResidual, std::abs(residual));
  }
  fit.rmsResidual = std::sqrt(squaredResiduals / (T)count);
  // A run that is already one value has no variance to explain, and the
  // fit through it is exact — which reads as 1 rather than as 0/0.
  fit.explained = squaredSpread > 0 ? (T)1 - squaredResiduals / squaredSpread : (T)1;
  return fit;
}

}  // namespace sigil::measure
