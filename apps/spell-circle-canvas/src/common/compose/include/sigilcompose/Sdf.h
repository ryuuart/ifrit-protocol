#pragma once

/** @file
 * SigilCompose SDF materials — shape + border + glow + soft shadow in ONE
 * shader pass (REVIEW.md §4.3; Inigo Quilez's 2D SDF operators). An
 * extension over the public Material API: `sdf::material(shape, style)`
 * returns an ordinary Material for `.fill()`, so it caches, prunes, and
 * animates by the kernel's standard rules:
 *
 *  - the effect reads `uResolution`, so the material is GEOMETRY-DEPENDENT:
 *    it resolves when its node records, stays picture-cached between
 *    layouts, and re-records on size change — static SDF chrome costs one
 *    recording, not per-frame paint;
 *  - every style parameter is a named uniform — bind one to a ch::Output
 *    (`.uniform("uBorderW", &width)` / `"uGlowR"` / `"uP0"`…) and the
 *    material goes live: ALIVE borders and breathing glows are bound
 *    scalars, not repaint loops;
 *  - ONE compiled effect per shape KIND (parameters are uniforms), so a
 *    thousand differently-styled cells share three shaders total — no
 *    permutation explosion (the UE material lesson).
 *
 * Distances are computed in aspect-corrected PIXEL space (never UV), so
 * borders stay even on stretched nodes — the verified SDF pitfall. Sharp
 * star tips at very small sizes want MSDF (median-of-3); this analytic
 * evaluation is exact at any zoom and needs no atlas.
 *
 * Known tradeoff: the anti-alias half-width is 0.75 LOCAL px, so a
 * recording replayed under a scaled host softens edges slightly (the
 * kernel's recordings are best-effort on host scale by contract).
 * Declaring uContentScale would track zoom exactly but makes a material
 * LIVE (per-frame paint) — this header deliberately chooses cacheability;
 * re-records on layout change re-crisp the edges.
 *
 * The SDF paints the node's LOOK; hit-testing and clipping still use the
 * node's geometry — pair with `.outline(shapes::star(...))` when hits/clips
 * should match the silhouette.
 */

#include "sigilcompose/Material.h"

#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace sigil::compose::sdf {

enum class Kind : uint8_t { RoundBox, Circle, Star };

/** A silhouette, sized by the node's box (minus the style's reserved pad). */
struct Shape {
  Kind kind = Kind::RoundBox;
  float p0 = 0, p1 = 0, p2 = 0; // per-kind params (uniforms uP0..uP2)
};

/** Rounded box inscribed in the node (radius in px, clamped to half-size). */
inline Shape roundBox(float radius) { return {Kind::RoundBox, radius, 0, 0}; }

/** Circle inscribed in the node's box. */
inline Shape circle() { return {Kind::Circle, 0, 0, 0}; }

/** N-pointed star (IQ sdStar). `pointiness` is IQ's m ∈ [2, points]:
 *  m = points is the regular polygon, values toward 2 sharpen the arms —
 *  the multi-pointed chaotic-star primitive, exact at any zoom. */
inline Shape star(int points, float pointiness) {
  const float n = (float)std::max(points, 3);
  return {Kind::Star, n, std::clamp(pointiness, 2.0f, n), 0};
}

/** How the silhouette is dressed — every field is a shader uniform.
 *  Layer order (back to front): shadow, glow, fill, border. */
struct Style {
  SkColor4f fill = {0, 0, 0, 0};
  float borderWidth = 0;                  // centered on the edge (uBorderW)
  SkColor4f borderColor = {1, 1, 1, 1};   // (uBorder)
  float glowRadius = 0;                   // exp falloff, px (uGlowR)
  SkColor4f glowColor = {1, 1, 1, 1};     // (uGlow)
  SkVector shadowOffset = {0, 0};         // (uShadowOffX/Y)
  float shadowBlur = 0;                   // (uShadowBlur)
  SkColor4f shadowColor = {0, 0, 0, 0};   // alpha 0 = no shadow (uShadow)
};

namespace detail {

inline constexpr char kPrelude[] = R"(
uniform float2 uResolution;
uniform float  uPad;
uniform float4 uFill;
uniform float  uBorderW;
uniform float4 uBorder;
uniform float  uGlowR;
uniform float4 uGlow;
uniform float  uShadowOffX;
uniform float  uShadowOffY;
uniform float  uShadowBlur;
uniform float4 uShadow;
uniform float  uP0;
uniform float  uP1;
uniform float  uP2;
)";

inline constexpr char kSdRoundBox[] = R"(
float sd(float2 p, float2 b) {
  float r = min(uP0, min(b.x, b.y));
  float2 q = abs(p) - b + r;
  return min(max(q.x, q.y), 0.0) + length(max(q, float2(0.0))) - r;
}
)";

inline constexpr char kSdCircle[] = R"(
float sd(float2 p, float2 b) { return length(p) - min(b.x, b.y); }
)";

