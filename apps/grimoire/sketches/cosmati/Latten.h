#pragma once

// LATTEN, a grained surface generated per pixel and never from an image: a
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

namespace cosmati {

using sigil::material::Color;

/** LATTEN: sheet brass under one light. Brass has ONE colour and many
 *  lights, so the material is a LADDER of three tones and a piece's
 *  `level` is where on it the piece sits. The light lays a SHEEN across
 *  the run from `from` to `to`, two points in the paint's own
 *  coordinates — a node's px, or the root's when the paint is anchored
 *  there, which is how one light crosses two hundred nodes of one
 *  instrument. */
struct LattenParameters {
  Color shadow = {0.36f, 0.27f, 0.18f, 1};
  Color body = {0.63f, 0.53f, 0.26f, 1};
  Color light = {1.0f, 0.86f, 0.55f, 1};
  glm::vec2 from = {0.0f, 0.0f};
  glm::vec2 to = {1.0f, 1.0f};
  float level = 0.5f;
  float sheen = 0.1f;
  float tooth = 0.08f;
  float toothScale = 0.9f;
  float patina = 0.0f;
  float patinaCell = 26.0f;
  Color patinaColor = {0.18f, 0.35f, 0.27f, 0.09f};
  float seed = 0.0f;
};

inline constexpr std::string_view kLattenSkSL = R"SHADER(
half4 main(float2 p) {
  float2 run = to - from;
  float u = clamp(dot(p - from, run) / max(dot(run, run), 1e-6), 0.0, 1.0);
  float along = clamp(level + (u - 0.5) * sheen, 0.0, 1.0);
  float3 c = along < 0.5 ? mix(shadow.rgb, body.rgb, along * 2.0)
                         : mix(body.rgb, light.rgb, (along - 0.5) * 2.0);
  float2 q = p + seed * 37.0;
  float t = fbm(q * toothScale);
  c = grained(c, t, tooth);
  float2 f = fleck(q, max(patinaCell, 2.0), patina, seed);
  c = mix(c, patinaColor.rgb, f.x * patinaColor.a);
  return half4(half3(clamp(c, 0.0, 1.0)), 1.0);
}
)SHADER";

inline sigil::material::Material latten(
    const LattenParameters& parameters = {}) {
  return sigil::material::shader(
      std::string(grainNoise(sigil::material::Target::SkSL))
          .append(kLattenSkSL),
      parameters, {.key = "cosmati.latten"});
}

}  // namespace cosmati
