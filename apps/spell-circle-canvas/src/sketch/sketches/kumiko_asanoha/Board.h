#pragma once

// BOARD, a grained surface generated per pixel and never from an image: a
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

namespace kumiko_asanoha {

using sigil::material::Color;

/** BOARD: a painted or manila surface — one colour under a fine tooth and
 *  a slow wear, the card a chart is mounted on, the plate a panel is
 *  painted. `tooth` and `toothScale` are the fine grain, `stretch` runs
 *  it one way; `wear` and `wearScale` are the slow blotch that makes one
 *  board differ from the next. */
struct BoardParameters {
  Color paint = {0.91f, 0.89f, 0.84f, 1};
  float tooth = 0.08f;
  float toothScale = 0.045f;
  float stretch = 1.0f;
  float wear = 0.05f;
  float wearScale = 0.006f;
  float seed = 0.0f;
};

inline constexpr std::string_view kBoardSkSL = R"SHADER(
half4 main(float2 p) {
  float2 q = p + seed * 61.0;
  float k = max(stretch, 0.01);
  float t = fbm(q * float2(toothScale / k, toothScale * k));
  float3 c = grained(paint.rgb, t, tooth);
  float w = fbm(q * wearScale + 11.0);
  c = grained(c, w, wear);
  return half4(half3(clamp(c, 0.0, 1.0)), 1.0);
}
)SHADER";

inline sigil::material::Material board(const BoardParameters& parameters = {}) {
  return sigil::material::shader(
      std::string(grainNoise(sigil::material::Target::SkSL)).append(kBoardSkSL), parameters, {.key = "kumiko_asanoha.board"});
}

}  // namespace kumiko_asanoha
