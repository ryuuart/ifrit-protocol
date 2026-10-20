#pragma once

/** @file
 * A SHADING TERM, EVALUATED, for the cases that hold the terms to what
 * they have to answer. A body is compiled through the Skia backend and
 * drawn over a float surface, so what comes back is what the term
 * computed rather than what an 8-bit channel could hold: one texel for a
 * single expression, a grid of texels for a body that reads its argument
 * off its pixel's coordinates. The recipes are named from one counter
 * shared by every file that includes this, so no two bodies in one binary
 * stand under the same name.
 */

#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/advanced/Terms.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

#include <cstddef>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <string>
#include <vector>

namespace sigil::material::test {

/** A recipe whose parameters are one number nothing reads: what every
 *  evaluated body stands on, since a term takes its arguments in the
 *  expression rather than from parameters. */
struct NoParameters {
  float unused = 0;
};

/** ONE BODY, EVALUATED: compiled through the Skia backend and drawn over
 *  one texel of a float surface. */
inline SkColor4f shadeBody(const std::string& body) {
  static int serial = 0;
  const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<NoParameters>("term." + std::to_string(serial++))
          .body(Target::SkSL, body));
  sk_sp<SkShader> shader = skia::shader(Material(recipe, NoParameters{}), {});
  EXPECT_TRUE(shader) << body;
  if (!shader) return {0, 0, 0, 0};
  sk_sp<SkSurface> surface = SkSurfaces::Raster(
      SkImageInfo::Make(1, 1, kRGBA_F32_SkColorType, kPremul_SkAlphaType));
  SkPaint paint;
  paint.setShader(shader);
  paint.setBlendMode(SkBlendMode::kSrc);
  surface->getCanvas()->drawPaint(paint);
  float px[4] = {0, 0, 0, 0};
  const SkImageInfo one =
      SkImageInfo::Make(1, 1, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  EXPECT_TRUE(surface->readPixels(SkPixmap(one, px, sizeof(px)), 0, 0));
  return {px[0], px[1], px[2], px[3]};
}

/** ONE TERM, EVALUATED: the term source is prepended to a body that
 *  returns the expression asked about, which names all four channels. */
inline SkColor4f term(const std::string& expression) {
  return shadeBody(termsSource(Target::SkSL) +
                   "half4 main(float2 xy) { return half4(" + expression +
                   "); }");
}

/** The red channel of a term that answers one number. */
inline float scalar(const std::string& expression) {
  return term(expression + ", 0.0, 0.0, 1.0").fR;
}

/** The split sum's table at @p roughness and @p nDotV, spelled as the
 *  argument a term takes it as. */
inline std::string brdfAt(float roughness, float nDotV) {
  const glm::vec2 ab = environmentBrdf(roughness, nDotV);
  return "float2(" + std::to_string(ab.x) + ", " + std::to_string(ab.y) + ")";
}

/** The four channels a term answers, as the shader computes them. */
inline glm::vec3 shaded(const std::string& expression) {
  const SkColor4f c = term(expression + ", 1.0");
  return {c.fR, c.fG, c.fB};
}

/** EVERY PIXEL OF ONE BODY over an @p n by @p n float surface, row by
 *  row: what a term answers at each of n² arguments the body reads off
 *  its pixel's coordinates. */
inline std::vector<SkColor4f> evaluated(const std::string& body, int n) {
  static int serial = 0;
  const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<NoParameters>("terms.grid." + std::to_string(serial++))
          .body(Target::SkSL, termsSource(Target::SkSL) + body));
  sk_sp<SkShader> shader = skia::shader(Material(recipe, NoParameters{}), {});
  EXPECT_TRUE(shader) << body;
  std::vector<SkColor4f> out((size_t)n * n, SkColor4f{0, 0, 0, 0});
  if (!shader) return out;
  const SkImageInfo info =
      SkImageInfo::Make(n, n, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
  SkPaint paint;
  paint.setShader(shader);
  paint.setBlendMode(SkBlendMode::kSrc);
  surface->getCanvas()->drawPaint(paint);
  EXPECT_TRUE(surface->readPixels(
      SkPixmap(info, out.data(), (size_t)n * sizeof(SkColor4f)), 0, 0));
  return out;
}

}  // namespace sigil::material::test
