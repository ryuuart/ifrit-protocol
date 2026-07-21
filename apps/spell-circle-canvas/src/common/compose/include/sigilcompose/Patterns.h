#pragma once

/** @file
 * SigilCompose stock pattern generators — seeded PatternPrograms over the
 * Pattern seam. Each returns a Pattern: `.material()` fills anything,
 * `.seed(n)` re-rolls the seeded ones, `.rotate()/.scale()` remap without
 * rebaking. The reference-grounded generators (girih tessellation, halftone
 * ramps in the Persona grammar, chrome ramps) build on these primitives.
 */

#include "sigilcompose/Pattern.h"
#include "sigilcompose/Shapes.h" // detail::hashNoise (the seeded noise)

#include <include/core/SkPaint.h>

#include <utility>
#include <vector>

namespace sigil::compose::patterns {

/** Square-grid halftone dots (stagger via a second offset row). */
inline Pattern halftone(float spacing, float radius, SkColor4f color,
                        bool staggered = true) {
  const float s = std::max(spacing, 1.0f);
  const float tileH = staggered ? 2 * s : s;
  return Pattern::tile(
      {s, tileH}, [s, radius, color, staggered](SkCanvas &c, SkSize, uint32_t) {
        SkPaint p;
        p.setAntiAlias(true);
        p.setColor4f(color, nullptr);
        auto dot = [&](float cx, float cy) {
          // Draw with wraparound copies so tile edges stay seamless.
          for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy)
              c.drawCircle(cx + (float)dx * s,
                           cy + (float)dy * (staggered ? 2 * s : s), radius,
                           p);
        };
        dot(s * 0.5f, s * 0.5f);
        if (staggered)
          dot(0.0f, s * 1.5f); // half-cell offset row (and its wrap at x=s)
      });
}

/** Stripes along +x (rotate the Pattern for diagonals — stays seamless). */
inline Pattern stripes(float on, float off, SkColor4f color) {
  const float period = std::max(on + off, 1.0f);
  return Pattern::tile({period, 8}, [on, color](SkCanvas &c, SkSize sz,
                                                uint32_t) {
    SkPaint p;
    p.setColor4f(color, nullptr);
    c.drawRect(SkRect::MakeWH(on, sz.height()), p);
  });
}

/** 2×2 checkerboard. */
inline Pattern checker(float cell, SkColor4f a, SkColor4f b) {
  const float s = std::max(cell, 1.0f);
  return Pattern::tile({2 * s, 2 * s}, [s, a, b](SkCanvas &c, SkSize,
                                                 uint32_t) {
    SkPaint pa, pb;
    pa.setColor4f(a, nullptr);
    pb.setColor4f(b, nullptr);
    c.drawRect(SkRect::MakeWH(s, s), pa);
    c.drawRect(SkRect::MakeXYWH(s, s, s, s), pa);
    c.drawRect(SkRect::MakeXYWH(s, 0, s, s), pb);
    c.drawRect(SkRect::MakeXYWH(0, s, s, s), pb);
  });
}

/** Grid lines (graph/blueprint paper). */
inline Pattern gridLines(float spacing, float width, SkColor4f color) {
  const float s = std::max(spacing, 1.0f);
  return Pattern::tile({s, s}, [width, color](SkCanvas &c, SkSize sz,
                                              uint32_t) {
    SkPaint p;
    p.setColor4f(color, nullptr);
    c.drawRect(SkRect::MakeWH(sz.width(), width), p);
    c.drawRect(SkRect::MakeWH(width, sz.height()), p);
  });
}

/** Seeded speckle (paper grain, star fields): `count` marks per tile with
 *  radii in [rMin, rMax], colors cycled from the palette — deterministic
 *  per seed, `.seed(n)` re-rolls the field. */
inline Pattern speckle(float tileSize, int count, float rMin, float rMax,
                       std::vector<SkColor4f> palette) {
  const float s = std::max(tileSize, 8.0f);
  return Pattern::tile(
      {s, s},
      [s, count, rMin, rMax, palette = std::move(palette)](
          SkCanvas &c, SkSize, uint32_t seed) {
        SkPaint p;
        p.setAntiAlias(true);
        for (int i = 0; i < count; ++i) {
          const uint32_t k = (uint32_t)i;
          const float x =
              (0.5f + 0.5f * shapes::detail::hashNoise(seed, 3 * k)) * s;
          const float y =
              (0.5f + 0.5f * shapes::detail::hashNoise(seed, 3 * k + 1)) * s;
          const float t =
              0.5f + 0.5f * shapes::detail::hashNoise(seed, 3 * k + 2);
          const float r = rMin + (rMax - rMin) * t;
          if (!palette.empty())
            p.setColor4f(palette[k % palette.size()], nullptr);
          // Wraparound copies keep edges seamless.
          for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy)
              c.drawCircle(x + (float)dx * s, y + (float)dy * s, r, p);
        }
      });
}

} // namespace sigil::compose::patterns
