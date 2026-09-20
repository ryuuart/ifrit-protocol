#pragma once

/** @file
 * @ingroup measure-stats
 * The straight-line map that puts a run of numbers on a common footing,
 * DERIVED FROM THE NUMBERS THEMSELVES — a z-score, or the run squeezed
 * into a chosen span.
 */

#include <sigilmeasure/stats/Moments.h>

#include <span>

namespace sigil::measure {

/** A STRAIGHT-LINE MAP, AND ITS INVERSE. Three numbers rather than a
 *  lambda, so it is a value that can be stored, compared and inverted.
 *  @trap IT IS A STATISTIC, NOT A DRAWING'S SCALE: what is here is
 *  DERIVED from a run of numbers, and deriving an axis from the data is
 *  what makes it move whenever a new point arrives. */
struct Rescale {
  /** Subtracted from the value first: where the run's own zero is. */
  double centre = 0.0;
  /** What the centred value is multiplied by: how the run's own width
   *  becomes the answer's. */
  double scale = 1.0;
  /** Added last: where the answer's zero is. */
  double origin = 0.0;

  /** Where @p value lands under the map. */
  [[nodiscard]] double operator()(double value) const {
    return origin + (value - centre) * scale;
  }
  /** The value that maps to @p mapped. A map that collapsed the run to a
   *  point cannot be undone and answers the centre. */
  [[nodiscard]] double invert(double mapped) const {
    return scale != 0.0 ? (mapped - origin) / scale + centre : centre;
  }
  friend bool operator==(const Rescale&, const Rescale&) = default;
};

/** HOW FAR EACH VALUE STANDS FROM THE MEAN, IN DEVIATIONS — what makes
 *  two runs in different units comparable at all. The deviation is the
 *  whole population's, not a sample estimate's, because the run being
 *  standardised is the run in hand.
 *  @silent a run with no spread: it has no deviations to count and maps
 *  every value to zero. */
[[nodiscard]] inline Rescale zScore(std::span<const double> values) {
  const Moments moments = Moments::of(values);
  const double sd = moments.sd();
  return {moments.mean(), sd > 0.0 ? 1.0 / sd : 0.0, 0.0};
}

/** THE RUN SQUEEZED INTO [@p low, @p high], its smallest value at the
 *  low end and its largest at the high one.
 *  @trap Sensitive to a single outlier by construction — one wild value
 *  pushes everything else into a corner — so read `Moments::min` and
 *  `max` beside this when the squeeze looks wrong. */
[[nodiscard]] inline Rescale unitRange(std::span<const double> values,
                                       double low = 0.0, double high = 1.0) {
  const Moments moments = Moments::of(values);
  const double width = moments.max() - moments.min();
  return {moments.min(), width > 0.0 ? (high - low) / width : 0.0, low};
}

}  // namespace sigil::measure
