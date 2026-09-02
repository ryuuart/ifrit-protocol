#pragma once

/** @file
 * HOW FAR TWO PLATES STAND APART, per colour channel in 0..255 — the one
 * measure every test in this feature compares two rasterisers with.
 *
 * TWO RASTERISERS ARE NOT ASKED TO AGREE BIT FOR BIT, so what is
 * asserted is a distance and not a hash: the mean over every channel of
 * every pixel is what says the two pictures ARE the same picture; the
 * value 99 in a hundred stay under is what says the disagreement is
 * confined; the maximum is an edge, and an edge is where two rasterisers
 * always differ.
 *
 * The engine's own `ComputeImageDifference` answers a different set of
 * numbers and cannot stand in for this: its per-pixel difference is the
 * largest of a pixel's channels rather than each channel on its own, and
 * its average skips every pixel that matched — so two pictures agreeing
 * everywhere but one edge would report that edge's own average rather
 * than a small number over the whole plate, which is the opposite of
 * what a tolerance here is stated in. Its maximum is this one.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace sigil::world::diligent {

/** The mean, the 99th percentile and the worst channel, in 0..255. */
struct Distance {
  double mean = 0;
  int p99 = 0;
  int max = 0;
};

/** @p a against @p b. Two plates of different sizes are as far apart as
 *  two plates can be. */
inline Distance distanceOf(const SkBitmap& a, const SkBitmap& b) {
  Distance out;
  if (a.width() != b.width() || a.height() != b.height()) {
    out.max = 255;
    return out;
  }
  std::vector<int> histogram(256, 0);
  double total = 0;
  size_t count = 0;
  for (int y = 0; y < a.height(); ++y) {
    for (int x = 0; x < a.width(); ++x) {
      const SkColor4f left = a.getColor4f(x, y);
      const SkColor4f right = b.getColor4f(x, y);
      const float channels[4][2] = {{left.fR, right.fR},
                                    {left.fG, right.fG},
                                    {left.fB, right.fB},
                                    {left.fA, right.fA}};
      for (const auto& pair : channels) {
        const int diff = (int)std::lround(std::abs(pair[0] - pair[1]) * 255.0f);
        ++histogram[(size_t)std::clamp(diff, 0, 255)];
        total += diff;
        ++count;
        out.max = std::max(out.max, diff);
      }
    }
  }
  out.mean = count ? total / (double)count : 0.0;
  const size_t cut = (size_t)((double)count * 0.99);
  size_t seen = 0;
  for (int value = 0; value < 256; ++value) {
    seen += (size_t)histogram[(size_t)value];
    if (seen >= cut) {
      out.p99 = value;
      break;
    }
  }
  return out;
}

}  // namespace sigil::world::diligent
