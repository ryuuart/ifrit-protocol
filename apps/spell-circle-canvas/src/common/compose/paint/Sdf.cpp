/** @file
 * SDF materials: the signed-distance silhouettes and the one-pass shape,
 * border, glow and soft-shadow shader built over them.
 */

#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/paint/Sdf.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace sigil::compose::sdf {

Shape star(int points, float pointiness) {
  const float n = (float)std::max(points, 3);
  Shape s;
  s.kind = Kind::Star;
  s.p0 = n;
  s.p1 = std::clamp(pointiness, 2.0f, n);
  return s;
}

namespace {

constexpr char kPrelude[] = R"(
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

constexpr char kSdRoundBox[] = R"(
float sd(float2 p, float2 b) {
  float r = min(uP0, min(b.x, b.y));
  float2 q = abs(p) - b + r;
  return min(max(q.x, q.y), 0.0) + length(max(q, float2(0.0))) - r;
}
)";

constexpr char kSdCircle[] = R"(
float sd(float2 p, float2 b) { return length(p) - min(b.x, b.y); }
)";

// IQ's exact-distance N-star
// (https://iquilezles.org/articles/distfunctions2d/).
constexpr char kSdStar[] = R"(
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

constexpr char kMain[] = R"(
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

sk_sp<SkRuntimeEffect> makeEffect(const char* sdFn) {
  const std::string src = std::string(kPrelude) + sdFn + kMain;
  auto [fx, err] = SkRuntimeEffect::MakeForShader(SkString(src.c_str()));
  if (!fx) SkDebugf("sigilcompose sdf shader: %s\n", err.c_str());
  return fx;
}

/** One compiled effect per kind, shared by every instance/style. */
const sk_sp<SkRuntimeEffect>& effectFor(Kind kind) {
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

}  // namespace

float pad(const Style& style) {
  const float glowPad = style.glowRadius > 0 ? style.glowRadius * 3.2f : 0.0f;
  const float shadowPad = style.shadowColor.fA > 0
                              ? std::max(std::abs(style.shadowOffset.fX),
                                         std::abs(style.shadowOffset.fY)) +
                                    style.shadowBlur * 1.5f
                              : 0.0f;
  return style.borderWidth * 0.5f + std::max(glowPad, shadowPad) + 1.0f;
}

Material material(const Shape& shape, const Style& style) {
  const sk_sp<SkRuntimeEffect>& fx = effectFor(shape.kind);
  if (!fx) return {};
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

}  // namespace sigil::compose::sdf
