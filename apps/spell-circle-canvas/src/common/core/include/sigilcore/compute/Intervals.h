#pragma once

/** @file
 * Interval arithmetic over a sorted, disjoint NORMAL FORM: normalise,
 * complement, intersect, and the first shared run.
 *
 * Every consumer that answers "where on this thing" in runs arrives at
 * the same four operations, and every hand-rolled copy of them is a pile
 * of special cases about touching endpoints and empty runs. Put the set
 * in normal form once — sorted, merged, nothing degenerate — and the
 * three combinators are one sweep each.
 *
 * The endpoint TYPE is the caller's: a fraction of an arc length, a code
 * unit index, a frame number. `IntervalEnds` says how one is read off
 * whatever value the caller already has, so nothing is copied into a
 * different shape to be operated on.
 *
 * EPSILON is a parameter and not a constant, because "these two runs
 * touch" and "these two runs overlap" are different questions with
 * different answers, and a caller asking the second may want a looser
 * threshold than the one it normalised with. On an integer endpoint it is
 * zero, and every comparison below reads as the exact one.
 */

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

namespace sigil::core {

/** How an interval value says where it starts and ends. Specialise for a
 *  value whose endpoints are named something other than `low` and `high`;
 *  the members must be assignable, since normalising rewrites them. */
template <class Interval>
struct IntervalEnds {
  using Value = decltype(Interval{}.low);
  static Value& low(Interval& i) { return i.low; }
  static Value& high(Interval& i) { return i.high; }
  static const Value& low(const Interval& i) { return i.low; }
  static const Value& high(const Interval& i) { return i.high; }
};

/** What to do with an interval whose end precedes its start. */
enum class Inverted : bool {
  Drop,  ///< it names nothing, so it is not in the answer
  Swap   ///< it names the same run written backwards
};

/** THE NORMAL FORM: clamped to [@p low, @p high], nothing shorter than
 *  @p epsilon, sorted, and neighbours that meet within @p epsilon merged.
 *  Every other operation here takes its inputs in this form and answers
 *  in it. */
template <class Interval, class Ends = IntervalEnds<Interval>>
std::vector<Interval> normalizeIntervals(
    std::vector<Interval> intervals, typename Ends::Value low,
    typename Ends::Value high, typename Ends::Value epsilon = {},
    Inverted inverted = Inverted::Drop) {
  using T = typename Ends::Value;
  std::vector<Interval> kept;
  kept.reserve(intervals.size());
  for (Interval& i : intervals) {
    if (Ends::high(i) < Ends::low(i)) {
      if (inverted == Inverted::Drop) continue;
      std::swap(Ends::low(i), Ends::high(i));
    }
    Ends::low(i) = std::clamp(Ends::low(i), low, high);
    Ends::high(i) = std::clamp(Ends::high(i), low, high);
    if (Ends::high(i) - Ends::low(i) > epsilon) kept.push_back(i);
  }
  std::sort(kept.begin(), kept.end(), [](const Interval& a, const Interval& b) {
    const T& aLow = Ends::low(a);
    const T& bLow = Ends::low(b);
    return aLow != bLow ? aLow < bLow : Ends::high(a) < Ends::high(b);
  });
  std::vector<Interval> merged;
  merged.reserve(kept.size());
  for (const Interval& i : kept) {
    if (!merged.empty() && Ends::low(i) <= Ends::high(merged.back()) + epsilon)
      Ends::high(merged.back()) =
          std::max(Ends::high(merged.back()), Ends::high(i));
    else
      merged.push_back(i);
  }
  return merged;
}

/** Everything in [@p low, @p high] that @p intervals (normalised) does
 *  not cover. */
template <class Interval, class Ends = IntervalEnds<Interval>>
std::vector<Interval> complementIntervals(const std::vector<Interval>& intervals,
                                          typename Ends::Value low,
                                          typename Ends::Value high,
                                          typename Ends::Value epsilon = {}) {
  using T = typename Ends::Value;
  std::vector<Interval> out;
  T at = low;
  for (const Interval& i : intervals) {
    if (Ends::low(i) - at > epsilon) {
      Interval gap{};
      Ends::low(gap) = at;
      Ends::high(gap) = Ends::low(i);
      out.push_back(gap);
    }
    at = std::max(at, Ends::high(i));
  }
  if (high - at > epsilon) {
    Interval gap{};
    Ends::low(gap) = at;
    Ends::high(gap) = high;
    out.push_back(gap);
  }
  return out;
}

/** THE RUNS BOTH SETS COVER. Both inputs are normalised — sorted,
 *  disjoint, nothing degenerate — so one sweep suffices, and two runs
 *  merely meeting at a point share no length and are not an
 *  intersection. */
template <class Interval, class Ends = IntervalEnds<Interval>>
std::vector<Interval> intersectIntervals(const std::vector<Interval>& a,
                                         const std::vector<Interval>& b,
                                         typename Ends::Value epsilon = {}) {
  using T = typename Ends::Value;
  std::vector<Interval> out;
  size_t i = 0, j = 0;
  while (i < a.size() && j < b.size()) {
    const T low = std::max(Ends::low(a[i]), Ends::low(b[j]));
    const T high = std::min(Ends::high(a[i]), Ends::high(b[j]));
    if (high - low > epsilon) {
      Interval shared{};
      Ends::low(shared) = low;
      Ends::high(shared) = high;
      out.push_back(shared);
    }
    if (Ends::high(a[i]) < Ends::high(b[j]))
      ++i;
    else
      ++j;
  }
  return out;
}

/** The first run @p a and @p b share by more than @p epsilon, or nullopt.
 *  A shared END POINT is two runs meeting, not two runs overlapping, and
 *  a caller reporting a conflict usually wants a looser threshold than
 *  the one it normalised with. */
template <class Interval, class Ends = IntervalEnds<Interval>>
std::optional<Interval> firstOverlap(const std::vector<Interval>& a,
                                     const std::vector<Interval>& b,
                                     typename Ends::Value epsilon = {}) {
  using T = typename Ends::Value;
  for (const Interval& x : a)
    for (const Interval& y : b) {
      const T low = std::max(Ends::low(x), Ends::low(y));
      const T high = std::min(Ends::high(x), Ends::high(y));
      if (high - low > epsilon) {
        Interval shared{};
        Ends::low(shared) = low;
        Ends::high(shared) = high;
        return shared;
      }
    }
  return std::nullopt;
}

}  // namespace sigil::core
