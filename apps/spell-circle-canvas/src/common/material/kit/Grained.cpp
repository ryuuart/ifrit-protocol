/** @file
 * The four grained bodies in each language a renderer speaks — the
 * shared prelude of hash, value noise, the luminance fold and the
 * lattice fleck, then the stone's bed, the timber's arrises and grain
 * lines, the latten's ladder and sheen, and the board's tooth and wear.
 */

#include "sigilmaterial/kit/Grained.h"

#include <string>

namespace sigil::material::kit {

namespace {

/** Hash, value noise, three octaves of it, the fold of a luminance field
 *  into a colour, and a fleck in a lattice cell: the pieces every grained
 *  body is made of. */
constexpr char kPrelude[] = R"(
float hash21(float2 p) {
  return fract(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}

float vnoise(float2 p) {
  float2 i = floor(p);
  float2 f = fract(p);
  float2 u = f * f * (3.0 - 2.0 * f);
  float a = hash21(i);
  float b = hash21(i + float2(1.0, 0.0));
  float c = hash21(i + float2(0.0, 1.0));
  float d = hash21(i + float2(1.0, 1.0));
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm3(float2 p) {
  return vnoise(p) * 0.55 + vnoise(p * 2.13) * 0.3 + vnoise(p * 4.41) * 0.15;
}

// The field folded into the colour as LIGHT: a scale about its mean, so
// a coloured surface stays its own hue and reads as shaded.
float3 grained(float3 c, float g, float k) {
  return c * (1.0 + (g - 0.5) * 2.0 * k);
}

// One fleck in the lattice cell around q, when the cell carries one:
// its coverage at q, and which of two tones it takes. The fleck is kept
// inside its cell, so no cell clips a neighbour's.
float2 fleck(float2 q, float cell, float density, float s) {
  float2 c = floor(q / cell);
  float h = hash21(c + s);
  if (h >= density) return float2(0.0, 0.0);
  float r = cell * (0.12 + 0.18 * hash21(c + s + 7.0));
  float2 jitter = float2(hash21(c + s + 17.0), hash21(c + s + 31.0)) - 0.5;
  float2 centre = (c + 0.5) * cell + jitter * (cell - 2.0 * r);
  float d = length(q - centre);
  float cover = 1.0 - smoothstep(r - 0.75, r + 0.75, d);
  return float2(cover, step(0.5, hash21(c + s + 3.0)));
}
)";

constexpr char kStone[] = R"(
half4 main(float2 p) {
  float2 q = p + seed * 91.7;
  float a = bedAngle * 0.017453292;
  float2 d = float2(cos(a), sin(a));
  float t = fract(dot(q, d) / max(bedLength, 1.0));
  float bed = 1.0 - abs(2.0 * t - 1.0);
  float3 c = mix(hi.rgb, lo.rgb, bed * bedDepth);
  float k = max(stretch, 0.01);
  float g = fbm3(q * float2(grainScale / k, grainScale * k));
  c = grained(c, g, grainContrast);
  float2 f = fleck(q, max(speckleCell, 2.0), speckle, seed);
  float3 tone = f.y > 0.5 ? min(hi.rgb * 1.45, float3(1.0)) : lo.rgb * 0.55;
  c = mix(c, tone, f.x * speckleAlpha);
  return half4(half3(clamp(c, 0.0, 1.0)), 1.0);
}
)";

constexpr char kTimber[] = R"(
half4 main(float2 p) {
  float2 xy = along > 0.5 ? float2(p.y, p.x) : p;
  float v = clamp(xy.y / max(span, 1.0), 0.0, 1.0);
  v = flip > 0.5 ? 1.0 - v : v;
  // A planed board: a flat face between a narrow lit arris and a narrow
  // shadowed one. Widen these two and the piece reads as rope.
  float lit = 1.0 - smoothstep(0.0, 0.17, v);
  float shade = smoothstep(0.78, 1.0, v);
  float s = seed * 0.41;
  float x = xy.x * grain + s * 17.3;
  float g = sin(x) * 0.5 + sin(x * 2.31 + s * 3.1) * 0.3 +
            sin(x * 5.77 + s * 7.7) * 0.2;
  g = g * 0.5 + 0.5;
  float fig = smoothstep(0.48, 0.96, g);
  // A slow lengthwise tone drift, so no two pieces read identical.
  float drift = sin(xy.x * 0.011 + s * 5.0) * 0.5 + 0.5;
  float3 c = mix(base.rgb, light.rgb, lit * 0.92);
  c = mix(c, dark.rgb, shade * 0.85);
  c = mix(c, dark.rgb, fig * figure);
  c = mix(c, light.rgb, drift * 0.07);
  float k = max(stretch, 0.01);
  float t = fbm3((xy + s * 53.0) * float2(toothScale / k, toothScale * k));
  c = grained(c, t, tooth);
  return half4(half3(clamp(c, 0.0, 1.0)), 1.0);
}
)";

constexpr char kLatten[] = R"(
half4 main(float2 p) {
  float2 run = to - from;
  float u = clamp(dot(p - from, run) / max(dot(run, run), 1e-6), 0.0, 1.0);
  float pos = clamp(level + (u - 0.5) * sheen, 0.0, 1.0);
  float3 c = pos < 0.5 ? mix(shadow.rgb, body.rgb, pos * 2.0)
                       : mix(body.rgb, light.rgb, (pos - 0.5) * 2.0);
  float2 q = p + seed * 37.0;
  float t = fbm3(q * toothScale);
  c = grained(c, t, tooth);
  float2 f = fleck(q, max(patinaCell, 2.0), patina, seed);
  c = mix(c, patinaColor.rgb, f.x * patinaColor.a);
  return half4(half3(clamp(c, 0.0, 1.0)), 1.0);
}
)";

