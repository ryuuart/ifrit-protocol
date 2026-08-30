#pragma once

/** @file
 * A rolling ring of samples with the summary statistics a frame-time HUD
 * reads, and the one quantile definition every ring and every one-off
 * sample list shares.
 */

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

namespace sigil::measure {

/** The value at fraction @p p of the sorted samples, interpolated
 *  linearly between the two ranks it falls between: `quantile(s, 0.5)` of
 *  {1, 2, 3, 4} is 2.5, not 2 or 3. An empty list reads 0, a single
 *  sample reads itself at every @p p, and @p p is clamped to [0, 1].
 *  Sorts a copy — the caller's order is untouched. */
inline double quantile(std::span<const double> samples, double p) {
  if (samples.empty()) return 0.0;
  std::vector<double> sorted(samples.begin(), samples.end());
  std::sort(sorted.begin(), sorted.end());
  p = std::clamp(p, 0.0, 1.0);
  const double rank = p * (double)(sorted.size() - 1);
  const size_t lo = (size_t)rank;
  const size_t hi = std::min(lo + 1, sorted.size() - 1);
  const double t = rank - (double)lo;
  return sorted[lo] + (sorted[hi] - sorted[lo]) * t;
}

/** The last `capacity` samples, oldest dropping first. The summaries
 *  read every sample the ring holds; none is cached, so a ring that is
 *  read every frame costs a pass over its contents each time, and a
 *  percentile costs a sort. Sized for a HUD, not for a histogram. */
class Samples {
 public:
  explicit Samples(size_t capacity = 120)
      : m_samples(capacity > 0 ? capacity : 1) {}

  void add(double sample) {
    m_samples[m_next] = sample;
    m_next = (m_next + 1) % m_samples.size();
    if (m_count < m_samples.size()) ++m_count;
  }
  void clear() { m_count = m_next = 0; }

  size_t size() const { return m_count; }
  size_t capacity() const { return m_samples.size(); }
  bool empty() const { return m_count == 0; }

  /** Arithmetic mean; 0 when empty. */
  double mean() const {
    if (m_count == 0) return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < m_count; ++i) sum += m_samples[i];
    return sum / (double)m_count;
  }
  /** `quantile()` over the samples held. */
  double percentile(double p) const {
    return quantile(std::span<const double>(m_samples.data(), m_count), p);
  }
  double min() const {
    return m_count == 0 ? 0.0
                        : *std::min_element(m_samples.begin(),
                                            m_samples.begin() + (long)m_count);
  }
  double max() const {
    return m_count == 0 ? 0.0
                        : *std::max_element(m_samples.begin(),
                                            m_samples.begin() + (long)m_count);
  }
  /** The most recently added sample; 0 when empty. */
  double last() const {
    if (m_count == 0) return 0.0;
    return m_samples[(m_next + m_samples.size() - 1) % m_samples.size()];
  }
  /** The samples oldest first, as a copy. */
  std::vector<double> samples() const {
    std::vector<double> out;
    out.reserve(m_count);
    const size_t oldest =
        m_count < m_samples.size() ? 0 : m_next;  // ring is full
    for (size_t i = 0; i < m_count; ++i)
      out.push_back(m_samples[(oldest + i) % m_samples.size()]);
    return out;
  }

 private:
  std::vector<double> m_samples;  // the ring; only the first m_count are live
  size_t m_next = 0;              // where the next sample lands
  size_t m_count = 0;
};

}  // namespace sigil::measure
