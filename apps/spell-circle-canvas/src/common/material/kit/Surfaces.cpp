/** @file
 * The gold, chrome and glass bodies in SkSL — the shared prelude of
 * normal decode, equirect lookup and value noise, then each reflection
 * model — and the builders that fill their texture slots.
 */

#include "sigilmaterial/kit/Surfaces.h"

#include <algorithm>
#include <string>

namespace sigil::material::kit {

namespace {

/** Shared helpers: normal decode (the child carries the device
 *  alignment in its own placement) and equirect environment lookup.
 *  Normals are device-space, +y down, +z toward the viewer. */
constexpr char kPrelude[] = R"(
float3 readNormal(float2 xy) {
  half4 t = normals.eval(xy);
  float3 n = float3(t.rgb) * 2.0 - 1.0;
  float len = length(n);
  return len < 0.001 ? float3(0.0, 0.0, 1.0) : n / len;
}

float2 envUv(float3 dir) {
  float u = atan(dir.x, dir.z) / 6.2831853 + 0.5;
  float v = acos(clamp(-dir.y, -1.0, 1.0)) / 3.1415927;
  return float2(u, v);
}

float3 envAt(float2 uv) {
  return float3(env.eval(uv * envSize).rgb);
}

float3 envSample(float3 dir) {
  return envAt(envUv(dir));
}

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

float fbm(float2 p) {
  return vnoise(p) * 0.55 + vnoise(p * 2.13) * 0.3 +
         vnoise(p * 4.41) * 0.15;
}
)";

constexpr char kGold[] = R"(
half4 main(float2 xy) {
  float3 n = readNormal(xy);
  // Foil crinkle: fbm gradient folded into the normal field. The scale
  // is cycles per pixel; forty of them is one noise cell.
  float2 np = xy * max(crinkleScale, 1e-4) * 40.0;
  float e = 0.45;
  float gx = fbm(np + float2(e, 0.0)) - fbm(np - float2(e, 0.0));
  float gy = fbm(np + float2(0.0, e)) - fbm(np - float2(0.0, e));
  n = normalize(float3(n.x + gx * crinkle * 2.2,
                       n.y + gy * crinkle * 2.2, n.z));

  float3 V = float3(0.0, 0.0, 1.0);
  float3 R = reflect(-V, n);
  float3 e3 = envSample(R);
  float ndv = clamp(dot(n, V), 0.0, 1.0);
  float fres = pow(1.0 - ndv, 5.0);
  float3 f0 = tint.rgb;
  // Metals tint their reflection; fresnel whitens the grazing rim.
  float3 spec = e3 * mix(f0, float3(1.0), fres);
  float3 col = f0 * ambient + spec;

  // Glints: sparse hash cells, gated to lit wrinkles.
  float cell = hash21(floor(xy * 0.7));
  float lum = dot(e3, float3(0.299, 0.587, 0.114));
  float glint = step(0.992, cell) * sparkle * smoothstep(0.35, 1.2, lum);
  col += float3(glint);
  return half4(half3(col), 1.0);
}
)";

constexpr char kChrome[] = R"(
half4 main(float2 xy) {
  float3 n = readNormal(xy);
  float3 V = float3(0.0, 0.0, 1.0);
  float3 R = reflect(-V, n);
  float2 uv = envUv(R);

  // Brushed steel: smear the lookup along the azimuth.
  float3 e3 = float3(0.0);
  if (brushed > 0.001) {
    float total = 0.0;
    for (int k = -3; k <= 3; ++k) {
      float w = 1.0 - abs(float(k)) / 4.0;
      float du = float(k) * brushed * 0.02;
      // Jitter breaks the 7-tap banding into grain — the brushed tell.
      du += (hash21(xy + float(k)) - 0.5) * brushed * 0.012;
      e3 += envAt(float2(fract(uv.x + du), uv.y)) * w;
      total += w;
    }
    e3 /= total;
  } else {
    e3 = envAt(uv);
  }

  // The chrome move: crush the environment's midtones.
  e3 = e3 * exposure;
  e3 = (e3 - 0.5) * contrast + 0.5;
  e3 = max(e3, float3(0.0));

  float ndv = clamp(dot(n, V), 0.0, 1.0);
  float face = mix(1.0 - fresnel, 1.0, pow(1.0 - ndv, 3.0));
  float3 col = e3 * tint.rgb * (0.55 + 0.45 * face);
  col += float3(0.9) * pow(1.0 - ndv, 6.0); // hot silhouette rim
  return half4(half3(col), 1.0);
}
)";

constexpr char kGlass[] = R"(
half4 main(float2 xy) {
  float3 n = readNormal(xy);
  // The bevel bends the view ray; the flat interior passes straight
  // through, so refraction reads at the rim — the lens look.
  float2 offset = n.xy * refractPx;
  float3 bg = float3(backdrop.eval(xy + offset).rgb);

  float3 V = float3(0.0, 0.0, 1.0);
  float3 R = reflect(-V, n);
  float3 e3 = envSample(R);
  float ndv = clamp(dot(n, V), 0.0, 1.0);
  float fres = pow(1.0 - ndv, 5.0);

  float3 trans = bg * tint.rgb;
  float mixv = clamp(reflection * (0.06 + 0.94 * fres), 0.0, 1.0);
  float3 col = mix(trans, e3, mixv);
  col += edgeGlow * pow(1.0 - ndv, 2.5) * float3(1.0);
  return half4(half3(col), 1.0) * opacity;
}
)";

glm::vec2 sizeOf(const Environment& env) {
  const SkISize s = env.size();
  return {(float)std::max(s.width(), 1), (float)std::max(s.height(), 1)};
}

}  // namespace

const std::shared_ptr<const Recipe>& goldRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<GoldParams>("gold").child("normals").child("env").body(
          Target::SkSL, std::string(kPrelude) + kGold));
  return recipe;
}

const std::shared_ptr<const Recipe>& chromeRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<ChromeParams>("chrome").child("normals").child("env").body(
          Target::SkSL, std::string(kPrelude) + kChrome));
  return recipe;
}

const std::shared_ptr<const Recipe>& glassRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<GlassParams>("glass")
          .child("normals")
          .child("env")
          .child("backdrop")
          .body(Target::SkSL, std::string(kPrelude) + kGlass));
  return recipe;
}

Material gold(Texture normals, const Environment& env,
              const GoldParams& params) {
  GoldParams p = params;
  p.envSize = sizeOf(env);
  Material m(goldRecipe(), p);
  m.child("normals", std::move(normals));
  m.child("env", env.texture(params.roughness));
  return m;
}

Material chrome(Texture normals, const Environment& env,
                const ChromeParams& params) {
  ChromeParams p = params;
  p.envSize = sizeOf(env);
  Material m(chromeRecipe(), p);
  m.child("normals", std::move(normals));
  m.child("env", env.texture(params.roughness));
  return m;
}

Material glass(Texture normals, const Environment& env, Texture backdrop,
               const GlassParams& params) {
  GlassParams p = params;
  p.envSize = sizeOf(env);
  Material m(glassRecipe(), p);
  m.child("normals", std::move(normals));
  m.child("env", env.texture(params.roughness));
  m.child("backdrop", std::move(backdrop));
  return m;
}

}  // namespace sigil::material::kit
