/** @file
 * The preset runtime shaders a paint style can carry: each resolves to a
 * shader over the bounds it is asked for. What one of them looks like on a
 * page of type is a picture, and a picture is judged by the plate ledger
 * rather than by an assertion.
 */

#include <gtest/gtest.h>
#include <include/core/SkRect.h>
#include <include/core/SkShader.h>
#include <sigilweave/shaders/PaintShaders.h>

using namespace sigil::weave;

TEST(PaintShaders, EveryPresetResolvesToAShaderOverTheBoundsItIsGiven) {
  const SkRect bounds = SkRect::MakeXYWH(10, 10, 1180, 880);
  EXPECT_NE(PaintShaders::water(bounds, 1.25f), nullptr);
  EXPECT_NE(PaintShaders::meshGradient(bounds, 1.25f), nullptr);
  EXPECT_NE(PaintShaders::sparkle(bounds, 1.25f), nullptr);
}
