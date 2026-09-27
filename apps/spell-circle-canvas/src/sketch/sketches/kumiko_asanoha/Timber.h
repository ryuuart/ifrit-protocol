#pragma once

// TIMBER, a grained surface generated per pixel and never from an image: a
// ramp of the material's tones, a luminance grain over it and a seeded
// speckle on top. The seed offsets every field, so two pieces at two
// seeds are two pieces of one quarry.

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>

#include <glm/vec2.hpp>
#include <memory>
#include <string>
#include <string_view>

#include "GrainNoise.h"

namespace kumiko_asanoha {

using sigil::material::Color;

/** TIMBER: a planed board, not a dowel — a flat face between a narrow
 *  lit arris and a narrow shadowed one, with grain lines along the piece
 *  and a fine tooth over the face. `span` is the face's width across the
 *  grain in px and `grain` is lines per px along it; `along` turns the
 *  piece to run down local y, so one recipe boards a lattice's rails and
 *  its posts.
 *  @trap Keep `toothScale · stretch` under about a tenth, or the tooth
 *  aliases to hash. */
struct TimberParameters {
  Color base = {0.84f, 0.74f, 0.54f, 1};
  Color light = {0.96f, 0.90f, 0.77f, 1};
  Color dark = {0.56f, 0.42f, 0.23f, 1};
  float span = 24.0f;
  float flip = 0.0f;
  float along = 0.0f;
  float grain = 0.19f;
  float figure = 0.26f;
  float tooth = 0.15f;
  float toothScale = 0.05f;
  float stretch = 2.0f;
  float seed = 0.0f;
};

inline constexpr std::string_view kTimberSkSL = R"SHADER(
half4 main(float2 p) {
  float2 xy = along > 0.5 ? float2(p.y, p.x) : p;
  float v = clamp(xy.y / max(span, 1.0), 0.0, 1.0);
  v = flip > 0.5 ? 1.0 - v : v;
  float lit = 1.0 - smoothstep(0.0, 0.17, v);
  float shade = smoothstep(0.78, 1.0, v);
  float s = seed * 0.41;
  float x = xy.x * grain + s * 17.3;
  float g = sin(x) * 0.5 + sin(x * 2.31 + s * 3.1) * 0.3 +
            sin(x * 5.77 + s * 7.7) * 0.2;
  g = g * 0.5 + 0.5;
  float fig = smoothstep(0.48, 0.96, g);
  float drift = sin(xy.x * 0.011 + s * 5.0) * 0.5 + 0.5;
  float3 c = mix(base.rgb, light.rgb, lit * 0.92);
  c = mix(c, dark.rgb, shade * 0.85);
  c = mix(c, dark.rgb, fig * figure);
  c = mix(c, light.rgb, drift * 0.07);
  float k = max(stretch, 0.01);
  float t = fbm((xy + s * 53.0) * float2(toothScale / k, toothScale * k));
  c = grained(c, t, tooth);
  return half4(half3(clamp(c, 0.0, 1.0)), 1.0);
}
)SHADER";

inline const std::shared_ptr<const sigil::material::Recipe>& timberRecipe() {
  using sigil::material::Recipe;
  using sigil::material::Target;
  static const std::shared_ptr<const Recipe> recipe =
      std::make_shared<const Recipe>(
          Recipe::of<TimberParameters>("kumiko_asanoha.timber")
              .body(Target::SkSL, std::string(grainNoise(Target::SkSL))
                                      .append(kTimberSkSL)));
  return recipe;
}

inline sigil::material::Material timber(const TimberParameters& parameters = {}) {
  return sigil::material::Material(timberRecipe(), parameters);
}

}  // namespace kumiko_asanoha
