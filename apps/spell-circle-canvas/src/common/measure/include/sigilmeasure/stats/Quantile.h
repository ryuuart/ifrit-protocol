#pragma once

/** @file
 * @ingroup measure-stats
 * The one quantile every reading in the library shares: the value at a
 * fraction of a sorted run, interpolated between the two ranks it falls
 * between.
 */

#include <algorithm>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <functional>
#include <ranges>
#include <span>
#include <type_traits>
#include <vector>

namespace sigil::measure {

namespace detail {

template <class Value>
struct ReadingType {
  using type = double;
};
template <class Representation, class Period>
struct ReadingType<std::chrono::duration<Representation, Period>> {
  using type = std::chrono::duration<double, Period>;
};

template <class Value>
struct IsDuration : std::false_type {};
template <class Representation, class Period>
struct IsDuration<std::chrono::duration<Representation, Period>>
    : std::true_type {};

}  // namespace detail

/** WHAT AN INSTRUMENT CAN READ: a number, or a `std::chrono` span. */
template <class Value>
concept Measurable = std::is_arithmetic_v<Value> ||
                     detail::IsDuration<std::remove_cv_t<Value>>::value;

/** What a mean or a quantile of @p Value answers in: a double for a
 *  number, and the same unit held as a double for a span of time, so a
 *  mean of whole milliseconds is not rounded back to one. */
template <Measurable Value>
using ReadingOf = typename detail::ReadingType<std::remove_cv_t<Value>>::type;

namespace detail {

/** The interpolation itself, over a run that is ALREADY SORTED and not
 *  empty: the rank @p fraction falls at, and the linear blend of the two
 *  values it falls between. Every quantile in the library reads through
 *  this one body, so the single-fraction call and the several-fractions
 *  call cannot drift apart in what "the median" means. */
template <class Reading>
[[nodiscard]] Reading interpolate(std::span<const Reading> sorted,
                                  double fraction) {
  fraction = std::clamp(fraction, 0.0, 1.0);
  const double rank = fraction * (double)(sorted.size() - 1);
  const auto below = (std::size_t)rank;
  const std::size_t above = std::min(below + 1, sorted.size() - 1);
  const double along = rank - (double)below;
  return sorted[below] + (sorted[above] - sorted[below]) * along;
}

/** @p values projected and converted to what they are read in, sorted. */
template <class Range, class Projection>
[[nodiscard]] auto sortedReadings(Range&& values, Projection& projection) {
  using Value = std::remove_cvref_t<std::invoke_result_t<
      Projection&, std::ranges::range_reference_t<Range>>>;
  using Reading = measure::ReadingOf<Value>;
  std::vector<Reading> sorted;
  if constexpr (std::ranges::sized_range<Range>)
    sorted.reserve(std::ranges::size(values));
  for (auto&& value : values)
    sorted.push_back(Reading(std::invoke(projection, value)));
  std::sort(sorted.begin(), sorted.end());
  return sorted;
}

}  // namespace detail

/** THE VALUE AT FRACTION @p fraction OF THE SORTED RUN, interpolated
 *  linearly between the two ranks it falls between: the 0.5 quantile of
 *  {1, 2, 3, 4} is 2.5, not 2 or 3, and 0.99 is the tail a frame-time
 *  gate reads. @p projection picks the number out of each element — a
 *  member pointer or a callable. A span of time answers a span of time.
 *  Sorts a copy — the caller's order is untouched.
 *  @silent an empty run reads zero; a single value reads itself at every
 *  fraction; the fraction is clamped to [0, 1]. */
template <std::ranges::input_range Range, class Projection = std::identity>
[[nodiscard]] auto quantile(Range&& values, double fraction,
                            Projection projection = {}) {
  const auto sorted = detail::sortedReadings(values, projection);
  using Reading = typename decltype(sorted)::value_type;
  if (sorted.empty()) return Reading{};
  return detail::interpolate(std::span<const Reading>(sorted), fraction);
}

}  // namespace sigil::measure
