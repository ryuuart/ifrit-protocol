#pragma once

// STONE, a grained surface generated per pixel and never from an image: a
// ramp of the material's tones, a luminance grain over it and a seeded
// speckle on top. The seed offsets every field, so two pieces at two
// seeds are two pieces of one quarry.

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/program/Shader.h>

#include <glm/vec2.hpp>
#include <string>
#include <string_view>

#include "GrainNoise.h"

namespace material_slots {

using sigil::material::Color;

/** STONE: a quarry's two tones on a diagonal bed, veined with grain and
 *  flecked with a speckle in its own colours. The bed runs hi → lo → hi
 *  over `bedLength` px along `bedAngle`, stated in PX rather than in the
 *  box, because a tessera is cut from a slab and its bed does not scale
 *  with the piece. `grainScale` is features per px. */
struct StoneParameters {
  Color hi = {0.87f, 0.84f, 0.77f, 1};
  Color lo = {0.73f, 0.69f, 0.63f, 1};
  float bedAngle = 24.0f;  ///< degrees
  float bedLength = 52.0f;
  float bedDepth = 1.0f;
  float grainScale = 0.055f;
  float grainContrast = 0.35f;
  float stretch = 1.0f;
  float speckle = 0.35f;
  float speckleCell = 8.0f;
  float speckleAlpha = 0.27f;
  float seed = 0.0f;
};

inline constexpr std::string_view kStoneSkSL = R"SHADER(
half4 main(float2 p) {
  float2 q = p + seed * 91.7;
  float a = bedAngle * 0.017453292;
  float2 d = float2(cos(a), sin(a));
  float t = fract(dot(q, d) / max(bedLength, 1.0));
  float bed = 1.0 - abs(2.0 * t - 1.0);
  float3 c = mix(hi.rgb, lo.rgb, bed * bedDepth);
  float k = max(stretch, 0.01);
  float g = fbm(q * float2(grainScale / k, grainScale * k));
  c = grained(c, g, grainContrast);
  float2 f = fleck(q, max(speckleCell, 2.0), speckle, seed);
  float3 tone = f.y > 0.5 ? min(hi.rgb * 1.45, float3(1.0)) : lo.rgb * 0.55;
  c = mix(c, tone, f.x * speckleAlpha);
  return half4(half3(clamp(c, 0.0, 1.0)), 1.0);
}
)SHADER";

inline sigil::material::Material stone(const StoneParameters& parameters = {}) {
  return sigil::material::shader(
      std::string(grainNoise(sigil::material::Target::SkSL)).append(kStoneSkSL),
      parameters, {.key = "material_slots.stone"});
}

}  // namespace material_slots
