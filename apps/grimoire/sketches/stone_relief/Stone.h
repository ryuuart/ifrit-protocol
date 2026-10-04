#pragma once

// Porous limestone, rounded shoulders and the cut faces of shaped letters.
// Height changes the normals; the shared material executor supplies the light.

#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/program/Shader.h>

#include <algorithm>
#include <string>
#include <string_view>

namespace stone_relief {

namespace material = sigil::material;

struct Finish {
  float relief = 2.6f;
  float roughness = 0.68f;
  float wear = 0.65f;
  bool directX = false;
  bool mismatchedNormals = false;
  bool operator==(const Finish&) const = default;
};

struct StoneField {
  material::Color tone = {0.77f, 0.71f, 0.60f, 1};
  glm::vec2 origin = {0, 0};
  float radius = 8;
  float shoulder = 5;
  float relief = 2.6f;
  float circular = 0;
  float wear = 0.65f;
  float encoding = 1;
};

inline constexpr std::string_view kStoneField = R"(
float hash21(float2 q) {
    return fract(sin(dot(q, float2(127.1, 311.7))) * 43758.5453);
}
float stoneNoise(float2 q) {
    float2 cell = floor(q), f = fract(q);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21(cell), hash21(cell + float2(1, 0)), f.x),
               mix(hash21(cell + float2(0, 1)),
                   hash21(cell + float2(1, 1)), f.x), f.y);
}
float boundaryDistance(float2 p) {
    float2 size = max(uResolution, float2(1));
    if (circular > 0.5) return length(p - size * 0.5) - min(size.x, size.y) * 0.5;
    float r = min(radius, min(size.x, size.y) * 0.5);
    float2 q = abs(p - size * 0.5) - size * 0.5 + r;
    return length(max(q, float2(0))) + min(max(q.x, q.y), 0.0) - r;
}
float stoneHeight(float2 p) {
    float2 q = p + origin;
    float ramp = smoothstep(0.0, max(shoulder, 0.2), -boundaryDistance(p));
    float pores = smoothstep(0.76, 0.94, stoneNoise(q * 0.43));
    float grain = stoneNoise(q * 0.82) * 0.12 + stoneNoise(q * 0.09) * 0.24;
    float scratch = exp(-abs(sin(q.y * 0.29 + stoneNoise(q * 0.022) * 3.0)) * 24.0);
    return ramp * relief + wear * (grain - pores * 0.48 - scratch * 0.10);
}
)";

inline constexpr std::string_view kStoneAlbedo = R"(
half4 main(float2 p) {
    float2 q = p + origin;
    float broad = stoneNoise(q * 0.014);
    float mineral = stoneNoise(q * 0.105);
    float pores = smoothstep(0.76, 0.94, stoneNoise(q * 0.43));
    float chalk = smoothstep(0.80, 0.96, stoneNoise(q * 0.74));
    float strata = exp(-abs(sin(q.y * 0.028 + broad * 4.0)) * 14.0);
    float value = 0.87 + broad * 0.15 + mineral * 0.08;
    value -= wear * (pores * 0.15 + strata * 0.045);
    float3 colour = tone.rgb * value + chalk * wear * float3(0.045, 0.041, 0.033);
    return half4(half3(clamp(colour, 0.0, 1.0)) * tone.a, tone.a);
}
)";

inline constexpr std::string_view kStoneNormal = R"(
half4 main(float2 p) {
    float step = 0.8;
    float dx = (stoneHeight(p + float2(step, 0)) -
                stoneHeight(p - float2(step, 0))) / (step * 2.0);
    float dy = (stoneHeight(p + float2(0, step)) -
                stoneHeight(p - float2(0, step))) / (step * 2.0);
    float3 n = normalize(float3(-dx, dy * encoding, 1.0));
    return half4(half3(n * 0.5 + 0.5), 1);
}
)";

inline material::Material limestone(StoneField field, const Finish& finish) {
  field.wear = finish.wear;
  field.encoding = finish.directX ? -1.0f : 1.0f;
  return material::shader(std::string(kStoneField) + std::string(kStoneAlbedo),
                          field)
      .surface(
          {.roughness = finish.roughness,
           .occlusion = 0.94f,
           .normal = material::shader(
               std::string(kStoneField) + std::string(kStoneNormal), field),
           .normalDirectX = finish.directX != finish.mismatchedNormals});
}

}  // namespace stone_relief
