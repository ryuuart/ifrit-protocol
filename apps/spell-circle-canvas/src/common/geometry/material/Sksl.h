#pragma once

/** @file
 * The SkSL sources of the material shaders — the prelude every effect
 * shares (normal decode, equirect environment lookup, value noise) and
 * the gold, chrome and glass bodies. Text only; the effects are
 * compiled where they are used.
 */

namespace sigil::geometry::materials::detail {

/** Shared helpers: normal decode (child carries the device alignment in
 *  its local matrix) and equirect environment lookup. Normals are
 *  DEVICE-space, +y down, +z toward the viewer. */
constexpr char kPrelude[] = R"(
uniform shader uNormals;
uniform shader uEnv;
uniform float2 uEnvSize;

float3 readNormal(float2 xy) {
  half4 t = uNormals.eval(xy);
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
  return float3(uEnv.eval(uv * uEnvSize).rgb);
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
uniform float4 uTint;
uniform float  uCrinkle;
uniform float  uCrinkleScale;
uniform float  uSparkle;
uniform float  uAmbient;

half4 main(float2 xy) {
  float3 n = readNormal(xy);
  // Foil crinkle: fbm gradient folded into the normal field.
  float2 np = xy * uCrinkleScale;
  float e = 0.45;
  float gx = fbm(np + float2(e, 0.0)) - fbm(np - float2(e, 0.0));
  float gy = fbm(np + float2(0.0, e)) - fbm(np - float2(0.0, e));
  n = normalize(float3(n.x + gx * uCrinkle * 2.2,
                       n.y + gy * uCrinkle * 2.2, n.z));

  float3 V = float3(0.0, 0.0, 1.0);
  float3 R = reflect(-V, n);
  float3 env = envSample(R);
  float ndv = clamp(dot(n, V), 0.0, 1.0);
  float fres = pow(1.0 - ndv, 5.0);
  float3 f0 = uTint.rgb;
  // Metals tint their reflection; fresnel whitens the grazing rim.
  float3 spec = env * mix(f0, float3(1.0), fres);
  float3 col = f0 * uAmbient + spec;

  // Glints: sparse hash cells, gated to lit wrinkles.
  float cell = hash21(floor(xy * 0.7));
  float lum = dot(env, float3(0.299, 0.587, 0.114));
  float glint = step(0.992, cell) * uSparkle * smoothstep(0.35, 1.2, lum);
  col += float3(glint);
  return half4(half3(col), 1.0);
}
)";

constexpr char kChrome[] = R"(
uniform float4 uTint;
uniform float  uContrast;
uniform float  uBrushed;
uniform float  uFresnel;
uniform float  uExposure;

half4 main(float2 xy) {
  float3 n = readNormal(xy);
  float3 V = float3(0.0, 0.0, 1.0);
  float3 R = reflect(-V, n);
  float2 uv = envUv(R);

  // Brushed steel: smear the lookup along the azimuth.
  float3 env = float3(0.0);
  if (uBrushed > 0.001) {
    float total = 0.0;
    for (int k = -3; k <= 3; ++k) {
      float w = 1.0 - abs(float(k)) / 4.0;
      float du = float(k) * uBrushed * 0.02;
      // Jitter breaks the 7-tap banding into grain — the brushed tell.
      du += (hash21(xy + float(k)) - 0.5) * uBrushed * 0.012;
      env += envAt(float2(fract(uv.x + du), uv.y)) * w;
      total += w;
    }
    env /= total;
  } else {
    env = envAt(uv);
  }

  // The chrome move: crush the environment's midtones.
  env = env * uExposure;
  env = (env - 0.5) * uContrast + 0.5;
  env = max(env, float3(0.0));

  float ndv = clamp(dot(n, V), 0.0, 1.0);
  float face = mix(1.0 - uFresnel, 1.0, pow(1.0 - ndv, 3.0));
  float3 col = env * uTint.rgb * (0.55 + 0.45 * face) ;
  col += float3(0.9) * pow(1.0 - ndv, 6.0); // hot silhouette rim
  return half4(half3(col), 1.0);
}
)";

constexpr char kGlass[] = R"(
uniform shader uBackdrop;
uniform float4 uTint;
uniform float  uRefract;
uniform float  uReflect;
uniform float  uEdgeGlow;
uniform float  uOpacity;

half4 main(float2 xy) {
  float3 n = readNormal(xy);
  // The bevel bends the view ray; the flat interior passes straight
  // through, so refraction reads at the rim — the lens look.
  float2 offset = n.xy * uRefract;
  float3 bg = float3(uBackdrop.eval(xy + offset).rgb);

  float3 V = float3(0.0, 0.0, 1.0);
  float3 R = reflect(-V, n);
  float3 env = envSample(R);
  float ndv = clamp(dot(n, V), 0.0, 1.0);
  float fres = pow(1.0 - ndv, 5.0);

  float3 trans = bg * uTint.rgb;
  float mixv = clamp(uReflect * (0.06 + 0.94 * fres), 0.0, 1.0);
  float3 col = mix(trans, env, mixv);
  col += uEdgeGlow * pow(1.0 - ndv, 2.5) * float3(1.0);
  return half4(half3(col), 1.0) * uOpacity;
}
)";
}  // namespace sigil::geometry::materials::detail
