#pragma once

/** @file
 * A weighted choice from the pen's random stream.
 */

#include <initializer_list>
#include <optional>
#include <utility>

namespace sigil::draw {
class Pen;
}

namespace sigil::draw::brush {

/** A number in [0, @p total) from the pen's deterministic stream. */
[[nodiscard]] float randomBelow(Pen& pen, float total);

/** Chooses a value in proportion to its positive weight using the pen's
 *  deterministic random stream. An empty or non-positive distribution has
 *  no result. */
template <typename T>
[[nodiscard]] std::optional<T> weightedChoice(
    Pen& pen, std::initializer_list<std::pair<T, float>> weights) {
  float total = 0.0f;
  for (const auto& [unused, weight] : weights)
    if (weight > 0.0f) total += weight;
  if (!(total > 0.0f)) return std::nullopt;

  const float choice = randomBelow(pen, total);
  float cumulative = 0.0f;
  std::optional<T> last;
  for (const auto& [value, weight] : weights) {
    if (!(weight > 0.0f)) continue;
    last = value;
    cumulative += weight;
    if (choice < cumulative) return value;
  }
  return last;
}

}  // namespace sigil::draw::brush
