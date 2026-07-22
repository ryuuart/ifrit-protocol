#pragma once

/** @file
 * SigilCompose stock pattern generators — seeded PatternPrograms over the
 * Pattern seam. Each returns a Pattern: `.material()` fills anything,
 * `.seed(n)` re-rolls the seeded ones, `.rotate()/.scale()` remap without
 * rebaking. The reference-grounded generators (girih tessellation, halftone
 * ramps in the Persona grammar, chrome ramps) build on these primitives.
 */

#include "sigilcompose/Material.h" // halftoneRamp is a Material (SkSL)
#include "sigilcompose/Pattern.h"
#include "sigilcompose/Shapes.h" // detail::hashNoise (the seeded noise)

#include <include/core/SkPaint.h>
#include <include/core/SkString.h>
#include <include/effects/SkPerlinNoiseShader.h>
#include <include/effects/SkRuntimeEffect.h>

#include <algorithm>
#include <string>
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
inline Pattern gridLines(float spacingX, float spacingY, float width,
                         SkColor4f color) {
  const float sx = std::max(spacingX, 1.0f);
  const float sy = std::max(spacingY, 1.0f);
  return Pattern::tile({sx, sy}, [width, color](SkCanvas &c, SkSize sz,
                                                uint32_t) {
    SkPaint p;
    p.setColor4f(color, nullptr);
    c.drawRect(SkRect::MakeWH(sz.width(), width), p);
    c.drawRect(SkRect::MakeWH(width, sz.height()), p);
  });
}
/** Square pitch — the common case. A lattice whose x and y pitch differ
 *  is not exotic: an X-COM control panel's is 5 × 2. */
inline Pattern gridLines(float spacing, float width, SkColor4f color) {
  return gridLines(spacing, spacing, width, color);
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

/** The halftone RAMP (REFERENCES.md §1 — the P3R backdrop): dot radius
 *  swells from `rMin` at the node's top to `rMax` at its bottom, evaluated
 *  in one SkSL pass (a Material, not a baked tile — the ramp needs the
 *  node's height, so it rides the geometry tier: resolved when the node
 *  records, cached between layouts). `angleDeg` rotates the dot grid; the
 *  ramp stays vertical. To DRIFT the field (the menu idle), bind uDriftX /
 *  uDriftY: `.uniform("uDriftX", &phase)` — the material goes live and the
 *  dots slide under the fixed ramp. Drift wraps seamlessly at a period of
 *  2·spacing·√2 px along a 45° grid (d·cosθ ≡ 0 mod 2·spacing generally).
 *  `rampFrom`/`rampTo` remap where the swell runs as fractions of the
 *  node's height (0.25→0.9 confines it to the lower band). Keep
 *  rMax ≲ 0.45·spacing or neighboring dots fuse. */
inline Material halftoneRamp(float spacing, float rMin, float rMax,
                             SkColor4f color, float angleDeg = 0.0f,
                             float rampFrom = 0.0f, float rampTo = 1.0f) {
  static const sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(R"(
      uniform float2 uResolution;
      uniform float  uSpacing;
      uniform float  uRMin;
      uniform float  uRMax;
      uniform float  uAngle;   // radians
      uniform float  uDriftX;  // px — bind for the idle drift
      uniform float  uDriftY;
      uniform float  uRamp0;   // swell band, fractions of height
      uniform float  uRamp1;
      uniform float4 uColor;
      half4 main(float2 xy) {
        float ty = xy.y / max(uResolution.y, 1.0);
        float t = clamp((ty - uRamp0) / max(uRamp1 - uRamp0, 0.001), 0.0, 1.0);
        float2 p = xy + float2(uDriftX, uDriftY);
        float cs = cos(uAngle); float sn = sin(uAngle);
        p = float2(p.x * cs - p.y * sn, p.x * sn + p.y * cs);
        float row = floor(p.y / uSpacing);
        p.x += mod(row, 2.0) * uSpacing * 0.5; // staggered rows
        float2 cell = p - uSpacing * (floor(p / uSpacing) + 0.5);
        float r = mix(uRMin, uRMax, t);
        float d = length(cell) - r;
        float cov = 1.0 - smoothstep(-0.75, 0.75, d);
        float a = uColor.a * cov;
        return half4(half3(uColor.rgb) * a, a);
      }
    )"));
    if (!effect)
      SkDebugf("sigilcompose halftoneRamp shader: %s\n", err.c_str());
    return effect;
  }();
  if (!fx)
    return {};
  return Material::sksl(fx, {{"uSpacing", std::max(spacing, 1.0f)},
                             {"uRMin", rMin},
                             {"uRMax", rMax},
                             {"uAngle", angleDeg * 0.017453293f},
                             {"uDriftX", 0.0f},
                             {"uDriftY", 0.0f},
                             {"uRamp0", rampFrom},
                             {"uRamp1", rampTo}})
      .uniform("uColor", color);
}

