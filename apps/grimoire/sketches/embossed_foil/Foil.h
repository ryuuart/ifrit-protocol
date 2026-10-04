#pragma once

#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/program/Shader.h>

#include <algorithm>
#include <string>
#include <string_view>

namespace embossed_foil {

struct Parameters {
  float scale = 1;
  float strength = 0.58f;
  float roughness = 0.24f;
  float variation = 0.12f;
  float seed = 11;
};

// Triangular interpolation leaves broad planar facets. The second scale and
// narrow warped fold add smaller creases without a repeating tile boundary.
inline constexpr std::string_view kHeight = R"(
float hash(float2 p) {
    return fract(sin(dot(p, float2(127.1, 311.7)) + seed * 17.3) * 43758.5453);
}
float triangular(float2 p) {
    float2 cell = floor(p);
    float2 f = fract(p);
    float a = hash(cell);
    float b = hash(cell + float2(1, 0));
    float c = hash(cell + float2(0, 1));
    float d = hash(cell + float2(1, 1));
    if (f.x + f.y < 1.0) return a + (b - a) * f.x + (c - a) * f.y;
    return d + (c - d) * (1.0 - f.x) + (b - d) * (1.0 - f.y);
}
float height(float2 p) {
    float2 q = (p + float2(seed * 13.0, seed * 7.0)) * scale;
    float broad = triangular(float2(q.x * 0.93 + q.y * 0.29,
                                     q.y * 0.89 - q.x * 0.17) / 76.0) * 15.0;
    float fine = triangular(float2(q.x * 0.81 - q.y * 0.46,
                                    q.y * 1.13 + q.x * 0.34) / 25.0) * 4.2;
    float crease = sin(q.x * 0.018 + q.y * 0.027 +
                       sin(q.y * 0.013 + seed) * 1.6);
    float fold = exp(-abs(crease) * 15.0) * 3.8;
    float micro = sin(q.x * 0.17 + sin(q.y * 0.072) * 2.4) * 0.25;
    return broad + fine + fold + micro;
}
)";

inline const std::string kNormal = std::string(kHeight) + R"(
half4 main(float2 p) {
    float footprint = max(0.55, 0.65 / max(uContentScale, 0.125));
    float dx = (height(p + float2(footprint, 0)) -
                height(p - float2(footprint, 0))) / (2.0 * footprint);
    float dy = (height(p + float2(0, footprint)) -
                height(p - float2(0, footprint))) / (2.0 * footprint);
    float3 normal = normalize(float3(-dx * strength, dy * strength, 1.0));
    return half4(half3(normal * 0.5 + 0.5), 1);
}
)";

inline const std::string kRoughness = std::string(kHeight) + R"(
half4 main(float2 p) {
    float h = height(p);
    float stress = abs(sin(h * 0.71 + p.y * 0.018));
    float value = clamp(roughness + variation * (stress - 0.5), 0.0, 1.0);
    return half4(half3(value), 1);
}
)";

inline constexpr std::string_view kFilmMask = R"(
half4 main(float2 p) {
    float2 size = max(uResolution, float2(1));
    float2 uv = p / size;
    float fold = 0.5 + 0.5 * sin(p.x * 0.018 + p.y * 0.031 +
                               sin(p.x * 0.004 - p.y * 0.009) * 2.1);
    float edge = smoothstep(0.05, 0.42, uv.x + uv.y * 0.23);
    float value = mix(0.27, 0.91, edge) + fold * 0.08;
    return half4(half3(value), 1);
}
)";

inline constexpr std::string_view kOverprintMask = R"(
half4 main(float2 p) {
    float2 uv = p / max(uResolution, float2(1));
    float band = smoothstep(0.39, 0.44, uv.x + uv.y * 0.20) *
                 (1.0 - smoothstep(0.59, 0.64, uv.x + uv.y * 0.20));
    float edgeWidth = max(0.03, 0.5 / (18.0 * max(uContentScale, 0.125)));
    float perforation = smoothstep(0.33 - edgeWidth, 0.33 + edgeWidth,
        length(fract(p / float2(18, 18)) - 0.5));
    float alpha = band * perforation;
    return half4(half3(alpha), half(alpha));
}
)";

// A latitude-longitude studio field: broad cool surround, two soft boxes,
// a warm sun strip and a subdued blue planet rim. It rotates as one light.
inline constexpr std::string_view kEnvironment = R"(
float gaussian(float value) { return exp(-value * value); }
half4 main(float2 p) {
    float2 uv = p / float2(512, 256);
    float longitude = uv.x * 6.2831853;
    float latitude = (uv.y - 0.5) * 3.14159265;
    float a = gaussian(sin((longitude - 1.0) * 0.5) / 0.32) *
              gaussian((latitude + 0.29) / 0.70);
    float b = gaussian(sin((longitude - 4.6) * 0.5) / 0.42) *
              gaussian((latitude - 0.21) / 0.80);
    float horizon = gaussian((latitude - 0.18) / 0.065);
    float sun = gaussian(sin((longitude - 2.9) * 0.5) / 0.034) *
                gaussian((latitude + 0.65) / 0.09);
    float3 color = float3(0.62, 0.67, 0.75) +
                   float3(1.18, 1.27, 1.43) * a +
                   float3(1.22, 1.08, 0.89) * b +
                   float3(0.06, 0.19, 0.35) * horizon +
                   float3(1.4, 1.15, 0.73) * sun;
    return half4(half3(color), 1);
}
)";

inline constexpr std::string_view kTelemetry = R"(
half4 main(float2 p) {
    float2 size = max(uResolution, float2(1));
    float x = p.x / size.x;
    float temperature = sin(x * 8.0 - uTime * 0.12) * 0.19 +
                        sin(x * 26.0 + uTime * 0.27) * 0.035;
    float target = size.y * (0.52 + temperature);
    float footprint = 1.0 / max(uContentScale, 0.125);
    float line = 1.0 - smoothstep(0.65, 0.65 + footprint, abs(p.y - target));
    float glow = exp(-abs(p.y - target) / 5.0) * 0.12;
    float under = smoothstep(target - footprint, target + footprint, p.y) *
                  (1.0 - p.y / size.y) * 0.1;
    float alpha = clamp(line + glow + under, 0.0, 1.0);
    return half4(half3(float3(0.24, 0.70, 0.84) * alpha), half(alpha));
}
)";

inline constexpr std::string_view kGrid = R"(
half4 main(float2 p) {
    float footprint = 1.0 / max(uContentScale, 0.125);
    float2 d = abs(mod(p + 12.0, 24.0) - 12.0);
    float2 low = max(d - footprint * 0.5, float2(-0.55));
    float2 high = min(d + footprint * 0.5, float2(0.55));
    float2 coverage = clamp((high - low) / footprint, float2(0), float2(1));
    float grid = max(coverage.x, coverage.y);
    float3 color = mix(float3(0.012, 0.028, 0.041),
                       float3(0.058, 0.101, 0.126), grid);
    return half4(half3(color), 1);
}
)";

}  // namespace embossed_foil
