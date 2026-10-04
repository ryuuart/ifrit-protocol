#pragma once

// Local finish fields: limestone pores, copper brushing and a unit height
// motif.

#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/program/Shader.h>

#include <glm/vec2.hpp>
#include <string>
#include <string_view>
#include <utility>

namespace carved_marks {

namespace material = sigil::material;

inline constexpr std::string_view kNoise = R"(
float hash21(float2 p) {
  return fract(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}
float grain(float2 p) {
  float2 cell = floor(p), f = fract(p);
  f = f * f * (3 - 2 * f);
  return mix(mix(hash21(cell), hash21(cell + float2(1, 0)), f.x),
             mix(hash21(cell + float2(0, 1)),
                 hash21(cell + float2(1, 1)), f.x), f.y);
}
)";

inline material::Material stoneColor() {
  return material::shader(std::string(kNoise) + R"(
half4 main(float2 p) {
  float broad = grain(p * .016);
  float mineral = grain(p * .11);
  float pores = smoothstep(.78, .95, grain(p * .42));
  float strata = exp(-abs(sin(p.y * .028 + broad * 3)) * 18);
  float3 color = mix(float3(.74, .70, .59), float3(.98, .94, .83),
                     .30 + broad * .43 + mineral * .20);
  color -= pores * float3(.12, .11, .08) + strata * .055;
  return half4(color, 1);
})");
}

inline material::Material stoneHeight() {
  return material::shader(std::string(kNoise) + R"(
half4 main(float2 p) {
  float pore = smoothstep(.79, .95, grain(p * .42));
  float height = .48 + .13 * grain(p * .13) + .045 * grain(p * .87)
                 - pore * .26;
  return half4(float3(height), 1);
})");
}

inline material::Material copperColor() {
  return material::shader(std::string(kNoise) + R"(
half4 main(float2 p) {
  float cloud = grain(p * .024);
  float scores = .5 + .5 * sin(p.y * 2.1 + grain(p * .035) * 2
                             + grain(float2(p.x * .013, p.y * .22)) * .65);
  float oxide = smoothstep(.63, .90, grain(p * .13) * .65 + cloud * .35) * .15;
  float3 color = mix(float3(.46, .23, .12), float3(.83, .47, .25),
                     .30 + cloud * .40 + scores * .06);
  color = mix(color, float3(.16, .30, .26), oxide);
  return half4(color, 1);
})");
}

inline material::Material copperHeight() {
  return material::shader(std::string(kNoise) + R"(
half4 main(float2 p) {
  float scores = .5 + .5 * sin(p.y * 2.1 + grain(p * .035) * 2
                             + grain(float2(p.x * .013, p.y * .22)) * .65);
  float hammer = grain(p * .10);
  return half4(float3(.42 + scores * .08 + hammer * .09), 1);
})");
}

inline material::Material shoulder(glm::vec2 extent) {
  struct Parameters {
    glm::vec2 extent;
  };
  return material::shader(R"(
half4 main(float2 p) {
  float2 q = abs(p - extent * .5) - extent * .5 + 10;
  float distance = length(max(q, float2(0))) + min(max(q.x, q.y), 0) - 10;
  float height = smoothstep(0, 5, -distance);
  return half4(float3(height), 1);
})",
                          Parameters{extent});
}

inline material::Material giltMask(material::Material height) {
  return material::shader(std::string(kNoise) + R"(
half4 main(float2 p) {
  float laid = 1 - smoothstep(278, 320, p.x + grain(p * .08) * 28);
  float coverage = float(height.eval(p).r) * laid;
  return half4(float3(coverage), 1);
})",
                          {.textures = {{"height", {}}}})
      .slot("height", std::move(height));
}

inline material::Material roughness(material::Material height, float base) {
  struct Parameters {
    float base;
  };
  return material::shader(R"(
half4 main(float2 p) {
  float marked = float(height.eval(p).r);
  float value = clamp(base - marked * .10, 0, 1);
  return half4(float3(value), 1);
})",
                          Parameters{base}, {.textures = {{"height", {}}}})
      .slot("height", std::move(height));
}

inline material::Material unitHeight(glm::vec2 extent) {
  struct Parameters {
    glm::vec2 extent;
  };
  return material::shader(R"(
half4 main(float2 p) {
  float2 uv = p / max(extent, float2(.0001));
  float ribs = .5 + .5 * sin(uv.x * 25.132741 + sin(uv.y * 6.2831853) * .7);
  float transverse = .5 + .5 * cos(uv.y * 12.566371);
  float height = .22 + ribs * .46 + transverse * .16;
  return half4(float3(height), 1);
})",
                          Parameters{extent});
}

}  // namespace carved_marks
