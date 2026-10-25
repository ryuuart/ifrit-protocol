#pragma once

/** @file
 * WHETHER A RECIPE'S DEVICE BODY STANDS AS A SURFACE: its Slang source,
 * declarations and body together, compiled under a vertex stage that
 * hands it a texture coordinate and a fragment stage that answers what
 * its `surface` does at it — the contract a device renderer calls a
 * body by.
 */

#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/slang/SlangCompiler.h>

#include <string>

namespace sigil::material::test {

/** The compiled surface of @p recipe, or false with the compiler's
 *  diagnostics in @p error. */
inline bool compilesAsSurface(const Recipe& recipe, slang::Compiled* built,
                              std::string* error) {
  if (!recipe.has(Target::Slang)) {
    *error = "the recipe has no Slang body";
    return false;
  }
  const std::string source = recipe.source(Target::Slang) + R"SLANG(
struct VSOut { float4 position : SV_Position; float2 uv : TEXCOORD0; };

[shader("vertex")]
VSOut vsSurface(uint id : SV_VertexID) {
  VSOut out;
  out.position = float4(float(id), 0, 0, 1);
  out.uv = float2(0.5, 0.5);
  return out;
}

[shader("fragment")]
float4 fsSurface(VSOut input) : SV_Target {
  return surface(input.uv);
}
)SLANG";
  auto compiled =
      slang::compileModule(source, "vsSurface", "fsSurface", /*lit=*/false);
  if (!compiled) {
    if (error) *error = std::move(compiled.error().message);
    return false;
  }
  *built = std::move(*compiled);
  return true;
}

}  // namespace sigil::material::test
