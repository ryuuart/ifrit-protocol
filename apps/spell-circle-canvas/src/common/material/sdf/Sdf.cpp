/** @file
 * The distance functions and the one-pass shape, border, glow and soft
 * shadow body, one recipe per silhouette kind.
 */

#include "sigilmaterial/sdf/Sdf.h"

#include <sigilio/hub/TextCatalog.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

namespace sigil::material::sdf {

Shape star(int points, float pointiness) {
  const float n = (float)std::max(points, 3);
  Shape s;
  s.kind = Kind::Star;
  s.p0 = n;
  s.p1 = std::clamp(pointiness, 2.0f, n);
  return s;
}

namespace {

constexpr char kShaderPrefix[] = "shader://material/sdf/";

io::TextCatalog& shaders() {
  static io::TextCatalog catalog(kShaderPrefix, SIGIL_MATERIAL_SDF_SHADER_DIR);
  return catalog;
}

std::string shaderSource(std::string_view name) {
  return shaders().text(name).value_or("");
}

std::shared_ptr<const Recipe> make(const char* name,
                                   std::string_view distanceFile) {
  return std::make_shared<const Recipe>(
      Recipe::of<SdfParams>(name)
          .frame(FrameInput::Resolution)
          .body(Target::SkSL,
                shaderSource(distanceFile) + shaderSource("Style.sksl")));
}

}  // namespace

const std::shared_ptr<const Recipe>& recipe(Kind kind) {
  switch (kind) {
    case Kind::RoundBox: {
      static const auto r = make("sdf.roundBox", "RoundBox.sksl");
      return r;
    }
    case Kind::Circle: {
      static const auto r = make("sdf.circle", "Circle.sksl");
      return r;
    }
    case Kind::Star:
    default: {
      static const auto r = make("sdf.star", "Star.sksl");
      return r;
    }
  }
}

float pad(const Style& style) {
  const float glowPad = style.glowRadius > 0 ? style.glowRadius * 3.2f : 0.0f;
  const float shadowPad = style.shadowColor.a > 0
                              ? std::max(std::abs(style.shadowOffset.x),
                                         std::abs(style.shadowOffset.y)) +
                                    style.shadowBlur * 1.5f
                              : 0.0f;
  return style.borderWidth * 0.5f + std::max(glowPad, shadowPad) + 1.0f;
}

Material material(const Shape& shape, const Style& style) {
  return Material(
      recipe(shape.kind),
      SdfParams{pad(style), style.fill, style.borderWidth, style.borderColor,
                style.glowRadius, style.glowColor, style.shadowOffset.x,
                style.shadowOffset.y, style.shadowBlur, style.shadowColor,
                shape.p0, shape.p1, shape.p2});
}

std::vector<Material> everyRecipe() {
  shaders().preload();
  Style dressed;
  dressed.fill = {0.2f, 0.5f, 0.9f, 1};
  dressed.borderWidth = 2;
  dressed.glowRadius = 6;
  dressed.shadowOffset = {2, 3};
  dressed.shadowBlur = 4;
  dressed.shadowColor = {0, 0, 0, 0.5f};
  return {material(roundBox(8), dressed), material(circle(), dressed),
          material(star(5, 3), dressed)};
}

}  // namespace sigil::material::sdf
