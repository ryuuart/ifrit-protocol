#pragma once

/** @file
 * The summary of a run of numbers that costs one pass and holds no
 * copies: how many, how large, how spread, how lopsided, and the two
 * ends.
 */

#include <cmath>
#include <cstddef>
#include <limits>
#include <span>

namespace sigil::measure {

/** WHAT A RUN OF NUMBERS AMOUNTS TO, accumulated one at a time.
 *
 *  `Samples` keeps the numbers and computes on read; this keeps none of
 *  them and computes as they arrive, so it summarises a run of any
 *  length in a fixed six words — a sweep of a million values, a per-frame
 *  count that never ends. What it cannot do is anything that needs the
 *  values back: a quantile, a histogram, a re-read after a wider window
 *  was wanted. Reach for `Samples` or a `Histogram` for those.
 *
 *  THE SPREAD IS ACCUMULATED, NOT SUBTRACTED. The obvious variance —
 *  the mean of the squares less the square of the mean — is two large
 *  numbers differing in their last digits, and for values that are large
 *  and close together (a run of timestamps, a run of pixel coordinates
 *  on a wide sheet) the difference is nearly all rounding, and can come
 *  out NEGATIVE. Clamping that at zero hides the loss and reports a
 *  spread of none where there was a real one. Here each value's
 *  deviation from the mean so far is folded in as it arrives, so the sum
 *  is of small numbers and never goes negative. */
class Moments {
 public:
  Moments() = default;

  /** The moments of a run already in hand. */
  [[nodiscard]] static Moments of(std::span<const double> values) {
    Moments moments;
    for (double value : values) moments.add(value);
    return moments;
  }

  void add(double value) {
    const double previous = (double)m_count;
    m_count += 1;
    const double n = (double)m_count;
    const double delta = value - m_mean;
    const double deltaOverN = delta / n;
    const double weighted = delta * deltaOverN * previous;
    m_mean += deltaOverN;
    m_third += weighted * deltaOverN * (n - 2.0) - 3.0 * deltaOverN * m_second;
    m_second += weighted;
    if (value < m_min) m_min = value;
    if (value > m_max) m_max = value;
  }

  void clear() { *this = Moments(); }

  /** EVERYTHING @p other SAW, FOLDED IN. Two halves of a run summarised
   *  separately amount to the same numbers as the whole run in one pass,
   *  which is what lets a sweep be divided across workers and the parts
   *  put back together. */
  void merge(const Moments& other) {
    if (other.m_count == 0) return;
    if (m_count == 0) {
      *this = other;
      return;
    }
    const double a = (double)m_count, b = (double)other.m_count;
    const double n = a + b;
    const double delta = other.m_mean - m_mean;
    const double third = m_third + other.m_third +
                         delta * delta * delta * a * b * (a - b) / (n * n) +
                         3.0 * delta * (a * other.m_second - b * m_second) / n;
    const double second =
        m_second + other.m_second + delta * delta * a * b / n;
    m_mean += delta * b / n;
    m_second = second;
    m_third = third;
    m_count += other.m_count;
    if (other.m_min < m_min) m_min = other.m_min;
    if (other.m_max > m_max) m_max = other.m_max;
  }

  [[nodiscard]] size_t count() const { return m_count; }
  [[nodiscard]] bool empty() const { return m_count == 0; }
  /** The arithmetic mean; 0 over nothing. */
  [[nodiscard]] double mean() const { return m_mean; }
  /** The sum, recovered from the mean rather than carried beside it. */
  [[nodiscard]] double sum() const { return m_mean * (double)m_count; }

  /** THE SPREAD OF THE RUN AS THE WHOLE POPULATION: the mean squared
   *  deviation. Take this when the values in hand are everything there
   *  is — every frame of a recording, every point of a drawing. */
  [[nodiscard]] double variance() const {
    return m_count > 0 ? m_second / (double)m_count : 0.0;
  }
  /** THE SPREAD OF WHAT THE RUN WAS DRAWN FROM: Bessel's correction,
   *  dividing by one less. Take this when the values are a SAMPLE of
   *  something larger and the answer is a claim about that larger thing.
   *  Fewer than two values make no such claim and answer 0. */
  [[nodiscard]] double sampleVariance() const {
    return m_count > 1 ? m_second / (double)(m_count - 1) : 0.0;
  }
  [[nodiscard]] double sd() const { return std::sqrt(variance()); }
  [[nodiscard]] double sampleSd() const { return std::sqrt(sampleVariance()); }

  /** HOW LOPSIDED THE RUN IS: zero for anything symmetric about its
   *  mean, positive when the long tail runs high, negative when it runs
   *  low. What separates a frame time that is steady with occasional
   *  spikes from one that is simply slow. A run with no spread at all
   *  has no shape to report and answers 0. */
  [[nodiscard]] double skewness() const {
    if (m_count < 2 || !(m_second > 0.0)) return 0.0;
    const double n = (double)m_count;
    return std::sqrt(n) * m_third / std::pow(m_second, 1.5);
  }

  /** The smallest and largest seen; 0 over nothing, so that an empty
   *  summary reads as empty rather than as an infinite span. */
  [[nodiscard]] double min() const { return m_count > 0 ? m_min : 0.0; }
  [[nodiscard]] double max() const { return m_count > 0 ? m_max : 0.0; }
  /** How far apart the two ends are. */
  [[nodiscard]] double range() const { return max() - min(); }

 private:
  size_t m_count = 0;
  double m_mean = 0.0;
  /** The accumulated squared deviation — variance times the count. */
  double m_second = 0.0;
  /** The accumulated cubed deviation, which skewness is read off. */
  double m_third = 0.0;
  double m_min = std::numeric_limits<double>::infinity();
  double m_max = -std::numeric_limits<double>::infinity();
};

}  // namespace sigil::measure