constexpr char kBoard[] = R"(
half4 main(float2 p) {
  float2 q = p + seed * 61.0;
  float k = max(stretch, 0.01);
  float t = fbm3(q * float2(toothScale / k, toothScale * k));
  float3 c = grained(paint.rgb, t, tooth);
  float w = fbm3(q * wearScale + 11.0);
  c = grained(c, w, wear);
  return half4(half3(clamp(c, 0.0, 1.0)), 1.0);
}
)";

/** The same pieces in Slang. A Slang body answers `float4 surface(float2
 *  uv)` in straight alpha, and reads the surface's uv where the SkSL
 *  body reads pixels. The intrinsics whose two targets are two different
 *  pieces of code are spelled in each language's own word. */
constexpr char kSlangPrelude[] = R"(
float hashG(float2 p) {
  return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}

float vnoiseG(float2 p) {
  float2 i = floor(p);
  float2 f = frac(p);
  float2 u = f * f * (3.0 - 2.0 * f);
  float a = hashG(i);
  float b = hashG(i + float2(1.0, 0.0));
  float c = hashG(i + float2(0.0, 1.0));
  float d = hashG(i + float2(1.0, 1.0));
  return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float fbm3G(float2 p) {
  return vnoiseG(p) * 0.55 + vnoiseG(p * 2.13) * 0.3 +
         vnoiseG(p * 4.41) * 0.15;
}

float3 grainedG(float3 c, float g, float k) {
  return c * (1.0 + (g - 0.5) * 2.0 * k);
}

float2 fleckG(float2 q, float cell, float density, float s) {
  float2 c = floor(q / cell);
  float h = hashG(c + s);
  if (h >= density) return float2(0.0, 0.0);
  float r = cell * (0.12 + 0.18 * hashG(c + s + 7.0));
  float2 jitter = float2(hashG(c + s + 17.0), hashG(c + s + 31.0)) - 0.5;
  float2 centre = (c + 0.5) * cell + jitter * (cell - 2.0 * r);
  float d = length(q - centre);
  float cover = 1.0 - smoothstep(r - 0.75, r + 0.75, d);
  return float2(cover, step(0.5, hashG(c + s + 3.0)));
}
)";

constexpr char kSlangStone[] = R"(
float4 surface(float2 uv) {
  float2 q = uv + seed * 91.7;
  float a = bedAngle * 0.017453292;
  float2 d = float2(cos(a), sin(a));
  float t = frac(dot(q, d) / max(bedLength, 1.0));
  float bed = 1.0 - abs(2.0 * t - 1.0);
  float3 c = lerp(hi.rgb, lo.rgb, bed * bedDepth);
  float k = max(stretch, 0.01);
  float g = fbm3G(q * float2(grainScale / k, grainScale * k));
  c = grainedG(c, g, grainContrast);
  float2 f = fleckG(q, max(speckleCell, 2.0), speckle, seed);
  float3 tone = f.y > 0.5 ? min(hi.rgb * 1.45, float3(1.0, 1.0, 1.0))
                          : lo.rgb * 0.55;
  c = lerp(c, tone, f.x * speckleAlpha);
  return float4(clamp(c, 0.0, 1.0), 1.0);
}
)";

