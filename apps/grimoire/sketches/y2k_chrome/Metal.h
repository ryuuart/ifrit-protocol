#pragma once

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/program/Shader.h>

namespace y2k {

namespace material = sigil::material;

struct MetalRelief {
  float fold = 0.58f;
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

/** A polished sheet with a folded face; on a page it paints its silver flat. */
inline material::Material liquidMetal(float fold = 0.58f,
                                      float roughness = 0.16f) {
  return material::from(material::hexColor(0xE1E7EF))
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
              {0, 0, 0, 0.78f}, {.blur = 6, .offset = {0, 3}, .inside = true})
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
