/** @file
 * The stock tile programs: each draws one seamless tile with wraparound
 * copies where a mark crosses the edge. And the two lattice sources,
 * scanlines and the stipple, as programs read per pixel.
 */

#include "sigilmaterial/pattern/Patterns.h"

#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <sigilcore/compute/Noise.h>  // core::noise::hash
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/skia/Painted.h>
#include <sigilshaders/MaterialPattern.h>

#include <algorithm>
#include <cmath>
#include <glm/vec4.hpp>
#include <string>

namespace sigil::material::pattern {

namespace {
SkColor4f sk(Color c) { return {c.r, c.g, c.b, c.a}; }

/** A tile of @p size baked by @p painter into a raster canvas. */
Tile paintedTile(glm::vec2 size, skia::Painter painter) {
  return Tile::of(size, skia::painted(std::move(painter)));
}
}  // namespace

Tile halftone(float spacing, float radius, Color color, bool staggered) {
  const float s = std::max(spacing, 1.0f);
  const float tileH = staggered ? 2 * s : s;
  return paintedTile(
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
  return paintedTile({period, 8}, [on, color](SkCanvas& c, SkSize sz, uint32_t) {
    SkPaint p;
    p.setColor4f(sk(color), nullptr);
    c.drawRect(SkRect::MakeWH(on, sz.height()), p);
  });
}

Tile sequence(std::vector<std::pair<float, Color>> runs, float phase,
              Axis along) {
  float period = 0;
  for (const auto& [w, c] : runs) period += std::max(w, 0.0f);
  if (period <= 0) return stripes(1, 0, {0, 0, 0, 0});  // draws nothing
  const bool down = along == Axis::V;
  const glm::vec2 size = down ? glm::vec2{8, period} : glm::vec2{period, 8};
  return paintedTile(size, [runs = std::move(runs), period, phase, down](
                            SkCanvas& c, SkSize sz, uint32_t) {
    // Start one wrapped phase back along the axis and paint two periods,
    // so the seam is covered whatever the phase.
    float at = -std::fmod(std::fmod(phase, period) + period, period);
    for (int rep = 0; rep < 2; ++rep)
      for (const auto& [w, col] : runs) {
        if (w <= 0) continue;
        SkPaint p;
        p.setColor4f(sk(col), nullptr);
        c.drawRect(down ? SkRect::MakeXYWH(0, at, sz.width(), w)
                        : SkRect::MakeXYWH(at, 0, w, sz.height()),
                   p);
        at += w;
      }
  });
}

Tile checker(float cell, Color first, Color second) {
  const float s = std::max(cell, 1.0f);
  return paintedTile({2 * s, 2 * s}, [s, first, second](SkCanvas& c, SkSize, uint32_t) {
    SkPaint pa, pb;
    pa.setColor4f(sk(first), nullptr);
    pb.setColor4f(sk(second), nullptr);
    c.drawRect(SkRect::MakeWH(s, s), pa);
    c.drawRect(SkRect::MakeXYWH(s, s, s, s), pa);
    c.drawRect(SkRect::MakeXYWH(s, 0, s, s), pb);
    c.drawRect(SkRect::MakeXYWH(0, s, s, s), pb);
  });
}

Tile gridLines(float spacingX, float spacingY, float width, Color color) {
  const float sx = std::max(spacingX, 1.0f);
  const float sy = std::max(spacingY, 1.0f);
  return paintedTile({sx, sy}, [width, color](SkCanvas& c, SkSize sz, uint32_t) {
    SkPaint p;
    p.setColor4f(sk(color), nullptr);
    c.drawRect(SkRect::MakeWH(sz.width(), width), p);
    c.drawRect(SkRect::MakeWH(width, sz.height()), p);
  });
}

Tile speckle(float tileSize, int count, float minimumRadius, float maximumRadius,
             std::vector<Color> palette) {
  const float s = std::max(tileSize, 8.0f);
  return paintedTile({s, s}, [s, count, minimumRadius, maximumRadius, palette = std::move(palette)](
                              SkCanvas& c, SkSize, uint32_t seed) {
    SkPaint p;
    p.setAntiAlias(true);
    for (int i = 0; i < count; ++i) {
      const uint32_t k = (uint32_t)i;
      const float x = (0.5f + 0.5f * core::noise::hash(seed, 3 * k)) * s;
      const float y = (0.5f + 0.5f * core::noise::hash(seed, 3 * k + 1)) * s;
      const float t = 0.5f + 0.5f * core::noise::hash(seed, 3 * k + 2);
      const float r = minimumRadius + (maximumRadius - minimumRadius) * t;
      if (!palette.empty())
        p.setColor4f(sk(palette[k % palette.size()]), nullptr);
      // Wraparound copies keep edges seamless.
      for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy)
          c.drawCircle(x + (float)dx * s, y + (float)dy * s, r, p);
    }
  });
}

namespace {

/** The scanlines' uniforms, by the names the body reads. */
struct ScanlineParameters {
  Color ink;
  float period;
  float on;
  float phase;
};

/** The stipple's uniforms: the mask as four sixteen-bit words, each a
 *  whole float, which every shading language holds exactly. */
struct StippleParameters {
  Color ink;
  glm::vec4 words;
  float size;
  float cell;
};

const std::shared_ptr<const Recipe>& scanlineRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<ScanlineParameters>("pattern.scanlines")
          .body(Target::SkSL, std::string(shaderSource("Scanlines.sksl"))));
  return recipe;
}

const std::shared_ptr<const Recipe>& stippleRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<StippleParameters>("pattern.stipple")
          .body(Target::SkSL, std::string(shaderSource("Stipple.sksl"))));
  return recipe;
}

}  // namespace

Material scanlines(const ScanlineOptions& options) {
  if (options.period <= 0.0f || options.on <= 0.0f)
    return Color{0, 0, 0, 0};
  return Material(scanlineRecipe(),
                  ScanlineParameters{options.color, options.period,
                                     options.on, options.phase});
}

Material stipple(const StippleOptions& options) {
  if (options.bits == 0 || options.size <= 0 || options.size > 8 ||
      options.cell <= 0.0f)
    return Color{0, 0, 0, 0};
  const auto word = [&](int index) {
    return (float)((options.bits >> (16 * index)) & 0xFFFFu);
  };
  return Material(stippleRecipe(),
                  StippleParameters{options.color,
                                    {word(0), word(1), word(2), word(3)},
                                    (float)options.size, options.cell});
}

uint64_t ditherBits(int on, int size) {
  size = std::clamp(size, 1, 8);
  // The Bayer threshold matrix, built by the recursion that defines it:
  // each step quadruples the lattice, the four quadrants offset by 0, 2,
  // 3 and 1 quarters of the range.
  int threshold[8][8] = {{0}};
  int side = 1;
  while (side < size) {
    for (int y = 0; y < side; ++y)
      for (int x = 0; x < side; ++x) {
        const int base = threshold[y][x] * 4;
        threshold[y][x] = base;
        threshold[y][x + side] = base + 2;
        threshold[y + side][x] = base + 3;
        threshold[y + side][x + side] = base + 1;
      }
    side *= 2;
  }
  uint64_t bits = 0;
  for (int y = 0; y < size; ++y)
    for (int x = 0; x < size; ++x)
      if (threshold[y][x] < on) bits |= uint64_t{1} << (y * size + x);
  return bits;
}

}  // namespace sigil::material::pattern
