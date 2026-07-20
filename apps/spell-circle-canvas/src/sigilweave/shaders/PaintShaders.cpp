#include "sigilweave/PaintShaders.h"

#include <include/core/SkM44.h>
#include <include/core/SkPaint.h>
#include <include/core/SkShader.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>

#include <algorithm>
#include <cmath>

namespace sigil::weave::PaintShaders {

namespace {

sk_sp<SkRuntimeEffect> compileEffect(const char *source) {
  SkRuntimeEffect::Result result =
      SkRuntimeEffect::MakeForShader(SkString(source));
  return std::move(result.effect);
}

void setCommonUniforms(SkRuntimeShaderBuilder &builder, const SkRect &bounds,
                       float timeSeconds) {
  builder.uniform("origin") = SkV2{bounds.left(), bounds.top()};
  builder.uniform("extent") =
      SkV2{std::max(1.0f, bounds.width()), std::max(1.0f, bounds.height())};
  builder.uniform("time") = timeSeconds;
  builder.uniform("motion") =
      SkV2{std::sin(timeSeconds * 0.83f), std::cos(timeSeconds * 0.61f)};
}

const sk_sp<SkRuntimeEffect> &waterEffect() {
  static const sk_sp<SkRuntimeEffect> effect = compileEffect(R"(
    uniform float2 origin;
    uniform float2 extent;
    uniform float time;
    uniform float2 motion;

    half4 main(float2 point) {
      float2 uv = (point - origin) / extent;
      float waveX = sin(uv.y * 18.0 - time * 2.1 + motion.x);
      float waveY = cos(uv.x * 15.0 + time * 1.6 + motion.y);
      float2 warped = uv + float2(waveX, waveY) * 0.035;

      half3 deep = half3(0.015, 0.10, 0.30);
      half3 mid = half3(0.02, 0.42, 0.68);
      half3 aqua = half3(0.08, 0.82, 0.80);
      half3 color = mix(deep, mid, half(smoothstep(-0.05, 0.72, warped.y)));
      color = mix(color, aqua,
                  half(0.30 + 0.22 * sin(warped.x * 8.0 - time * 0.7)));

      float crossing = sin((warped.x + warped.y) * 34.0 + time * 2.0) *
                       cos((warped.x - warped.y) * 27.0 - time * 1.4);
      float caustic = pow(max(0.0, crossing), 7.0);
      color += half3(0.30, 0.48, 0.40) * half(caustic);
      return half4(color, 1.0);
    }
  )");
  return effect;
}

const sk_sp<SkRuntimeEffect> &meshEffect() {
  static const sk_sp<SkRuntimeEffect> effect = compileEffect(R"(
    uniform float2 origin;
    uniform float2 extent;
    uniform float time;
    uniform float2 motion;

    half4 main(float2 point) {
      float2 uv = (point - origin) / extent;
      float2 q = uv;
      q.x += (q.y - 0.5) * (0.11 * motion.x) +
             q.y * (1.0 - q.y) * (0.08 * motion.y);
      q.y += (q.x - 0.5) * (0.09 * motion.y) -
             q.x * (1.0 - q.x) * (0.06 * motion.x);
      float sx = smoothstep(-0.12, 1.12, q.x);
      float sy = smoothstep(-0.12, 1.12, q.y);

      half3 top = mix(half3(0.96, 0.20, 0.42),
                      half3(0.98, 0.67, 0.18), half(sx));
      half3 bottom = mix(half3(0.27, 0.12, 0.70),
                         half3(0.02, 0.78, 0.76), half(sx));
      half3 color = mix(top, bottom, half(sy));

      float2 center = float2(0.52 + 0.18 * motion.x,
                             0.48 + 0.16 * motion.y);
      float bloom = 1.0 - smoothstep(0.0, 0.62, length(q - center));
      color = mix(color, half3(0.53, 0.30, 0.94), half(bloom * 0.34));
      return half4(color, 1.0);
    }
  )");
  return effect;
}

// Each point's size, brightness, phase, and twinkle rate come from a hash of
// its grid cell, so unlike a single global pulse, points fade in and out out
// of sync with their neighbors — closer to a real sky than a uniform strobe.
const sk_sp<SkRuntimeEffect> &sparkleEffect() {
  static const sk_sp<SkRuntimeEffect> effect = compileEffect(R"(
    uniform float2 origin;
    uniform float2 extent;
    uniform float time;
    uniform float2 motion;

    float sparkleHash(float2 p) {
      return fract(sin(dot(p, float2(41.3, 289.1))) * 43758.5453123);
    }

    half4 main(float2 point) {
      float cellSize = 22.0;
      float2 uv = point - origin + float2(time * 3.0, time * 1.3);
      float2 cell = floor(uv / cellSize);
      float2 local = fract(uv / cellSize) - 0.5;

      float exists = step(0.6, sparkleHash(cell + 3.0));
      float sizeSeed = sparkleHash(cell + 11.0);
      float brightSeed = sparkleHash(cell + 19.0);
      float phaseSeed = sparkleHash(cell + 27.0);
      float rateSeed = sparkleHash(cell + 5.0);
      float2 jitter = float2(sparkleHash(cell), sparkleHash(cell + 41.0)) - 0.5;

      float dist = length(local - jitter * 0.7);
      float radius = mix(0.035, 0.14, sizeSeed);
      float twinkleRate = mix(1.1, 3.6, rateSeed) + motion.x * 0.15;
      float twinklePhase = phaseSeed * 6.2831853;
      float twinkle = 0.5 + 0.5 * sin(time * twinkleRate + twinklePhase);
      twinkle = twinkle * twinkle * (3.0 - 2.0 * twinkle);

      float core = smoothstep(radius, 0.0, dist);
      float halo = smoothstep(radius * 5.5, 0.0, dist) * 0.30;
      float brightness =
          (core + halo) * mix(0.6, 2.2, brightSeed) * twinkle * exists;

      half3 tint = mix(half3(0.80, 0.88, 1.0), half3(1.0, 0.92, 0.75),
                       half(brightSeed));
      half alpha = half(clamp(brightness, 0.0, 1.0));
      return half4(tint * alpha, alpha);
    }
  )");
  return effect;
}

// Pablo Roman Andrioli's "Star Nest" (MIT licensed), adapted to this file's
// origin/extent/time/motion uniforms in place of iResolution/iTime/iMouse —
// a volumetric raymarch through drifting fractal dust.
const sk_sp<SkRuntimeEffect> &starNestEffect() {
  static const sk_sp<SkRuntimeEffect> effect = compileEffect(R"(
    uniform float2 origin;
    uniform float2 extent;
    uniform float time;
    uniform float2 motion;

    const int iterations = 17;
    const int volsteps = 20;

    half4 main(float2 fragCoord) {
      float formuparam = 0.53;
      float stepsize = 0.1;
      float zoom = 0.800;
      float tile = 0.850;
      float speed = 0.010;
      float brightness = 0.0015;
      float darkmatter = 0.300;
      float distfading = 0.730;
      float saturation = 0.850;

      float2 uv = (fragCoord - origin) / extent - 0.5;
      uv.y *= extent.y / extent.x;
      float3 dir = float3(uv * zoom, 1.0);
      float t = time * speed + 0.25;

      float a1 = 0.5 + motion.x * 1.5;
      float a2 = 0.8 + motion.y * 1.5;
      float2x2 rot1 = float2x2(cos(a1), sin(a1), -sin(a1), cos(a1));
      float2x2 rot2 = float2x2(cos(a2), sin(a2), -sin(a2), cos(a2));
      dir.xz = rot1 * dir.xz;
      dir.xy = rot2 * dir.xy;
      float3 from = float3(1.0, 0.5, 0.5);
      from += float3(t * 2.0, t, -2.0);
      from.xz = rot1 * from.xz;
      from.xy = rot2 * from.xy;

      float s = 0.1;
      float fade = 1.0;
      float3 v = float3(0.0);
      for (int r = 0; r < volsteps; r++) {
        float3 p = from + s * dir * 0.5;
        p = abs(float3(tile) - mod(p, float3(tile * 2.0)));
        float pa = 0.0;
        float a = 0.0;
        for (int i = 0; i < iterations; i++) {
          p = abs(p) / dot(p, p) - formuparam;
          a += abs(length(p) - pa);
          pa = length(p);
        }
        float dm = max(0.0, darkmatter - a * a * 0.001);
        a *= a * a;
        if (r > 6)
          fade *= 1.0 - dm;
        v += fade;
        v += float3(s, s * s, s * s * s * s) * a * brightness * fade;
        fade *= distfading;
        s += stepsize;
      }
      v = mix(float3(length(v)), v, saturation);
      // The original's 0.01 exposure reads as near-black once masked into
      // thin glyph strokes rather than filling a whole screen; this preset
      // is meant to be loud, so push it several stops brighter.
      return half4(half3(v * 0.055), 1.0);
    }
  )");
  return effect;
}

// Layered ridged/fbm value noise clouds, adapted to this file's uniforms.
const sk_sp<SkRuntimeEffect> &cloudsEffect() {
  static const sk_sp<SkRuntimeEffect> effect = compileEffect(R"(
    uniform float2 origin;
    uniform float2 extent;
    uniform float time;
    uniform float2 motion;

    float2 cloudHash(float2 p) {
      p = float2(dot(p, float2(127.1, 311.7)), dot(p, float2(269.5, 183.3)));
      return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
    }

    float cloudNoise(float2 p) {
      float k1 = 0.366025404;
      float k2 = 0.211324865;
      float2 i = floor(p + (p.x + p.y) * k1);
      float2 a = p - i + (i.x + i.y) * k2;
      float2 o = (a.x > a.y) ? float2(1.0, 0.0) : float2(0.0, 1.0);
      float2 b = a - o + k2;
      float2 c = a - 1.0 + 2.0 * k2;
      float3 h = max(0.5 - float3(dot(a, a), dot(b, b), dot(c, c)), 0.0);
      float3 n = h * h * h * h *
                float3(dot(a, cloudHash(i + 0.0)), dot(b, cloudHash(i + o)),
                       dot(c, cloudHash(i + 1.0)));
      return dot(n, float3(70.0));
    }

    float cloudFbm(float2 n, float2x2 m) {
      float total = 0.0;
      float amplitude = 0.1;
      for (int i = 0; i < 7; i++) {
        total += cloudNoise(n) * amplitude;
        n = m * n;
        amplitude *= 0.4;
      }
      return total;
    }

    half4 main(float2 fragCoord) {
      float cloudscale = 1.1;
      float cloudSpeed = 0.03;
      float clouddark = 0.5;
      float cloudlight = 0.3;
      float cloudcover = 0.2;
      float cloudalpha = 8.0;
      float skytint = 0.5;
      float3 skycolour1 = float3(0.2, 0.4, 0.6);
      float3 skycolour2 = float3(0.4, 0.7, 1.0);
      float2x2 m = float2x2(1.6, 1.2, -1.2, 1.6);

      float2 p = (fragCoord - origin) / extent;
      float2 uv = p * float2(extent.x / extent.y, 1.0);
      float t = time * cloudSpeed;
      float q = cloudFbm(uv * cloudscale * 0.5, m);

      float r = 0.0;
      uv *= cloudscale;
      uv -= q - t;
      float weight = 0.8;
      for (int i = 0; i < 8; i++) {
        r += abs(weight * cloudNoise(uv));
        uv = m * uv + t;
        weight *= 0.7;
      }

      float f = 0.0;
      uv = p * float2(extent.x / extent.y, 1.0);
      uv *= cloudscale;
      uv -= q - t;
      weight = 0.7;
      for (int i = 0; i < 8; i++) {
        f += weight * cloudNoise(uv);
        uv = m * uv + t;
        weight *= 0.6;
      }

      f *= r + f;

      float c = 0.0;
      float t2 = time * cloudSpeed * 2.0;
      uv = p * float2(extent.x / extent.y, 1.0);
      uv *= cloudscale * 2.0;
      uv -= q - t2;
      weight = 0.4;
      for (int i = 0; i < 7; i++) {
        c += weight * cloudNoise(uv);
        uv = m * uv + t2;
        weight *= 0.6;
      }

      float c1 = 0.0;
      float t3 = time * cloudSpeed * 3.0;
      uv = p * float2(extent.x / extent.y, 1.0);
      uv *= cloudscale * 3.0;
      uv -= q - t3;
      weight = 0.4;
      for (int i = 0; i < 7; i++) {
        c1 += abs(weight * cloudNoise(uv));
        uv = m * uv + t3;
        weight *= 0.6;
      }
      c += c1;

      float3 skycolour = mix(skycolour2, skycolour1, p.y);
      float3 cloudcolour =
          float3(1.1, 1.1, 0.9) * clamp(clouddark + cloudlight * c, 0.0, 1.0);

      f = cloudcover + cloudalpha * f * r;

      float3 result =
          mix(skycolour, clamp(skytint * skycolour + cloudcolour, 0.0, 1.0),
              clamp(f + c, 0.0, 1.0));
      return half4(half3(result), 1.0);
    }
  )");
  return effect;
}

// Endless raymarched kaleidoscope tunnel (after a shader by @notargs),
// adapted to this file's uniforms.
const sk_sp<SkRuntimeEffect> &tunnelEffect() {
  static const sk_sp<SkRuntimeEffect> effect = compileEffect(R"(
    uniform float2 origin;
    uniform float2 extent;
    uniform float time;
    uniform float2 motion;

    float tunnelStep(float3 p) {
      p.z -= time * 10.0;
      float a = p.z * 0.1 + motion.x * 0.3;
      float2x2 rot = float2x2(cos(a), sin(a), -sin(a), cos(a));
      p.xy = rot * p.xy;
      return 0.1 - length(cos(p.xy) + sin(p.yz));
    }

    half4 main(float2 fragCoord) {
      float2 uv = fragCoord - origin;
      float3 d = 0.5 - float3(uv, extent.y) / extent.y;
      float3 p = float3(0.0);
      for (int i = 0; i < 32; i++) {
        p += tunnelStep(p) * d;
      }
      float3 color = (sin(p) + float3(2.0, 5.0, 12.0)) / length(p);
      return half4(half3(color), 1.0);
    }
  )");
  return effect;
}

sk_sp<SkShader> makeShader(const sk_sp<SkRuntimeEffect> &effect,
                           const SkRect &bounds, float timeSeconds) {
  if (!effect)
    return nullptr;
  SkRuntimeShaderBuilder builder(effect);
  setCommonUniforms(builder, bounds, timeSeconds);
  return builder.makeShader();
}

} // namespace

sk_sp<SkShader> water(const SkRect &bounds, float timeSeconds) {
  return makeShader(waterEffect(), bounds, timeSeconds);
}

sk_sp<SkShader> meshGradient(const SkRect &bounds, float timeSeconds) {
  return makeShader(meshEffect(), bounds, timeSeconds);
}

sk_sp<SkShader> sparkle(const SkRect &bounds, float timeSeconds) {
  return makeShader(sparkleEffect(), bounds, timeSeconds);
}

sk_sp<SkShader> starNest(const SkRect &bounds, float timeSeconds) {
  return makeShader(starNestEffect(), bounds, timeSeconds);
}

sk_sp<SkShader> clouds(const SkRect &bounds, float timeSeconds) {
  return makeShader(cloudsEffect(), bounds, timeSeconds);
}

sk_sp<SkShader> tunnel(const SkRect &bounds, float timeSeconds) {
  return makeShader(tunnelEffect(), bounds, timeSeconds);
}

} // namespace sigil::weave::PaintShaders
