/** @file
 * The stock tile programs: each draws one seamless tile with wraparound
 * copies where a mark crosses the edge.
 */

#include "sigilmaterial/pattern/Patterns.h"

#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>

#include <algorithm>
#include <cmath>

#include "sigilgeometry/path/Noise.h"  // noise::hash (the seeded noise)

namespace sigil::material::pattern {

namespace {
SkColor4f sk(Color c) { return {c.r, c.g, c.b, c.a}; }
}  // namespace

Tile halftone(float spacing, float radius, Color color, bool staggered) {
  const float s = std::max(spacing, 1.0f);
  const float tileH = staggered ? 2 * s : s;
  return Tile::of(
      {s, tileH}, [s, radius, color, staggered](SkCanvas& c, SkSize, uint32_t) {
        SkPaint p;
        p.setAntiAlias(true);
        p.setColor4f(sk(color), nullptr);
        auto dot = [&](float cx, float cy) {
          // Draw with wraparound copies so tile edges stay seamless.
          for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy)
              c.drawCircle(cx + (float)dx * s,
                           cy + (float)dy * (staggered ? 2 * s : s), radius, p);
        };
        dot(s * 0.5f, s * 0.5f);
        if (staggered) dot(0.0f, s * 1.5f);  // half-cell offset row
      });
}

Tile stripes(float on, float off, Color color) {
  const float period = std::max(on + off, 1.0f);
  return Tile::of({period, 8}, [on, color](SkCanvas& c, SkSize sz, uint32_t) {
    SkPaint p;
    p.setColor4f(sk(color), nullptr);
    c.drawRect(SkRect::MakeWH(on, sz.height()), p);
  });
}

Tile sequence(std::vector<std::pair<float, Color>> runs, float phase) {
  float period = 0;
  for (const auto& [w, c] : runs) period += std::max(w, 0.0f);
  if (period <= 0) return stripes(1, 0, {0, 0, 0, 0});  // draws nothing
  return Tile::of({period, 8}, [runs = std::move(runs), period, phase](
                                   SkCanvas& c, SkSize sz, uint32_t) {
    // Start one wrapped phase to the left and paint two periods, so the
    // seam is covered whatever the phase.
    float x = -std::fmod(std::fmod(phase, period) + period, period);
    for (int rep = 0; rep < 2; ++rep)
      for (const auto& [w, col] : runs) {
        if (w <= 0) continue;
        SkPaint p;
        p.setColor4f(sk(col), nullptr);
        c.drawRect(SkRect::MakeXYWH(x, 0, w, sz.height()), p);
        x += w;
      }
  });
}

Tile checker(float cell, Color a, Color b) {
  const float s = std::max(cell, 1.0f);
  return Tile::of({2 * s, 2 * s}, [s, a, b](SkCanvas& c, SkSize, uint32_t) {
    SkPaint pa, pb;
    pa.setColor4f(sk(a), nullptr);
    pb.setColor4f(sk(b), nullptr);
    c.drawRect(SkRect::MakeWH(s, s), pa);
    c.drawRect(SkRect::MakeXYWH(s, s, s, s), pa);
    c.drawRect(SkRect::MakeXYWH(s, 0, s, s), pb);
    c.drawRect(SkRect::MakeXYWH(0, s, s, s), pb);
  });
}

Tile gridLines(float spacingX, float spacingY, float width, Color color) {
  const float sx = std::max(spacingX, 1.0f);
  const float sy = std::max(spacingY, 1.0f);
  return Tile::of({sx, sy}, [width, color](SkCanvas& c, SkSize sz, uint32_t) {
    SkPaint p;
    p.setColor4f(sk(color), nullptr);
    c.drawRect(SkRect::MakeWH(sz.width(), width), p);
    c.drawRect(SkRect::MakeWH(width, sz.height()), p);
  });
}

Tile speckle(float tileSize, int count, float rMin, float rMax,
             std::vector<Color> palette) {
  const float s = std::max(tileSize, 8.0f);
  return Tile::of({s, s}, [s, count, rMin, rMax, palette = std::move(palette)](
                              SkCanvas& c, SkSize, uint32_t seed) {
    SkPaint p;
    p.setAntiAlias(true);
    for (int i = 0; i < count; ++i) {
      const uint32_t k = (uint32_t)i;
      const float x = (0.5f + 0.5f * geometry::noise::hash(seed, 3 * k)) * s;
      const float y =
          (0.5f + 0.5f * geometry::noise::hash(seed, 3 * k + 1)) * s;
      const float t = 0.5f + 0.5f * geometry::noise::hash(seed, 3 * k + 2);
      const float r = rMin + (rMax - rMin) * t;
      if (!palette.empty())
        p.setColor4f(sk(palette[k % palette.size()]), nullptr);
      // Wraparound copies keep edges seamless.
      for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy)
          c.drawCircle(x + (float)dx * s, y + (float)dy * s, r, p);
    }
  });
}

}  // namespace sigil::material::pattern
