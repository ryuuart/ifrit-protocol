#pragma once

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/program/Shader.h>

#include <algorithm>

namespace y2k {

namespace material = sigil::material;

struct MetalRelief {
  float fold = 0.58f;
  /** How far a rougher polish blurs the studio toward its mean grey. */
  float haze = 0.0f;
};

// The shaped sheet's normal field, a choice of this material.
inline constexpr std::string_view kMetalNormal = R"(
half4 main(float2 p) {
    float2 uv = p / max(uResolution, float2(1));
    float x = sin(uv.x * 6.0 + uv.y * 2.0) * fold * 0.72;
    float y = sin(uv.y * 7.0 - uv.x * 2.0) * fold;
    float3 normal = normalize(float3(x, y, 1.0));
    return half4(half3(normal * 0.5 + 0.5), 1);
}
)";

// The studio the folded sheet reflects, across the box it dresses: a
// pale ceiling darkening toward a dark horizon band just under the middle,
// a bright strip along that band, and a lit floor below it, with faint
// softbox panels across. It is read where the sheet's folds lean, so the
// horizon and the panels bend with the metal. The folds are the normal
// field above.
inline constexpr std::string_view kMetalStudio = R"(
half4 main(float2 p) {
    float2 uv = p / max(uResolution, float2(1));
    float leanX = sin(uv.x * 6.0 + uv.y * 2.0) * fold * 0.72;
    float leanY = sin(uv.y * 7.0 - uv.x * 2.0) * fold;
    float2 lean = normalize(float3(leanX, leanY, 1.0)).xy;
    uv -= float2(lean.x, -lean.y) * 0.16;
    float3 ceiling = mix(float3(0.86, 0.88, 0.92), float3(0.16, 0.20, 0.26),
                         smoothstep(0.05, 0.50, uv.y));
    float3 floorLight = mix(float3(0.22, 0.26, 0.32), float3(0.84, 0.88, 0.93),
                            smoothstep(0.54, 0.98, uv.y));
    float3 color = uv.y < 0.52 ? ceiling : floorLight;
    float stripDistance = (uv.y - 0.52) / 0.018;
    color += float3(0.55, 0.60, 0.66) * exp(-stripDistance * stripDistance);
    float panels = 0.5 + 0.5 * cos(uv.x * 18.85 + 0.6);
    color *= 0.90 + 0.10 * smoothstep(0.3, 0.9, panels);
    color = mix(color, float3(0.74, 0.78, 0.84), haze);
    return half4(half3(clamp(color, float3(0.0), float3(1.0))), 1);
}
)";

/** A polished sheet with a folded face reflecting a broad studio. Its
 *  colour is the reflection, so a fill, an ink and a keyline all show it;
 *  the folds and the polish are its surface on a lit body. */
inline material::Material liquidMetal(float fold = 0.58f,
                                      float roughness = 0.16f) {
  // A rougher polish reflects a hazier studio.
  const float haze = std::clamp((roughness - 0.12f) * 4.0f, 0.0f, 0.8f);
  return material::from(material::shader(kMetalStudio, MetalRelief{fold, haze}))
      .surface({.metallic = 1.0f,
                .roughness = roughness,
                .normal = material::shader(kMetalNormal, MetalRelief{fold})});
}

/** A dark, translucent-looking instrument inset with a reflected upper lip. */
inline material::Material smokedGlass() {
  return material::from(
             material::linearGradient({0, 0}, {0, 1},
                                      {{0.0f, material::hexColor(0x263D58)},
                                       {0.25f, material::hexColor(0x102035)},
                                       {0.54f, material::hexColor(0x060D1B)},
                                       {1.0f, material::hexColor(0x1B3552)}}))
      .effects(
          material::Filter::shadow(
              {0, 0, 0, 0.78f}, {.blur = 6, .offset = {0, 3}, .inset = true})
              .then(material::Filter::bevel(
                  {.depth = 1.3f,
                   .size = 0.65f,
                   .highlight = {0.65f, 0.82f, 1, 0.65f},
                   .shadow = {0.01f, 0.025f, 0.055f, 1}}))
              .then(material::Filter::stroke(material::hexColor(0x06101D),
                                             {.width = 1.1f})));
}

/** Fine horizontal brushing over a softly crowned aluminium housing. */
inline material::Material brushedMetal() {
  return material::from(
             material::linearGradient({0, 0}, {0.18f, 1},
                                      {{0.0f, material::hexColor(0xE9EFF3)},
                                       {0.34f, material::hexColor(0xAFBBC5)},
                                       {0.64f, material::hexColor(0xD5DEE4)},
                                       {1.0f, material::hexColor(0x788793)}}))
      .layer(material::noise(0.025f, {.octaves = 2,
                                      .seed = 24,
                                      .grain = true,
                                      .contrast = 1.25f,
                                      .stretch = 8.0f}),
             {.blend = material::BlendMode::SoftLight, .opacity = 0.14f});
}

}  // namespace y2k
