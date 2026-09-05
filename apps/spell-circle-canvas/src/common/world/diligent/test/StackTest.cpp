/** @file
 * A STACK ON THE DEVICE: `material::over` composed into one body, and
 * what that body then shades.
 *
 * The tier is handed one body per material and cannot reach a child
 * material at all, so a stack for it is a recipe built out of its
 * operands' own definitions. What these hold it to is the two claims
 * that makes: the composed recipe compiles for the target, and the
 * picture it shades where the mask is half is a picture neither operand
 * alone produces.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/core/Combine.h>
#include <sigilmaterial/core/FrameData.h>
#include <sigilmaterial/kit/Mask.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilworld/diligent/Runtime.h>

#include "DeviceSeams.h"

using namespace sigil;
using namespace sigil::world;

namespace {

constexpr SkISize kExtent{80, 80};

/** The centre of a card wearing @p surface, rendered on @p runtime —
 *  which is where a mask that is half is half. */
SkColor4f centreOfCard(const material::Material& surface,
                       const Runtime& runtime) {
  return diligent::at(
      diligent::photograph(diligent::card(surface, kExtent), runtime, kExtent,
                           diligent::levelEye()),
      0.5f, 0.5f);
}

material::Material red() {
  return material::kit::surface({.baseColor = {0.9f, 0.05f, 0.05f, 1.0f}});
}

material::Material blue() {
  return material::kit::surface({.baseColor = {0.05f, 0.05f, 0.9f, 1.0f}});
}

}  // namespace

TEST(Stack, ComposesIntoABodyThisTargetCanCompile) {
  // No device needed: the compiler turns a recipe into SPIR-V and the
  // question here is whether the composed text is a program at all.
  world::diligent::installSlangCompiler();

  const material::Material stack =
      material::over(red(), blue(), material::kit::maskConstant(0.5f));
  EXPECT_EQ(stack.recipe().name(), material::stackName(material::Blend::Mix));
  ASSERT_TRUE(stack.recipe().has(material::Target::Slang))
      << "a stack whose operands all have a body for the target has one too";

  const material::Material::Resolved resolved =
      stack.resolve(material::Target::Slang, material::FrameData{},
                    material::Variant{world::diligent::kVariantLit});
  EXPECT_NE(resolved.program, nullptr);
}

TEST(Stack, ShadesAsNeitherOperandWhereTheMaskIsHalf) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;

  const SkColor4f base = centreOfCard(red(), on.runtime);
  const SkColor4f top = centreOfCard(blue(), on.runtime);
  const SkColor4f mixed = centreOfCard(
      material::over(red(), blue(), material::kit::maskConstant(0.5f)),
      on.runtime);

  // Half of each, which is neither of them: the stack has to have run
  // both operands' bodies and mixed what they returned.
  EXPECT_GT(base.fR, top.fR + 0.2f) << "the operands are told apart at all";
  EXPECT_LT(mixed.fR, base.fR - 0.1f);
  EXPECT_GT(mixed.fR, top.fR + 0.1f);
  EXPECT_GT(mixed.fB, base.fB + 0.1f);
  EXPECT_LT(mixed.fB, top.fB - 0.1f);

  // …and at the ends of the mask it IS each of them.
  const SkColor4f none = centreOfCard(
      material::over(red(), blue(), material::kit::maskConstant(0.0f)),
      on.runtime);
  const SkColor4f all = centreOfCard(
      material::over(red(), blue(), material::kit::maskConstant(1.0f)),
      on.runtime);
  EXPECT_NEAR(none.fR, base.fR, 0.02f);
  EXPECT_NEAR(all.fB, top.fB, 0.02f);
}
