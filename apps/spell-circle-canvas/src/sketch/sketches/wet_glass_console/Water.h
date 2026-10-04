#pragma once

#include <sigilmaterial/program/Shader.h>

#include <algorithm>
#include <glm/vec2.hpp>
#include <string_view>

namespace wet_glass {

struct WaterParameters {
  glm::vec2 boxSize{1, 1};
  float bend = 12;
  float capHeight = 0.72f;
  float roughness = 0.035f;
  float opacity = 0.42f;
  float film = 0;
};

inline WaterParameters parameters(glm::vec2 size, float bend, float capHeight,
                                  float roughness, float opacity,
                                  float film = 0) {
  return {.boxSize = {std::max(size.x, 1.0f), std::max(size.y, 1.0f)},
          .bend = std::clamp(bend, 0.0f, 18.0f),
          .capHeight = std::clamp(capHeight, 0.0f, 1.3f),
          .roughness = std::clamp(roughness, 0.0f, 1.0f),
          .opacity = std::clamp(opacity, 0.0f, 1.0f),
          .film = std::clamp(film, 0.0f, 1.0f)};
}

// The strongest bend, colour separation and diffusion together stay inside
// forty local pixels. Moving the node moves this fixed optical profile.
inline constexpr std::string_view kRefraction = R"(
half4 main(float2 p) {
    float2 extent = max(boxSize, float2(1));
    float2 q = clamp((p - extent * 0.5) / (extent * 0.5), float2(-1), float2(1));
    float r2 = min(dot(q, q), 1.0);
    float2 capShift = -q * bend * (0.45 + r2 * 0.55);
    float2 filmShift = float2(sin(p.y * 0.047), sin(p.x * 0.063)) * bend * 0.22;
    float2 shift = mix(capShift, filmShift, film) * opacity;
    float fringe = min(bend * 0.045, 0.81) * opacity;
    half4 center = content.eval(p + shift);
    half4 red = content.eval(p + shift + float2(fringe, 0));
    half4 blue = content.eval(p + shift - float2(fringe, 0));
    half4 result = half4(red.r, center.g, blue.b, center.a);
    float spread = roughness * opacity * 3.0;
    if (spread > 0.01) {
        half4 soft = (content.eval(p + shift + float2(spread, spread)) +
                      content.eval(p + shift - float2(spread, spread))) * 0.5;
        result = mix(result, soft, half(roughness * opacity));
    }
    return result;
}
)";

inline constexpr std::string_view kCapNormal = R"(
half4 main(float2 p) {
    float2 extent = max(boxSize, float2(1));
    float2 q = (p - extent * 0.5) / (extent * 0.5);
    float z = sqrt(max(1.0 - min(dot(q, q), 0.99), 0.04));
    float2 cap = float2(q.x, -q.y) * capHeight / max(z, 0.2);
    float2 streak = float2(cos(p.y * 0.047), -cos(p.x * 0.063)) * 0.10;
    float3 normal = normalize(float3(mix(cap, streak, film), 1.0));
    return half4(half3(normal * 0.5 + 0.5), 1);
}
)";

inline constexpr std::string_view kGlazing = R"(
float gaussian(float value) { return exp(-value * value); }
half4 main(float2 p) {
    float2 extent = max(boxSize, float2(1));
    float2 q = (p - extent * 0.5) / (extent * 0.5);
    float r = length(q);
    float inner = max(0.0, 1.0 - r);
    float rim = exp(-inner * min(extent.x, extent.y) * 0.33);
    float darkBand = gaussian((r - 0.89) / 0.044);
    float highlight = gaussian((q.x * 0.58 + q.y * 0.81 + 0.49) / 0.095);
    highlight *= smoothstep(-0.12, 0.75, -q.y);
    float fine = gaussian((q.x * 0.58 + q.y * 0.81 + 0.62) / 0.018);
    fine *= smoothstep(0.15, 0.85, -q.y);
    float streak = pow(0.5 + 0.5 * sin(p.x * 0.15 + sin(p.y * 0.023)), 9.0);
    float3 tint = mix(float3(0.55, 0.82, 0.87), float3(0.92, 0.99, 1.0),
                      max(highlight, fine));
    tint = mix(tint, float3(0.015, 0.060, 0.078), darkBand * 0.62);
    float beadAlpha = 0.018 + rim * 0.45 + highlight * 0.29 + fine * 0.48;
    float filmAlpha = 0.028 + streak * 0.055 + roughness * 0.07;
    float alpha = clamp(mix(beadAlpha, filmAlpha, film) * opacity, 0.0, 1.0);
    alpha = mix(alpha, 1.0, pow(opacity, 12.0));
    return half4(half3(tint * alpha), half(alpha));
}
)";

inline constexpr std::string_view kPanelNormal = R"(
half4 main(float2 p) {
    float2 size = max(uResolution, float2(1));
    float2 slope = float2(exp(-p.x / 5.0) - exp(-(size.x - p.x) / 5.0),
                         -exp(-p.y / 5.0) + exp(-(size.y - p.y) / 5.0));
    float3 normal = normalize(float3(slope * 0.52, 1.0));
    return half4(half3(normal * 0.5 + 0.5), 1);
}
)";

