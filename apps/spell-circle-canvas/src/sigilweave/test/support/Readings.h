#pragma once

/** @file
 * A reading taken off a set of measurements, wherever they were measured.
 */

#include <algorithm>
#include <vector>

namespace sigil::weave::test {

/// How far apart the largest and the smallest of `values` stand — the one
/// number an evenness claim is made against, whether the values are line
/// widths or digit advances. Zero for fewer than two values.
inline float spread(const std::vector<float>& values) {
  if (values.size() < 2) return 0.0f;
  const auto [low, high] = std::minmax_element(values.begin(), values.end());
  return *high - *low;
}

}  // namespace sigil::weave::test
