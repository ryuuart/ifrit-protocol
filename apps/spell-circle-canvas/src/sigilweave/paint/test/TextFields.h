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

inline sigil::material::Material meshGradient(const SkRect& bounds, float timeSeconds) {
  static const std::shared_ptr<const sigil::material::Recipe> recipe =
      fieldRecipe("text_fields.meshGradient", kMeshGradientSkSL);
  return sigil::material::Material(recipe, fieldParameters(bounds, timeSeconds));
}

}  // namespace text_fields
