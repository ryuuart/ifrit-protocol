/** @file
 * The preset text paints a paint style can carry are SigilMaterial's kit
 * recipes: each shades to a shader over the bounds it is asked for. What
 * one of them looks like on a page of type is a picture, and a picture is
 * judged by the plate ledger rather than by an assertion.
 */

#include <gtest/gtest.h>
#include <include/core/SkRect.h>
#include <include/core/SkShader.h>
#include <sigilmaterial/kit/TextPaint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

namespace {
sk_sp<SkShader> shade(const sigil::material::Material& m) {
  sigil::material::skia::install();
  return sigil::material::skia::shader(m, {});
}
}  // namespace

TEST(TextPaint, EveryPresetResolvesToAShaderOverTheBoundsItIsGiven) {
  const SkRect bounds = SkRect::MakeXYWH(10, 10, 1180, 880);
  EXPECT_NE(shade(sigil::material::kit::water(bounds, 1.25f)), nullptr);
  EXPECT_NE(shade(sigil::material::kit::meshGradient(bounds, 1.25f)), nullptr);
  EXPECT_NE(shade(sigil::material::kit::sparkle(bounds, 1.25f)), nullptr);
}
