#include "sigilcompose/Patterns.h"

#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <include/effects/SkPerlinNoiseShader.h>
#include <include/effects/SkRuntimeEffect.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "sigilgeometry/path/Noise.h"  // noise::hash (the seeded noise)

namespace sigil::compose::patterns {

Pattern halftone(float spacing, float radius, SkColor4f color, bool staggered) {
  const float s = std::max(spacing, 1.0f);
  const float tileH = staggered ? 2 * s : s;
  return Pattern::tile(
      {s, tileH}, [s, radius, color, staggered](SkCanvas& c, SkSize, uint32_t) {
        SkPaint p;
        p.setAntiAlias(true);
        p.setColor4f(color, nullptr);
        auto dot = [&](float cx, float cy) {
          // Draw with wraparound copies so tile edges stay seamless.
          for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy)
              c.drawCircle(cx + (float)dx * s,
                           cy + (float)dy * (staggered ? 2 * s : s), radius, p);
        };
        dot(s * 0.5f, s * 0.5f);
        if (staggered)
          dot(0.0f, s * 1.5f);  // half-cell offset row (and its wrap at x=s)
      });
}

Pattern stripes(float on, float off, SkColor4f color) {
  const float period = std::max(on + off, 1.0f);
  return Pattern::tile({period, 8},
                       [on, color](SkCanvas& c, SkSize sz, uint32_t) {
                         SkPaint p;
                         p.setColor4f(color, nullptr);
                         c.drawRect(SkRect::MakeWH(on, sz.height()), p);
                       });
}

Pattern sequence(std::vector<std::pair<float, SkColor4f>> runs, float phase) {
  float period = 0;
  for (const auto& [w, c] : runs) period += std::max(w, 0.0f);
  if (period <= 0)
    return stripes(1, 0, {0, 0, 0, 0});  // degenerate: draws nothing
  return Pattern::tile({period, 8}, [runs = std::move(runs), period, phase](
                                        SkCanvas& c, SkSize sz, uint32_t) {
    // Start one wrapped phase to the left and paint two periods, so
    // the seam is covered whatever the phase.
    float x = -std::fmod(std::fmod(phase, period) + period, period);
    for (int rep = 0; rep < 2; ++rep)
      for (const auto& [w, col] : runs) {
        if (w <= 0) continue;
        SkPaint p;
        p.setColor4f(col, nullptr);
        c.drawRect(SkRect::MakeXYWH(x, 0, w, sz.height()), p);
        x += w;
      }
  });
}

Pattern checker(float cell, SkColor4f a, SkColor4f b) {
  const float s = std::max(cell, 1.0f);
  return Pattern::tile({2 * s, 2 * s},
                       [s, a, b](SkCanvas& c, SkSize, uint32_t) {
                         SkPaint pa, pb;
                         pa.setColor4f(a, nullptr);
                         pb.setColor4f(b, nullptr);
                         c.drawRect(SkRect::MakeWH(s, s), pa);
                         c.drawRect(SkRect::MakeXYWH(s, s, s, s), pa);
                         c.drawRect(SkRect::MakeXYWH(s, 0, s, s), pb);
                         c.drawRect(SkRect::MakeXYWH(0, s, s, s), pb);
                       });
}

Pattern gridLines(float spacingX, float spacingY, float width,
                  SkColor4f color) {
  const float sx = std::max(spacingX, 1.0f);
  const float sy = std::max(spacingY, 1.0f);
  return Pattern::tile({sx, sy},
                       [width, color](SkCanvas& c, SkSize sz, uint32_t) {
                         SkPaint p;
                         p.setColor4f(color, nullptr);
                         c.drawRect(SkRect::MakeWH(sz.width(), width), p);
                         c.drawRect(SkRect::MakeWH(width, sz.height()), p);
                       });
}

Pattern speckle(float tileSize, int count, float rMin, float rMax,
                std::vector<SkColor4f> palette) {
  const float s = std::max(tileSize, 8.0f);
  return Pattern::tile({s, s}, [s, count, rMin, rMax,
                                palette = std::move(palette)](
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
      if (!palette.empty()) p.setColor4f(palette[k % palette.size()], nullptr);
      // Wraparound copies keep edges seamless.
      for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy)
          c.drawCircle(x + (float)dx * s, y + (float)dy * s, r, p);
    }
  });
}

Material halftoneRamp(float spacing, float rMin, float rMax, SkColor4f color,
                      float angleDeg, float rampFrom, float rampTo) {
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
  if (!fx) return {};
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

Material noise(float frequency, int octaves, float seed, bool turbulence) {
  sk_sp<SkShader> shader =
      turbulence ? SkShaders::MakeTurbulence(frequency, frequency, octaves,
                                             seed, nullptr)
                 : SkShaders::MakeFractalNoise(frequency, frequency, octaves,
                                               seed, nullptr);
  return Material::shader(std::move(shader));
}

Material grain(float frequency, int octaves, float seed, float contrast,
               float stretch) {
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
    sum += )" +
             a + R"( * n;
    total += )" +
             a + R"(;
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
    auto [effect, error] =
        SkRuntimeEffect::MakeForShader(SkString(src.c_str()));
    if (!effect) SkDebugf("patterns::grain: %s\n", error.c_str());
    cache[(size_t)n] = std::move(effect);
  }
  if (!cache[(size_t)n]) return Material::solid({0.5f, 0.5f, 0.5f, 1});
  const float k = stretch > 0.01f ? stretch : 1.0f;
  return Material::sksl(cache[(size_t)n],
                        {{"uSeed", seed}, {"uContrast", contrast}})
      .uniform("uFreq", std::array<float, 2>{frequency / k, frequency * k});
}

Pattern girih8(float edge, GirihPalette pal, float strapWidth) {
  const float a = std::max(edge, 4.0f);
  const float s = a * (1.0f + 1.41421356f);
  const float w = strapWidth > 0 ? strapWidth : 0.12f * a;
  return Pattern::tile({s, s}, [a, s, w, pal](SkCanvas& c, SkSize, uint32_t) {
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
      const float ang = (float)k * 0.78539816f;  // 45°
      return SkPoint{at.x() + radius * std::cos(ang),
                     at.y() + radius * std::sin(ang)};
    };
    auto squarePath = [&](int k0) {
      SkPathBuilder b;
      b.moveTo(ringPoint(k0, R, center));
      for (int i = 1; i < 4; ++i) b.lineTo(ringPoint(k0 + 2 * i, R, center));
      b.close();
      return b.detach();
    };
    SkPath khatamA = squarePath(0), khatamB = squarePath(1);
    SkPathBuilder khatam;
    khatam.addPath(khatamA);
    khatam.addPath(khatamB);
    fill.setColor4f(pal.star, nullptr);
    c.drawPath(khatam.detach(), fill);  // winding → the 8-point star union

    // Straps: outlined ribbons (dark wide pass, then the strap on top).
    auto ribbon = [&](const SkPath& path) {
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
    for (const SkPoint& corner : corners) {
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

}  // namespace sigil::compose::patterns
