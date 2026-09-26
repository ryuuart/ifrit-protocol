#pragma once

/** @file
 * @ingroup measure-stats
 * Several quantiles of one run for one sort, and the median under its own
 * name.
 */

#include <sigilmeasure/stats/Quantile.h>

#include <cstddef>
#include <functional>
#include <ranges>
#include <span>
#include <vector>

namespace sigil::measure {

/** SEVERAL QUANTILES OF ONE RUN, for ONE sort. `quantile()` sorts a copy
 *  every time it is called, so a summary that reports a median and two
 *  tails sorts the same numbers three times; this sorts once and reads
 *  every fraction off the one order. Each answer is the one `quantile()`
 *  would have given, and the answers come back in the order the fractions
 *  were asked for rather than in sorted order. */
template <std::ranges::input_range Range, class Projection = std::identity>
[[nodiscard]] auto quantiles(Range&& values, std::span<const double> fractions,
                             Projection projection = {}) {
  const auto sorted = detail::sortedReadings(values, projection);
  using Reading = typename decltype(sorted)::value_type;
  std::vector<Reading> answers(fractions.size(), Reading{});
  if (sorted.empty()) return answers;
  for (std::size_t index = 0; index < fractions.size(); ++index)
    answers[index] = detail::interpolate(std::span<const Reading>(sorted),
                                         fractions[index]);
  return answers;
}

/** The middle of the run: `quantile(values, 0.5)`, under the name a
 *  reader of the caption will use. */
template <std::ranges::input_range Range, class Projection = std::identity>
[[nodiscard]] auto median(Range&& values, Projection projection = {}) {
  return quantile(values, 0.5, projection);
}

}  // namespace sigil::measure