/** Perlin fractal noise as a Material (SkPerlinNoiseShader — no SkSL):
 *  the organic texture floor. `frequency` is features-per-px (0.01–0.05
 *  reads as clouds/paper at UI scale; ~0.9 as film grain); `turbulence`
 *  uses the abs-value variant (sharper, veiny — brushed-metal fodder).
 *  Raw-shader identity: HOLD the returned Material as a member to prune
 *  across re-describes (each call mints a fresh shader — the same
 *  identity contract as Pattern). */
inline Material noise(float frequency, int octaves = 4, float seed = 1.0f,
                      bool turbulence = false) {
  sk_sp<SkShader> shader =
      turbulence ? SkShaders::MakeTurbulence(frequency, frequency, octaves,
                                             seed, nullptr)
                 : SkShaders::MakeFractalNoise(frequency, frequency, octaves,
                                               seed, nullptr);
  return Material::shader(std::move(shader));
}

/** LUMINANCE noise — value-noise fBm collapsed to one channel.
 *
 *  `noise()` above wraps Skia's Perlin shader, whose three channels are
 *  INDEPENDENT fields. That is the right thing for a displacement source
 *  and the wrong thing for grain: composited over a coloured surface with
 *  kOverlay or kSoftLight it does not darken and lighten the surface, it
 *  hue-shifts it — porphyry comes out as rainbow terrazzo. Every "make
 *  this surface less perfect" move — paper tooth, film grain, stone
 *  veining, dither, worn metal — wants THIS one, where all three channels
 *  carry the same value and a blend mode reads as light.
 *
 *  `frequency` is features-per-px on the same scale as `noise()`.
 *  `contrast` scales the field about 0.5 — wood tooth lives around
 *  0.25–0.35, and at 1.0 a soft-light pass turns timber into polished
 *  granite. `stretch` is anisotropy: the cells are divided by it in x and
 *  multiplied in y, so > 1 runs the fibre lengthwise (wood, brushed
 *  metal) and 1.0 is isotropic (dust, paper, stone).
 *
 *  MIND THE PRODUCT. `stretch` divides the x frequency but MULTIPLIES the
 *  y one by the same factor, so it is not free: keep
 *  `frequency · stretch · 2^(octaves-1)` under roughly 0.4, or the y axis
 *  aliases and you get hash noise with no diagnostic. Brushed metal at ×3
 *  wants something like `frequency 0.075, stretch 5` — not
 *  `frequency 6, stretch 6`, which asks for 36 cycles per pixel.
 *
 *  TWO IMPLEMENTATION RULES, and both are load-bearing. A sketch dylib
 *  carries its OWN copy of Skia — vcpkg builds Skia hidden-visibility, so
 *  sketch dylibs link libskia.a directly rather than resolving it from the
 *  host (sketch/README.md). `SkRuntimeEffect::MakeForShader` therefore
 *  builds the SkSL AST inside the SKETCH's Skia image while
 *  `getRPProgram` and the SkSL inliner run inside the HOST's, and virtual
 *  dispatch across that boundary faults on pointer authentication. So
 *  every stock SkSL material in this library must:
 *
 *    1. keep `main()` MONOLITHIC — no user-defined SkSL functions, so the
 *       inliner never runs;
 *    2. avoid a UNIFORM-GUARDED `break` — bake the loop count into the
 *       source string and cache one effect per count instead.
 *
 *  This function shipped violating both and segfaulted every sketch that
 *  painted it, with `MakeForShader` returning a valid effect and an empty
 *  error string, so there was nothing to catch. Four agents hit it inside
 *  an hour. `halftoneRamp()` above was always monolithic, which is exactly
 *  why it survived. `sketches/stock_materials.cpp` paints one of each
 *  stock material from a sketch dylib and is wired up as a ctest so the
 *  rule is enforced by the build rather than by memory. */
