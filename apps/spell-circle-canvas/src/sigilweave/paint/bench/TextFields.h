#pragma once

// Animated fields over one ABI of origin, extent, time and motion, as
// programs a glyph run is filled with — the bodies the text paints study
// carries, held here so what this code draws with is its own.

#include <include/core/SkRect.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>

#include <algorithm>
#include <cmath>
#include <glm/vec2.hpp>
#include <memory>
#include <string>
#include <string_view>

namespace text_fields {

/** The ABI every text paint shares: where the run sits, how large it is,
 *  the clock, and a slow two-axis drift derived from it. */
struct TextPaintParameters {
  glm::vec2 origin;
  glm::vec2 extent;
  float time;
  glm::vec2 motion;
};

/** The parameters every field reads, from the box it fills and the
 *  moment: the motion is a slow Lissajous the fields drift along. */
inline TextPaintParameters fieldParameters(const SkRect& bounds,
                                           float timeSeconds) {
  return {{bounds.left(), bounds.top()},
          {std::max(1.0f, bounds.width()), std::max(1.0f, bounds.height())},
          timeSeconds,
          {std::sin(timeSeconds * 0.83f), std::cos(timeSeconds * 0.61f)}};
}

inline std::shared_ptr<const sigil::material::Recipe> fieldRecipe(
    const char* name, std::string_view body) {
  using sigil::material::Recipe;
  using sigil::material::Target;
  return std::make_shared<const Recipe>(
      Recipe::of<TextPaintParameters>(name).body(Target::SkSL,
                                                 std::string(body)));
}

inline constexpr std::string_view kMeshGradientSkSL = R"SHADER(
half4 main(float2 point) {
  float2 uv = (point - origin) / extent;
  float2 q = uv;
  q.x +=
      (q.y - 0.5) * (0.11 * motion.x) + q.y * (1.0 - q.y) * (0.08 * motion.y);
  q.y +=
      (q.x - 0.5) * (0.09 * motion.y) - q.x * (1.0 - q.x) * (0.06 * motion.x);
  float sx = smoothstep(-0.12, 1.12, q.x);
  float sy = smoothstep(-0.12, 1.12, q.y);

  half3 top = mix(half3(0.96, 0.20, 0.42), half3(0.98, 0.67, 0.18), half(sx));
  half3 bottom =
      mix(half3(0.27, 0.12, 0.70), half3(0.02, 0.78, 0.76), half(sx));
  half3 color = mix(top, bottom, half(sy));

  float2 center = float2(0.52 + 0.18 * motion.x, 0.48 + 0.16 * motion.y);
  float bloom = 1.0 - smoothstep(0.0, 0.62, length(q - center));
  color = mix(color, half3(0.53, 0.30, 0.94), half(bloom * 0.34));
  return half4(color, 1.0);
}
)SHADER";
inline constexpr std::string_view kSparkleSkSL = R"SHADER(
float sparkleHash(float2 p) {
  return fract(sin(dot(p, float2(41.3, 289.1))) * 43758.5453123);
}

half4 main(float2 point) {
  float cellSize = 22.0;
  float2 uv = point - origin + float2(time * 3.0, time * 1.3);
  float2 cell = floor(uv / cellSize);
  float2 local = fract(uv / cellSize) - 0.5;

  float exists = step(0.6, sparkleHash(cell + 3.0));
  float sizeSeed = sparkleHash(cell + 11.0);
  float brightSeed = sparkleHash(cell + 19.0);
  float phaseSeed = sparkleHash(cell + 27.0);
  float rateSeed = sparkleHash(cell + 5.0);
  float2 jitter = float2(sparkleHash(cell), sparkleHash(cell + 41.0)) - 0.5;

  float dist = length(local - jitter * 0.7);
  float radius = mix(0.035, 0.14, sizeSeed);
  float twinkleRate = mix(1.1, 3.6, rateSeed) + motion.x * 0.15;
  float twinklePhase = phaseSeed * 6.2831853;
  float twinkle = 0.5 + 0.5 * sin(time * twinkleRate + twinklePhase);
  twinkle = twinkle * twinkle * (3.0 - 2.0 * twinkle);

  float core = smoothstep(radius, 0.0, dist);
  float halo = smoothstep(radius * 5.5, 0.0, dist) * 0.30;
  float brightness =
      (core + halo) * mix(0.6, 2.2, brightSeed) * twinkle * exists;

  half3 tint =
      mix(half3(0.80, 0.88, 1.0), half3(1.0, 0.92, 0.75), half(brightSeed));
  half alpha = half(clamp(brightness, 0.0, 1.0));
  return half4(tint * alpha, alpha);
}
)SHADER";

inline sigil::material::Material meshGradient(const SkRect& bounds, float timeSeconds) {
  static const std::shared_ptr<const sigil::material::Recipe> recipe =
      fieldRecipe("text_fields.meshGradient", kMeshGradientSkSL);
  return sigil::material::Material(recipe, fieldParameters(bounds, timeSeconds));
}

inline sigil::material::Material sparkle(const SkRect& bounds, float timeSeconds) {
  static const std::shared_ptr<const sigil::material::Recipe> recipe =
      fieldRecipe("text_fields.sparkle", kSparkleSkSL);
  return sigil::material::Material(recipe, fieldParameters(bounds, timeSeconds));
}

}  // namespace text_fields
