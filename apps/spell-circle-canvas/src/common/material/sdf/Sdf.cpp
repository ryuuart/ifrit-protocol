/** @file
 * The distance functions and the one-pass shape, border, glow and soft
 * shadow body, one recipe per silhouette kind.
 */

#include "sigilmaterial/sdf/Sdf.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace sigil::material::sdf {

Shape star(int points, float pointiness) {
  const float n = (float)std::max(points, 3);
  Shape s;
  s.kind = Kind::Star;
  s.p0 = n;
  s.p1 = std::clamp(pointiness, 2.0f, n);
  return s;
}

namespace {

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

// The exact-distance N-star.
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
    // box never crops a visible square.
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

std::shared_ptr<const Recipe> make(const char* name, const char* sdFn) {
  return std::make_shared<const Recipe>(
      Recipe::of<SdfParams>(name)
          .frame(FrameInput::Resolution)
          .body(Target::SkSL, std::string(sdFn) + kMain));
}

}  // namespace

const std::shared_ptr<const Recipe>& recipe(Kind kind) {
  switch (kind) {
    case Kind::RoundBox: {
      static const auto r = make("sdf.roundBox", kSdRoundBox);
      return r;
    }
    case Kind::Circle: {
      static const auto r = make("sdf.circle", kSdCircle);
      return r;
    }
    case Kind::Star:
    default: {
      static const auto r = make("sdf.star", kSdStar);
      return r;
    }
  }
}

float pad(const Style& style) {
  const float glowPad = style.glowRadius > 0 ? style.glowRadius * 3.2f : 0.0f;
  const float shadowPad = style.shadowColor.a > 0
                              ? std::max(std::abs(style.shadowOffset.x),
                                         std::abs(style.shadowOffset.y)) +
                                    style.shadowBlur * 1.5f
                              : 0.0f;
  return style.borderWidth * 0.5f + std::max(glowPad, shadowPad) + 1.0f;
}

Material material(const Shape& shape, const Style& style) {
  return Material(
      recipe(shape.kind),
      SdfParams{pad(style), style.fill, style.borderWidth, style.borderColor,
                style.glowRadius, style.glowColor, style.shadowOffset.x,
                style.shadowOffset.y, style.shadowBlur, style.shadowColor,
                shape.p0, shape.p1, shape.p2});
}

std::vector<Material> everyRecipe() {
  Style dressed;
  dressed.fill = {0.2f, 0.5f, 0.9f, 1};
  dressed.borderWidth = 2;
  dressed.glowRadius = 6;
  dressed.shadowOffset = {2, 3};
  dressed.shadowBlur = 4;
  dressed.shadowColor = {0, 0, 0, 0.5f};
  return {material(roundBox(8), dressed), material(circle(), dressed),
          material(star(5, 3), dressed)};
}

}  // namespace sigil::material::sdf