inline Material grain(float frequency, int octaves = 4, float seed = 1.0f,
                      float contrast = 1.0f, float stretch = 1.0f) {
  const int n = std::clamp(octaves, 1, 8);
  // One effect per octave count: the count is a compile-time constant in
  // the source, never a uniform the loop breaks against.
  static std::array<sk_sp<SkRuntimeEffect>, 9> cache{};
  if (!cache[(size_t)n]) {
    std::string src = R"(
uniform float2 uFreq;     // frequency, with the anisotropy folded in
uniform float  uSeed;
uniform float  uContrast;

half4 main(float2 pos) {
  float2 q = pos * uFreq;
  float sum = 0.0;
  float total = 0.0;
)";
    float amp = 0.5f;
    for (int o = 0; o < n; ++o) {
      const std::string a = std::to_string(amp);
      src += R"(
  {
    float2 c = floor(q);
    float2 f = fract(q);
    float2 w = f * f * (3.0 - 2.0 * f);
    float2 k0 = fract((c + float2(0.0, 0.0)) * float2(123.34, 456.21) + uSeed);
    k0 += dot(k0, k0 + 45.32);
    float2 k1 = fract((c + float2(1.0, 0.0)) * float2(123.34, 456.21) + uSeed);
    k1 += dot(k1, k1 + 45.32);
    float2 k2 = fract((c + float2(0.0, 1.0)) * float2(123.34, 456.21) + uSeed);
    k2 += dot(k2, k2 + 45.32);
    float2 k3 = fract((c + float2(1.0, 1.0)) * float2(123.34, 456.21) + uSeed);
    k3 += dot(k3, k3 + 45.32);
    float n = mix(mix(fract(k0.x * k0.y), fract(k1.x * k1.y), w.x),
                  mix(fract(k2.x * k2.y), fract(k3.x * k3.y), w.x), w.y);
    sum += )" + a + R"( * n;
    total += )" + a + R"(;
    q *= 2.0;
  }
)";
      amp *= 0.5f;
    }
    src += R"(
  float v = total > 0.0 ? sum / total : 0.5;
  v = clamp(0.5 + (v - 0.5) * uContrast, 0.0, 1.0);
  return half4(half3(v), 1.0);
}
)";
    auto [effect, error] = SkRuntimeEffect::MakeForShader(SkString(src.c_str()));
    if (!effect)
      SkDebugf("patterns::grain: %s\n", error.c_str());
    cache[(size_t)n] = std::move(effect);
  }
  if (!cache[(size_t)n])
    return Material::solid({0.5f, 0.5f, 0.5f, 1});
  const float k = stretch > 0.01f ? stretch : 1.0f;
  return Material::sksl(cache[(size_t)n], {{"uSeed", seed},
                                           {"uContrast", contrast}})
      .uniform("uFreq", std::array<float, 2>{frequency / k, frequency * k});
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
