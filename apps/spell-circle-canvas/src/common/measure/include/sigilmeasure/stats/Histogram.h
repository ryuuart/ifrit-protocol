#pragma once

/** @file
 * @ingroup measure-stats
 * How a run of numbers is distributed across the range it covers: equal
 * bins, what fell in each, and what fell outside.
 */

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <span>
#include <vector>

namespace sigil::measure {

/** EQUAL BINS ACROSS A RANGE, AND WHAT FELL IN EACH — what SHAPE a
 *  run's spread has, where `Moments` says only how wide it is. Counts
 *  are weights rather than integers, and an unweighted `add`
 *  contributes one.
 *  @trap WHAT FALLS OUTSIDE IS COUNTED, NOT DROPPED and not clamped: a
 *  value past either edge goes to `below()` or `above()`, so `total()`
 *  is the weight INSIDE and not everything added. */
class Histogram {
 public:
  /** @p bins equal bins spanning [@p low, @p high). A count of zero is
   *  one bin. A range that is empty or backwards collapses to [low,
   *  low]: only `low` itself is inside it, and it falls in the first
   *  bin — never a divide by a width of nothing. */
  Histogram(double low, double high, size_t bins)
      : m_low(low),
        m_high(high > low ? high : low),
        m_counts(bins > 0 ? bins : 1, 0.0) {}

  /** THE HISTOGRAM OF A RUN ALREADY IN HAND, over the range the run
   *  itself covers: the first look at data whose extent is not known in
   *  advance. `below()` and `above()` are zero for one of these,
   *  nothing falling outside a range taken from the values. */
  [[nodiscard]] static Histogram over(std::span<const double> values,
                                      size_t bins) {
    if (values.empty()) return Histogram(0.0, 0.0, bins);
    const auto [low, high] = std::minmax_element(values.begin(), values.end());
    Histogram histogram(*low, *high, bins);
    for (double value : values) histogram.add(value);
    return histogram;
  }

  /** Adds @p weight to the bin @p value falls in, or to the count
   *  outside the range when it falls beyond an edge. */
  void add(double value, double weight = 1.0) {
    if (value < m_low) {
      m_below += weight;
      return;
    }
    if (value > m_high) {
      m_above += weight;
      return;
    }
    m_counts[binOf(value)] += weight;
    m_inside += weight;
  }

  /** Empties every bin, keeping the range and the bin count. */
  void clear() {
    std::fill(m_counts.begin(), m_counts.end(), 0.0);
    m_inside = m_below = m_above = 0.0;
  }

  /** WHICH BIN @p value FALLS IN. The bins are half-open — a value on a
   *  boundary belongs to the bin above it — except at the top, where
   *  the high edge belongs to the last bin rather than to nothing. */
  [[nodiscard]] size_t binOf(double value) const {
    if (!(m_high > m_low)) return 0;
    const double where =
        (value - m_low) / (m_high - m_low) * (double)m_counts.size();
    if (!(where > 0.0)) return 0;
    const auto bin = (size_t)where;
    return bin < m_counts.size() ? bin : m_counts.size() - 1;
  }

  /** How many bins the range is cut into. */
  [[nodiscard]] size_t bins() const { return m_counts.size(); }
  /** The low edge of the range. */
  [[nodiscard]] double low() const { return m_low; }
  /** The high edge of the range. */
  [[nodiscard]] double high() const { return m_high; }
  /** How wide one bin is. */
  [[nodiscard]] double binWidth() const {
    return (m_high - m_low) / (double)m_counts.size();
  }
  /** The left edge of bin @p bin; `edge(bins())` is the high end, so the
   *  edges read as one run of `bins() + 1` values. */
  [[nodiscard]] double edge(size_t bin) const {
    return m_low + binWidth() * (double)bin;
  }
  /** The middle of bin @p bin — where a bar is centred and a point is
   *  plotted. */
  [[nodiscard]] double centre(size_t bin) const {
    return edge(bin) + binWidth() * 0.5;
  }

  /** The weight in every bin, low edge first. */
  [[nodiscard]] std::span<const double> counts() const { return m_counts; }
  /** The weight in bin @p bin; 0 for a bin that is not there. */
  [[nodiscard]] double count(size_t bin) const {
    return bin < m_counts.size() ? m_counts[bin] : 0.0;
  }
  /** The weight that landed in a bin, out of the weight that landed in
   *  any of them: the bars a stacked-to-one drawing wants. */
  [[nodiscard]] double fraction(size_t bin) const {
    return m_inside > 0.0 ? count(bin) / m_inside : 0.0;
  }
  /** The weight per unit of the value's own axis, which is what makes
   *  two histograms of different bin counts comparable: the bars sum to
   *  one when multiplied by the bin width, whatever the bin count. */
  [[nodiscard]] double density(size_t bin) const {
    const double width = binWidth();
    return width > 0.0 ? fraction(bin) / width : 0.0;
  }

  /** The weight that landed in ANY bin: everything added but what
   *  `below()` and `above()` hold. It is what `fraction()` divides by. */
  [[nodiscard]] double total() const { return m_inside; }
  /** The weight that fell below the low edge, and above the high one. */
  [[nodiscard]] double below() const { return m_below; }
  /** The weight that fell above the high edge. */
  [[nodiscard]] double above() const { return m_above; }
  /** The heaviest bin, and how heavy it is. The first of a tie. */
  [[nodiscard]] size_t mode() const {
    return (size_t)std::distance(
        m_counts.begin(), std::max_element(m_counts.begin(), m_counts.end()));
  }
  /** The weight in the heaviest bin. */
  [[nodiscard]] double peak() const { return count(mode()); }

 private:
  double m_low = 0.0;
  double m_high = 0.0;
  std::vector<double> m_counts;
  double m_inside = 0.0;
  double m_below = 0.0;
  double m_above = 0.0;
};

}  // namespace sigil::measure
