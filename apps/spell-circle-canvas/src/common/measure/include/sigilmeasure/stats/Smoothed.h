#pragma once

/** @file
 * @ingroup measure-stats
 * A reading that follows a jumpy value slowly: an exponential mean over
 * about the last few values, or over a time constant, and the decaying
 * peak a meter holds.
 */

#include <sigilmeasure/time/Stopwatch.h>

#include <cmath>

namespace sigil::measure {

/** HOW FAST A `Smoothed` FOLLOWS. `add(value)` reads `over`, or `weight`
 *  when `over` is not set; `add(value, elapsed)` reads `timeConstant`
 *  when it is set. */
struct SmoothedOptions {
  /** About how many values the reading remembers: each new value weighs
   *  2 / (over + 1), the exponential mean a frame-rate readout uses. */
  double over = 0;
  /** Or the time the reading takes to cover about two thirds of a step:
   *  each value added with `add(value, elapsed)` weighs
   *  1 − e^(−elapsed / timeConstant), so the reading follows at the same
   *  speed whatever the frame rate. */
  Duration timeConstant{};
  /** Or the new value's weight stated directly, between 0 and 1 — what
   *  keeps a reading that was written as `old · 0.85 + new · 0.15`
   *  exactly the number it was. */
  double weight = 0;
  /** A PEAK rather than a mean: a value above the reading is taken at
   *  once, and the reading falls toward a lower one at the rate above —
   *  the marker an audio meter holds over its bar. */
  bool peak = false;
};

/** A VALUE FOLLOWED SLOWLY. `Smoothed level{12}` is the exponential mean
 *  over about the last 12 values: `add()` each new one and read
 *  `value()`. The first value is taken as it is, so the reading does not
 *  climb up from zero.
 *  @silent no rate set: every value is taken whole, which is no
 *  smoothing at all. */
class Smoothed {
 public:
  /** The mean over about the last @p over values. */
  explicit Smoothed(double over) : m_options{.over = over} {}
  /** A reading that follows as @p options says. */
  explicit Smoothed(SmoothedOptions options = {}) : m_options(options) {}

  /** Folds in @p value as one step of the stream. */
  void add(double value) { fold(value, stepWeight()); }
  /** Folds in @p value, @p elapsed after the value before it. With a
   *  time constant the weight is read off the time that passed; without
   *  one, this is `add(value)`. */
  void add(double value, Duration elapsed) {
    if (m_options.timeConstant > Duration::zero()) {
      const double ratio = elapsed / m_options.timeConstant;
      fold(value, ratio > 0 ? 1.0 - std::exp(-ratio) : 0.0);
      return;
    }
    add(value);
  }
  /** Forgets the reading; the next value is taken as it is. */
  void clear() {
    m_value = 0;
    m_empty = true;
  }

  /** The reading; zero before the first value. */
  [[nodiscard]] double value() const { return m_value; }
  /** Whether no value has been added since construction or `clear()`. */
  [[nodiscard]] bool empty() const { return m_empty; }

 private:
  [[nodiscard]] double stepWeight() const {
    if (m_options.over > 0) return 2.0 / (m_options.over + 1.0);
    if (m_options.weight > 0) return m_options.weight;
    return 1.0;
  }
  void fold(double value, double weight) {
    if (m_empty || (m_options.peak && value > m_value)) {
      m_value = value;
      m_empty = false;
      return;
    }
    m_value += (value - m_value) * weight;
  }

  SmoothedOptions m_options;
  double m_value = 0;
  bool m_empty = true;
};

}  // namespace sigil::measure