inline constexpr std::string_view kWindow = R"(
float gaussian(float value) { return exp(-value * value); }
half4 main(float2 p) {
    float2 extent = max(uResolution, float2(1));
    float2 uv = p / extent;
    float edge = min(min(p.x, extent.x - p.x), min(p.y, extent.y - p.y));
    float rim = exp(-max(edge, 0.0) / 1.8);
    float stripe = gaussian((uv.x * 0.60 + uv.y * 0.34 - 0.46) / 0.095);
    float arc = length((uv - float2(0.39, 0.51)) * float2(1.0, 1.4));
    float wipe = gaussian((arc - 0.49) / 0.005) *
                 (1.0 - smoothstep(0.20, 0.65, uv.y)) * smoothstep(0.1, 0.55, uv.x);
    float alpha = 0.008 + stripe * 0.028 + rim * 0.17 + wipe * 0.032;
    return half4(half3(float3(0.58, 0.82, 0.89) * alpha), half(alpha));
}
)";

inline constexpr std::string_view kDisplay = R"(
float coverage(float d, float width, float footprint) {
    float low = max(d - footprint * 0.5, -width);
    float high = min(d + footprint * 0.5, width);
    return clamp((high - low) / footprint, 0.0, 1.0);
}
half4 main(float2 p) {
    float footprint = 1.0 / max(uContentScale, 0.125);
    float2 grid = abs(mod(p + 12.0, 24.0) - 12.0);
    float gx = coverage(grid.x, 0.35, footprint);
    float gy = coverage(grid.y, 0.35, footprint);
    float line = max(gx, gy);
    float3 color = mix(float3(0.012, 0.028, 0.042), float3(0.024, 0.065, 0.080), line);
    float fine = 0.5 + 0.5 * sin(p.y * 3.14159265);
    color += float3(0.001, 0.003, 0.004) * fine / max(footprint, 1.0);
    return half4(half3(color), 1);
}
)";

struct TraceParameters {
  glm::vec2 boxSize{1, 1};
  float channel = 0;
};

inline constexpr std::string_view kTrace = R"(
half4 main(float2 p) {
    float2 size = max(boxSize, float2(1));
    float x = p.x / size.x;
    float time = uTime * 0.31 + channel * 2.3;
    float wave = sin(x * 19.0 - time) * 0.18 +
                 sin(x * 41.0 + time * 0.73) * 0.055 +
                 sin(x * 7.0 + time * 1.27) * 0.09;
    float target = size.y * (0.50 + wave);
    float d = abs(p.y - target);
    float footprint = 1.0 / max(uContentScale, 0.125);
    float trace = 1.0 - smoothstep(0.75, 0.75 + footprint, d);
    float glow = exp(-d / 5.0) * 0.14;
    float below = smoothstep(target - footprint, target + footprint, p.y) *
                  (1.0 - p.y / size.y) * 0.10;
    float cursor = 1.0 - smoothstep(0.55, 0.55 + footprint,
                                  abs(p.x - mod(uTime * 19.0, size.x)));
    float3 color = mix(float3(0.12, 0.84, 0.78), float3(0.95, 0.64, 0.23), channel);
    float alpha = clamp(trace + glow + below + cursor * 0.15, 0.0, 1.0);
    return half4(half3(color * alpha), half(alpha));
}
)";

inline constexpr std::string_view kCalibration = R"(
float coverage(float d, float width, float footprint) {
    float low = max(d - footprint * 0.5, -width);
    float high = min(d + footprint * 0.5, width);
    return clamp((high - low) / footprint, 0.0, 1.0);
}
half4 main(float2 p) {
    float footprint = 1.0 / max(uContentScale, 0.125);
    float2 cell = abs(mod(p + 8.0, 16.0) - 8.0);
    float grid = max(coverage(cell.x, 0.5, footprint), coverage(cell.y, 0.5, footprint));
    float checker = mod(floor(p.x / 32.0) + floor(p.y / 32.0), 2.0);
    float3 color = mix(float3(0.075, 0.16, 0.19), float3(0.095, 0.20, 0.23), checker);
    color = mix(color, float3(0.34, 0.56, 0.59), grid * 0.55);
    float2 q = mod(p + 48.0, 96.0) - 48.0;
    float cross = max(coverage(abs(q.x), 0.75, footprint) *
                      coverage(abs(q.y), 12.0, footprint),
                      coverage(abs(q.y), 0.75, footprint) *
                      coverage(abs(q.x), 12.0, footprint));
    float ring = coverage(abs(length(q) - 20.0), 0.7, footprint);
    color = mix(color, float3(0.64, 0.90, 0.88), max(cross, ring));
    return half4(half3(color), 1);
}
)";

}  // namespace wet_glass
