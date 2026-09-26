#pragma once

/** @file
 * @ingroup measure-stats
 * What a run of numbers amounts to — how many, the mean, the two ends and
 * the spread — in one pass that keeps none of them.
 */

#include <sigilmeasure/advanced/Moments.h>

#include <cstddef>
#include <functional>
#include <ranges>

namespace sigil::measure {

/** HOW MANY, HOW LARGE, AND HOW SPREAD: the count, the sum and mean, the
 *  smallest and largest, and the deviation of a run, fed one value at a
 *  time or read off a run in hand with `summary()`. It keeps none of the
 *  values, so a run of any length costs the same few words, and two
 *  summaries of two halves merge into the summary of the whole.
 *  @trap Nothing that needs the values back can be asked of it — a
 *  quantile is `quantile()` over the run, or a `Window`'s. */
class Summary {
 public:
  /** Folds @p value in. */
  void add(double value) { m_moments.add(value); }
  /** Everything @p other saw, folded in. */
  void merge(const Summary& other) { m_moments.merge(other.m_moments); }
  /** Forgets everything seen so far. */
  void clear() { m_moments.clear(); }

  /** How many values were folded in. */
  [[nodiscard]] std::size_t count() const { return m_moments.count(); }
  /** Whether nothing has been folded in. */
  [[nodiscard]] bool empty() const { return m_moments.empty(); }
  /** The sum of the values. */
  [[nodiscard]] double sum() const { return m_moments.sum(); }
  /** The arithmetic mean; 0 over nothing. */
  [[nodiscard]] double mean() const { return m_moments.mean(); }
  /** The smallest value; 0 over nothing. */
  [[nodiscard]] double min() const { return m_moments.min(); }
  /** The largest value; 0 over nothing. */
  [[nodiscard]] double max() const { return m_moments.max(); }
  /** How far apart the two ends are. */
  [[nodiscard]] double range() const { return m_moments.range(); }
  /** The spread of the values in their own units, the values in hand
   *  taken as the whole of what there is. */
  [[nodiscard]] double deviation() const { return m_moments.deviation(); }

  /** The running moments underneath, for the sample spread and the skew. */
  [[nodiscard]] const Moments& moments() const { return m_moments; }

 private:
  Moments m_moments;
};

/** THE SUMMARY OF A RUN IN HAND. @p projection picks the number out of
 *  each element — `summary(points, &Point::y)`, `summary(sticks,
 *  stretchOf)` — and every number is read as a double. */
template <std::ranges::input_range Range, class Projection = std::identity>
[[nodiscard]] Summary summary(Range&& values, Projection projection = {}) {
  Summary run;
  for (auto&& value : values)
    run.add(static_cast<double>(std::invoke(projection, value)));
  return run;
}

}  // namespace sigil::measure
