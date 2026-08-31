/** @file
 * The three stacking bodies in SkSL and the builder that fills their
 * slots. Each body reads the mask's red channel, clamps it against the
 * amount uniform, and combines the two operands the blend's way.
 */

#include "sigilmaterial/core/Combine.h"

#include <string>
#include <utility>

namespace sigil::material {

namespace {

/** The mask read: red, scaled by the amount, clamped into 0..1. */
constexpr char kPrelude[] = R"(
half coverage(float2 xy) {
  return half(clamp(float(mask.eval(xy).r) * amount, 0.0, 1.0));
}
)";

constexpr char kMix[] = R"(
half4 main(float2 xy) {
  return mix(base.eval(xy), top.eval(xy), coverage(xy));
}
)";

constexpr char kAdd[] = R"(
half4 main(float2 xy) {
  return base.eval(xy) + top.eval(xy) * coverage(xy);
}
)";

constexpr char kMultiply[] = R"(
half4 main(float2 xy) {
  half4 b = base.eval(xy);
  return mix(b, b * top.eval(xy), coverage(xy));
}
)";

std::shared_ptr<const Recipe> make(Blend blend, const char* body) {
  return std::make_shared<const Recipe>(
      Recipe::of<OverParams>(std::string("over.") + std::string(name(blend)))
          .child("base")
          .child("top")
          .child("mask")
          .body(Target::SkSL, std::string(kPrelude) + body));
}

}  // namespace

std::string_view name(Blend blend) {
  switch (blend) {
    case Blend::Mix:
      return "mix";
    case Blend::Add:
      return "add";
    case Blend::Multiply:
      return "multiply";
  }
  return "?";
}

const std::shared_ptr<const Recipe>& overRecipe(Blend blend) {
  static const std::shared_ptr<const Recipe> mix = make(Blend::Mix, kMix);
  static const std::shared_ptr<const Recipe> add = make(Blend::Add, kAdd);
  static const std::shared_ptr<const Recipe> multiply =
      make(Blend::Multiply, kMultiply);
  switch (blend) {
    case Blend::Add:
      return add;
    case Blend::Multiply:
      return multiply;
    case Blend::Mix:
      break;
  }
  return mix;
}

Material over(Material base, Material top, Material mask, Blend blend) {
  Material out(overRecipe(blend), OverParams{});
  out.child("base", std::move(base));
  out.child("top", std::move(top));
  out.child("mask", std::move(mask));
  return out;
}

const Material* under(const Material& m) {
  for (Blend blend : {Blend::Mix, Blend::Add, Blend::Multiply})
    if (m.recipePtr() == overRecipe(blend))
      if (const Material* base = m.child("base")) return base;
  return &m;
}

int stackDepth(const Material& m) {
  int depth = 0;
  for (const Material* p = &m; under(*p) != p; p = under(*p)) ++depth;
  return depth;
}

}  // namespace sigil::material