constexpr char kSlangTimber[] = R"(
float4 surface(float2 uv) {
  float2 xy = along > 0.5 ? float2(uv.y, uv.x) : uv;
  float v = clamp(xy.y / max(span, 1.0), 0.0, 1.0);
  v = flip > 0.5 ? 1.0 - v : v;
  float lit = 1.0 - smoothstep(0.0, 0.17, v);
  float shade = smoothstep(0.78, 1.0, v);
  float s = seed * 0.41;
  float x = xy.x * grain + s * 17.3;
  float g = sin(x) * 0.5 + sin(x * 2.31 + s * 3.1) * 0.3 +
            sin(x * 5.77 + s * 7.7) * 0.2;
  g = g * 0.5 + 0.5;
  float fig = smoothstep(0.48, 0.96, g);
  float drift = sin(xy.x * 0.011 + s * 5.0) * 0.5 + 0.5;
  float3 c = lerp(base.rgb, light.rgb, lit * 0.92);
  c = lerp(c, dark.rgb, shade * 0.85);
  c = lerp(c, dark.rgb, fig * figure);
  c = lerp(c, light.rgb, drift * 0.07);
  float k = max(stretch, 0.01);
  float t = fbm3G((xy + s * 53.0) * float2(toothScale / k, toothScale * k));
  c = grainedG(c, t, tooth);
  return float4(clamp(c, 0.0, 1.0), 1.0);
}
)";

constexpr char kSlangLatten[] = R"(
float4 surface(float2 uv) {
  float2 run = to - from;
  float u = clamp(dot(uv - from, run) / max(dot(run, run), 1e-6), 0.0, 1.0);
  float pos = clamp(level + (u - 0.5) * sheen, 0.0, 1.0);
  float3 c = pos < 0.5 ? lerp(shadow.rgb, body.rgb, pos * 2.0)
                       : lerp(body.rgb, light.rgb, (pos - 0.5) * 2.0);
  float2 q = uv + seed * 37.0;
  float t = fbm3G(q * toothScale);
  c = grainedG(c, t, tooth);
  float2 f = fleckG(q, max(patinaCell, 2.0), patina, seed);
  c = lerp(c, patinaColor.rgb, f.x * patinaColor.a);
  return float4(clamp(c, 0.0, 1.0), 1.0);
}
)";

constexpr char kSlangBoard[] = R"(
float4 surface(float2 uv) {
  float2 q = uv + seed * 61.0;
  float k = max(stretch, 0.01);
  float t = fbm3G(q * float2(toothScale / k, toothScale * k));
  float3 c = grainedG(paint.rgb, t, tooth);
  float w = fbm3G(q * wearScale + 11.0);
  c = grainedG(c, w, wear);
  return float4(clamp(c, 0.0, 1.0), 1.0);
}
)";

template <class P>
std::shared_ptr<const Recipe> define(const char* name, const char* sksl,
                                     const char* slang) {
  return std::make_shared<const Recipe>(
      Recipe::of<P>(name)
          .body(Target::SkSL, std::string(kPrelude) + sksl)
          .body(Target::Slang, std::string(kSlangPrelude) + slang));
}

}  // namespace

const std::shared_ptr<const Recipe>& stoneRecipe() {
  static const std::shared_ptr<const Recipe> recipe =
      define<StoneParams>("kit.stone", kStone, kSlangStone);
  return recipe;
}

const std::shared_ptr<const Recipe>& timberRecipe() {
  static const std::shared_ptr<const Recipe> recipe =
      define<TimberParams>("kit.timber", kTimber, kSlangTimber);
  return recipe;
}

const std::shared_ptr<const Recipe>& lattenRecipe() {
  static const std::shared_ptr<const Recipe> recipe =
      define<LattenParams>("kit.latten", kLatten, kSlangLatten);
  return recipe;
}

const std::shared_ptr<const Recipe>& boardRecipe() {
  static const std::shared_ptr<const Recipe> recipe =
      define<BoardParams>("kit.board", kBoard, kSlangBoard);
  return recipe;
}

Material stone(const StoneParams& params) {
  return Material(stoneRecipe(), params);
}

Material timber(const TimberParams& params) {
  return Material(timberRecipe(), params);
}

Material latten(const LattenParams& params) {
  return Material(lattenRecipe(), params);
}

Material board(const BoardParams& params) {
  return Material(boardRecipe(), params);
}

}  // namespace sigil::material::kit
