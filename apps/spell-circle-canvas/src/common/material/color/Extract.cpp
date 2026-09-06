/** @file
 * The two ways a table is chosen from a run of pixels — the moved means
 * and the divided boxes — both in OKLab, and the nearest-entry read.
 */

#include "sigilmaterial/color/Extract.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace sigil::material {
namespace {

float distanceSquared(const Oklab& a, const Oklab& b) {
  const float dL = a.L - b.L, da = a.a - b.a, db = a.b - b.b;
  return dL * dL + da * da + db * db;
}

/** The pixels that are read: the stride applied and the transparent ones
 *  dropped, in OKLab. */
std::vector<Oklab> readable(std::span<const Color> pixels,
                            const PaletteOptions& options) {
  const size_t step = (size_t)std::max(options.stride, 1);
  std::vector<Oklab> kept;
  kept.reserve(pixels.size() / step + 1);
  for (size_t i = 0; i < pixels.size(); i += step) {
    if (pixels[i].a <= options.minimumAlpha) continue;
    kept.push_back(toOklab(pixels[i]));
  }
  return kept;
}

/** The starting entries: the first pixel, then repeatedly the pixel
 *  farthest from every entry so far. Deterministic, and it lands on the
 *  extremes of the picture rather than in the middle of it. */
std::vector<Oklab> farthestFirst(const std::vector<Oklab>& pixels, int count) {
  std::vector<Oklab> centres;
  if (pixels.empty()) return centres;
  centres.push_back(pixels.front());
  std::vector<float> nearest(pixels.size(), 0.0f);
  for (size_t i = 0; i < pixels.size(); ++i)
    nearest[i] = distanceSquared(pixels[i], centres.front());
  while ((int)centres.size() < count) {
    size_t farthest = 0;
    float best = -1.0f;
    for (size_t i = 0; i < pixels.size(); ++i) {
      if (nearest[i] <= best) continue;
      best = nearest[i];
      farthest = i;
    }
    // Every pixel already sits on an entry: the picture holds fewer
    // distinct colours than the table asked for, and a table that
    // repeated one would be saying the picture holds a colour twice.
    if (!(best > 0.0f)) break;
    centres.push_back(pixels[farthest]);
    for (size_t i = 0; i < pixels.size(); ++i)
      nearest[i] = std::min(nearest[i], distanceSquared(pixels[i], centres.back()));
  }
  return centres;
}

std::vector<Oklab> kmeans(const std::vector<Oklab>& pixels,
                          const PaletteOptions& options) {
  std::vector<Oklab> centres = farthestFirst(pixels, std::max(options.entries, 1));
  if (centres.empty()) return centres;
  const size_t count = centres.size();
  std::vector<double> sumL(count), sumA(count), sumB(count);
  std::vector<size_t> population(count);
  for (int pass = 0; pass < std::max(options.iterations, 1); ++pass) {
    std::fill(sumL.begin(), sumL.end(), 0.0);
    std::fill(sumA.begin(), sumA.end(), 0.0);
    std::fill(sumB.begin(), sumB.end(), 0.0);
    std::fill(population.begin(), population.end(), (size_t)0);
    for (const Oklab& pixel : pixels) {
      size_t best = 0;
      float bestDistance = distanceSquared(pixel, centres[0]);
      for (size_t c = 1; c < count; ++c) {
        const float d = distanceSquared(pixel, centres[c]);
        if (d >= bestDistance) continue;
        bestDistance = d;
        best = c;
      }
      sumL[best] += pixel.L;
      sumA[best] += pixel.a;
      sumB[best] += pixel.b;
      ++population[best];
    }
    bool moved = false;
    for (size_t c = 0; c < count; ++c) {
      // An entry nothing is closest to stays where it is rather than
      // being re-seeded: moving it would make the answer depend on the
      // order the pixels arrived in.
      if (population[c] == 0) continue;
      const Oklab next{(float)(sumL[c] / (double)population[c]),
                       (float)(sumA[c] / (double)population[c]),
                       (float)(sumB[c] / (double)population[c]), 1.0f};
      moved = moved || distanceSquared(next, centres[c]) > 1e-10f;
      centres[c] = next;
    }
    if (!moved) break;
  }
  return centres;
}

std::vector<Oklab> medianCut(std::vector<Oklab> pixels, int entries) {
  struct Box {
    size_t begin = 0, end = 0;
  };
  std::vector<Box> boxes{{0, pixels.size()}};
  const size_t wanted = (size_t)std::max(entries, 1);
  while (boxes.size() < wanted) {
    // The box that spans the most colour is the one worth dividing:
    // dividing the tightest would spend an entry saying the same thing
    // twice.
    size_t widest = boxes.size();
    float widestSpan = 0.0f;
    int widestAxis = 0;
    for (size_t b = 0; b < boxes.size(); ++b) {
      if (boxes[b].end - boxes[b].begin < 2) continue;
      float low[3]{1e30f, 1e30f, 1e30f}, high[3]{-1e30f, -1e30f, -1e30f};
      for (size_t i = boxes[b].begin; i < boxes[b].end; ++i) {
        const float channel[3]{pixels[i].L, pixels[i].a, pixels[i].b};
        for (int axis = 0; axis < 3; ++axis) {
          low[axis] = std::min(low[axis], channel[axis]);
          high[axis] = std::max(high[axis], channel[axis]);
        }
      }
      for (int axis = 0; axis < 3; ++axis) {
        const float span = high[axis] - low[axis];
        if (span <= widestSpan) continue;
        widestSpan = span;
        widest = b;
        widestAxis = axis;
      }
    }
    if (widest == boxes.size() || !(widestSpan > 0.0f)) break;
    const Box box = boxes[widest];
    const size_t middle = box.begin + (box.end - box.begin) / 2;
    std::nth_element(pixels.begin() + (long)box.begin,
                     pixels.begin() + (long)middle,
                     pixels.begin() + (long)box.end,
                     [widestAxis](const Oklab& a, const Oklab& b) {
                       const float ca[3]{a.L, a.a, a.b};
                       const float cb[3]{b.L, b.a, b.b};
                       return ca[widestAxis] < cb[widestAxis];
                     });
    boxes[widest] = {box.begin, middle};
    boxes.push_back({middle, box.end});
  }
  std::vector<Oklab> centres;
  centres.reserve(boxes.size());
  for (const Box& box : boxes) {
    if (box.end <= box.begin) continue;
    double L = 0, a = 0, b = 0;
    for (size_t i = box.begin; i < box.end; ++i) {
      L += pixels[i].L;
      a += pixels[i].a;
      b += pixels[i].b;
    }
    const double population = (double)(box.end - box.begin);
    centres.push_back({(float)(L / population), (float)(a / population),
                       (float)(b / population), 1.0f});
  }
  return centres;
}

}  // namespace

Palette palette(std::span<const Color> pixels, const PaletteOptions& options) {
  const std::vector<Oklab> read = readable(pixels, options);
  std::vector<Oklab> centres = options.method == PaletteMethod::KMeans
                                   ? kmeans(read, options)
                                   : medianCut(read, options.entries);
  if (options.sortByLightness)
    std::sort(centres.begin(), centres.end(),
              [](const Oklab& a, const Oklab& b) { return a.L < b.L; });
  Palette table;
  table.entries.reserve(centres.size());
  for (const Oklab& centre : centres) table.entries.push_back(fromOklab(centre));
  return table;
}

int closestEntry(const Palette& palette, const Color& color) {
  if (palette.entries.empty()) return -1;
  const Oklab target = toOklab(color);
  int best = 0;
  float bestDistance = distanceSquared(target, toOklab(palette.entries[0]));
  for (size_t i = 1; i < palette.entries.size(); ++i) {
    const float d = distanceSquared(target, toOklab(palette.entries[i]));
    if (d >= bestDistance) continue;
    bestDistance = d;
    best = (int)i;
  }
  return best;
}

}  // namespace sigil::material
