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
#include <sigilgeometry/kit/Solids.h>
#include <sigilmaterial/advanced/Combine.h>
#include <sigilmaterial/advanced/FrameData.h>
#include <sigilmaterial/mask/Mask.h>
#include <sigilmaterial/slang/SlangCompiler.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilworld/advanced/Diligent.h>
#include <sigilworld/advanced/Skia.h>
#include <sigilworld/diligent/Runtime.h>

#include <string>
#include <utility>

#include "DeviceSeams.h"

using namespace sigil;
using namespace sigil::world;

namespace {

constexpr glm::ivec2 kExtent{80, 80};

/** The centre of a card wearing @p surface, rendered on @p runtime —
 *  which is where a mask that is half is half. */
SkColor4f centreOfCard(const material::Material& surface,
                       const Runtime& runtime) {
  return diligent::at(
      diligent::photograph(diligent::card(surface, kExtent), runtime, kExtent),
      0.5f, 0.5f);
}

material::Material red() {
  return material::surface::program({.baseColor = {0.9f, 0.05f, 0.05f, 1.0f}});
}

material::Material blue() {
  return material::surface::program({.baseColor = {0.05f, 0.05f, 0.9f, 1.0f}});
}

}  // namespace

TEST(Stack, ComposesIntoABodyThisTargetCanCompile) {
  // No device needed: the compiler turns a recipe into SPIR-V and the
  // question here is whether the composed text is a program at all.
  world::diligent::installSlangCompiler();

  const material::Material stack =
      material::over(red(), blue(), material::maskConstant(0.5f));
  EXPECT_EQ(stack.recipe().name(),
            material::stackName(material::BlendMode::Normal));
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
      material::over(red(), blue(), material::maskConstant(0.5f)), on.runtime);

  // Half of each, which is neither of them: the stack has to have run
  // both operands' bodies and mixed what they returned.
  EXPECT_GT(base.fR, top.fR + 0.2f) << "the operands are told apart at all";
  EXPECT_LT(mixed.fR, base.fR - 0.1f);
  EXPECT_GT(mixed.fR, top.fR + 0.1f);
  EXPECT_GT(mixed.fB, base.fB + 0.1f);
  EXPECT_LT(mixed.fB, top.fB - 0.1f);

  // …and at the ends of the mask it IS each of them.
  const SkColor4f none = centreOfCard(
      material::over(red(), blue(), material::maskConstant(0.0f)), on.runtime);
  const SkColor4f all = centreOfCard(
      material::over(red(), blue(), material::maskConstant(1.0f)), on.runtime);
  EXPECT_NEAR(none.fR, base.fR, 0.02f);
  EXPECT_NEAR(all.fB, top.fB, 0.02f);
}
namespace {

/** The sampled slots the lit device program of @p surface declares: the
 *  scaffold's own, named with a leading `u`, and its body's. */
std::pair<size_t, size_t> slotsOf(const material::Material& surface) {
  world::diligent::installSlangCompiler();
  const material::Material::Resolved resolved =
      material::inlined(surface).resolve(
          material::Target::Slang, material::FrameData{},
          material::Variant{world::diligent::kVariantLit});
  const auto* program =
      resolved.program ? resolved.program->as<material::slang::SlangProgram>()
                       : nullptr;
  std::pair<size_t, size_t> out{0, 0};
  if (!program) return out;
  for (const std::string& slot : program->compiled().textures)
    ++(slot.starts_with("u") ? out.first : out.second);
  return out;
}

}  // namespace

TEST(Stack, DeclaresOnlyTheSlotsItsOperandsRead) {
  // No device needed: the compiler reports the program's sampled slots.
  const auto [scaffold, one] = slotsOf(red());
  ASSERT_GT(one, 0u);
  const auto [stackScaffold, both] =
      slotsOf(material::over(red(), blue(), material::maskConstant(0.5f)));
  EXPECT_EQ(stackScaffold, scaffold);
  EXPECT_EQ(both, 2 * one) << "each operand's maps, and nothing it never reads";
}

TEST(Stack, MixesAnOperandsSheenByTheMask) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  material::surface::SurfaceParameters plain;
  plain.baseColor = {0.4f, 0.4f, 0.4f, 1};
  plain.roughness = 0.6f;
  material::surface::SurfaceParameters velvet = plain;
  velvet.sheenColor = {1, 1, 1, 1};
  velvet.sheenRoughness = 0.5f;
  // A ball, so the sheen stands at its rim, lit from the eye's side.
  const auto lit = [&](const material::Material& surface) {
    Frame frame(
        group({directionalLight({-0.2f, -0.3f, -1.0f}).key("sun"),
               mesh(geometry::mesh::superellipsoid({50, 50, 50}, 2.0f, 96, 64))
                   .key("ball")
                   .fill(surface)})
            .key("set"));
    frame.extent(kExtent)
        .defaultCamera(diligent::levelEye())
        .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
    const SkBitmap plate = diligent::photograph(frame, on.runtime, kExtent);
    double sum = 0;
    for (int y = 0; y < kExtent.y; ++y)
      for (int x = 0; x < kExtent.x; ++x) sum += plate.getColor4f(x, y).fG;
    return sum;
  };
  const double bare = lit(material::surface::program(plain));
  const double full = lit(material::surface::program(velvet));
  const double half = lit(material::over(material::surface::program(plain),
                                         material::surface::program(velvet),
                                         material::maskConstant(0.5f)));
  ASSERT_GT(full, bare * 1.05) << "the sheen shows at all";
  const double share = (half - bare) / (full - bare);
  EXPECT_GT(share, 0.3) << share;
  EXPECT_LT(share, 0.7) << "half the mask is about half the sheen";
}
