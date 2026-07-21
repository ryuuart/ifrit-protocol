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

// ---------------------------------------------------------------------------
// Islamic geometric pattern (girih) — REFERENCES.md §4

/** Zellige color roles for the girih generators. */
struct GirihPalette {
  SkColor4f ground;    // the crosses (the leftover between stars)
  SkColor4f star;      // the khatam star fill
  SkColor4f strap;     // the ribbon
  SkColor4f strapEdge; // the ribbon's dark outline
};
/** Fez palette (REFERENCES.md §4): blue stars on teal ground, bone straps
 *  outlined in ink. */
inline GirihPalette fezPalette() {
  return {{0.078f, 0.463f, 0.420f, 1},   // #14766B
          {0.106f, 0.294f, 0.608f, 1},   // #1B4B9B
          {0.914f, 0.878f, 0.796f, 1},   // #E9E0CB
          {0.180f, 0.129f, 0.106f, 1}};  // #2E211B
}
/** Nasrid-leaning variant: parchment stars on deep blue. */
inline GirihPalette nasridPalette() {
  return {{0.204f, 0.329f, 0.612f, 1},   // #34549C
          {0.918f, 0.890f, 0.816f, 1},   // #EAE3D0
          {0.663f, 0.435f, 0.180f, 1},   // #A96F2E
          {0.149f, 0.125f, 0.110f, 1}};  // #26201C
}

/** The 8-fold star-and-cross panel ("Breath of the Compassionate") — real
 *  Hankin polygons-in-contact on the 4.8.8 tiling, in closed form: octagons
 *  of edge `a` sit on a square lattice of spacing s = a(1+√2), and the
 *  octagon APOTHEM equals s/2 exactly, so one s×s tile (octagon at center,
 *  square fillers at the corners) repeats seamlessly. The contact angle
 *  θ = 45° turns every octagon into the {8/2} khatam (two overlapped
 *  squares through the edge midpoints) and every filler square into its
 *  inscribed square — the strapwork of the classic panel. The crosses are
 *  the leftover ground, exactly as on the walls of Fez. REFERENCES.md §4. */
inline Pattern girih8(float edge, GirihPalette pal = fezPalette(),
                      float strapWidth = 0 /* 0 → 0.12·edge */) {
  const float a = std::max(edge, 4.0f);
  const float s = a * (1.0f + 1.41421356f);
  const float w = strapWidth > 0 ? strapWidth : 0.12f * a;
  return Pattern::tile({s, s}, [a, s, w, pal](SkCanvas &c, SkSize,
                                              uint32_t) {
    SkPaint fill;
    fill.setAntiAlias(true);

    // Ground: the crosses ARE the leftover between stars + corner squares.
    fill.setColor4f(pal.ground, nullptr);
    c.drawRect(SkRect::MakeWH(s, s), fill);

    // Khatam at the tile center: two squares through the octagon's 8 edge
    // midpoints (radius = apothem = s/2, at k·45°). Winding fill = union.
    const SkPoint center{s / 2, s / 2};
    const float R = s / 2;
    auto ringPoint = [&](int k, float radius, SkPoint at) {
      const float ang = (float)k * 0.78539816f; // 45°
      return SkPoint{at.x() + radius * std::cos(ang),
                     at.y() + radius * std::sin(ang)};
    };
    auto squarePath = [&](int k0) {
      SkPathBuilder b;
      b.moveTo(ringPoint(k0, R, center));
      for (int i = 1; i < 4; ++i)
        b.lineTo(ringPoint(k0 + 2 * i, R, center));
      b.close();
      return b.detach();
    };
    SkPath khatamA = squarePath(0), khatamB = squarePath(1);
    SkPathBuilder khatam;
    khatam.addPath(khatamA);
    khatam.addPath(khatamB);
    fill.setColor4f(pal.star, nullptr);
    c.drawPath(khatam.detach(), fill); // winding → the 8-point star union

    // Straps: outlined ribbons (dark wide pass, then the strap on top).
    auto ribbon = [&](const SkPath &path) {
      SkPaint p;
      p.setAntiAlias(true);
      p.setStyle(SkPaint::kStroke_Style);
      p.setStrokeJoin(SkPaint::kMiter_Join);
      p.setStrokeWidth(w * 1.5f);
      p.setColor4f(pal.strapEdge, nullptr);
      c.drawPath(path, p);
      p.setStrokeWidth(w);
      p.setColor4f(pal.strap, nullptr);
      c.drawPath(path, p);
    };
    ribbon(khatamA);
    ribbon(khatamB);

    // Corner fillers: each tile corner carries the 4.8.8 square (edge a,
    // rotated 45°); PIC θ=45 inscribes the square through ITS edge
    // midpoints (radius a/2 at 45°+k·90°). Drawn with wraparound copies so
    // the repeat is seamless.
    const SkPoint corners[4] = {{0, 0}, {s, 0}, {0, s}, {s, s}};
    for (const SkPoint &corner : corners) {
      SkPathBuilder b;
      for (int k = 0; k < 4; ++k) {
        const float ang = 0.78539816f + (float)k * 1.57079633f;
        const SkPoint pt{corner.x() + (a / 2) * std::cos(ang),
                         corner.y() + (a / 2) * std::sin(ang)};
        if (k == 0)
          b.moveTo(pt);
        else
          b.lineTo(pt);
      }
      b.close();
      ribbon(b.detach());
    }
  });
}

} // namespace sigil::compose::patterns