// IQ's exact-distance N-star (https://iquilezles.org/articles/distfunctions2d/).
inline constexpr char kSdStar[] = R"(
float sd(float2 p, float2 b) {
  float r = min(b.x, b.y);
  float an = 3.1415927 / uP0;
  float en = 3.1415927 / uP1;
  float2 acs = float2(cos(an), sin(an));
  float2 ecs = float2(cos(en), sin(en));
  float bn = mod(atan(p.x, p.y), 2.0 * an) - an;
  p = length(p) * float2(cos(bn), abs(sin(bn)));
  p -= r * acs;
  p += ecs * clamp(-dot(p, ecs), 0.0, r * acs.y / ecs.y);
  return length(p) * sign(p.x);
}
)";

inline constexpr char kMain[] = R"(
float4 overOp(float4 dst, float4 srcStraight, float cov) {
  float a = clamp(srcStraight.a * cov, 0.0, 1.0);
  float4 src = float4(srcStraight.rgb * a, a);
  return src + dst * (1.0 - a);
}
half4 main(float2 xy) {
  float2 c = uResolution * 0.5;
  float2 b = max(c - uPad, float2(0.5));
  float2 p = xy - c;
  float aa = 0.75;
  float d = sd(p, b);
  float4 acc = float4(0.0);
  if (uShadow.a > 0.0) {
    float ds = sd(p - float2(uShadowOffX, uShadowOffY), b);
    float sb = max(uShadowBlur, aa);
    acc = overOp(acc, uShadow, 1.0 - smoothstep(-sb, sb, ds));
  }
  if (uGlowR > 0.0) {
    // Exponential falloff, forced to EXACT zero by the pad edge so the
    // node's box never crops a visible square (the poe-study fix).
    float g = exp(-max(d, 0.0) / uGlowR) *
              max(0.0, 1.0 - max(d, 0.0) / max(uPad - 1.0, 1.0));
    acc = overOp(acc, uGlow, g);
  }
  acc = overOp(acc, uFill, 1.0 - smoothstep(-aa, aa, d));
  if (uBorderW > 0.0) {
    float hw = uBorderW * 0.5;
    acc = overOp(acc, uBorder, 1.0 - smoothstep(hw - aa, hw + aa, abs(d)));
  }
  return half4(acc); // premultiplied
}
)";

inline sk_sp<SkRuntimeEffect> makeEffect(const char *sdFn) {
  const std::string src = std::string(kPrelude) + sdFn + kMain;
  auto [fx, err] = SkRuntimeEffect::MakeForShader(SkString(src.c_str()));
  if (!fx)
    SkDebugf("sigilcompose sdf shader: %s\n", err.c_str());
  return fx;
}

/** One compiled effect per kind, shared by every instance/style. */
inline const sk_sp<SkRuntimeEffect> &effectFor(Kind kind) {
  switch (kind) {
  case Kind::RoundBox: {
    static const sk_sp<SkRuntimeEffect> fx = makeEffect(kSdRoundBox);
    return fx;
  }
  case Kind::Circle: {
    static const sk_sp<SkRuntimeEffect> fx = makeEffect(kSdCircle);
    return fx;
  }
  case Kind::Star:
  default: {
    static const sk_sp<SkRuntimeEffect> fx = makeEffect(kSdStar);
    return fx;
  }
  }
}

} // namespace detail

/** The pad the style reserves inside the node's box (border half-width +
 *  glow/shadow reach). PUBLIC so callers can size a node by its VISIBLE
 *  silhouette: box dimension = visible diameter + 2·sdf::pad(style)
 *  (the poe-study ask — never hand-copy the formula). */
inline float pad(const Style &style) {
  const float glowPad = style.glowRadius > 0 ? style.glowRadius * 3.2f : 0.0f;
  const float shadowPad =
      style.shadowColor.fA > 0
          ? std::max(std::abs(style.shadowOffset.fX),
                     std::abs(style.shadowOffset.fY)) +
                style.shadowBlur * 1.5f
          : 0.0f;
  return style.borderWidth * 0.5f + std::max(glowPad, shadowPad) + 1.0f;
}

/** The SDF material: shape + style, one shader pass. The style's outer
 *  treatments (border half-width, glow falloff, shadow reach) reserve a pad
 *  inside the node's box so nothing clips; bindable uniforms animate within
 *  that reserve (bind a glow up to the style's glowRadius, not past it). */
inline Material material(const Shape &shape, const Style &style) {
  const sk_sp<SkRuntimeEffect> &fx = detail::effectFor(shape.kind);
  if (!fx)
    return {};
  const float padPx = pad(style);
  return Material::sksl(fx, {{"uPad", padPx},
                             {"uBorderW", style.borderWidth},
                             {"uGlowR", style.glowRadius},
                             {"uShadowOffX", style.shadowOffset.fX},
                             {"uShadowOffY", style.shadowOffset.fY},
                             {"uShadowBlur", style.shadowBlur},
                             {"uP0", shape.p0},
                             {"uP1", shape.p1},
                             {"uP2", shape.p2}})
      .uniform("uFill", style.fill)
      .uniform("uBorder", style.borderColor)
      .uniform("uGlow", style.glowColor)
      .uniform("uShadow", style.shadowColor);
}

} // namespace sigil::compose::sdf
