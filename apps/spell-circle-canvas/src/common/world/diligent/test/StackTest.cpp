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
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/core/Combine.h>
#include <sigilmaterial/core/FrameData.h>
#include <sigilmaterial/kit/Mask.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilgeometry/device/Device.h>
#include <sigilworld/diligent/Runtime.h>
#include <sigilworld/scene/Scene.h>

#include <memory>
#include <string>

using namespace sigil;
using namespace sigil::world;

namespace {

constexpr SkISize kExtent{80, 80};

struct OnDevice {
  std::unique_ptr<geometry::device::Device> device;
  Runtime runtime;
  std::string error;
  explicit operator bool() const { return (bool)device; }
};

OnDevice onDevice() {
  OnDevice out;
  const geometry::device::DeviceConfig config;
  out.device = geometry::device::Device::create(config, &out.error);
  if (out.device) out.runtime = world::diligent::runtime(*out.device);
  return out;
}

Camera eye() {
  Camera camera;
  camera.eye = {0, 0, 200};
  camera.target = {0, 0, 0};
  return camera;
}

/** A card facing the camera, wearing @p surface, lit by one sun. */
Frame card(const material::Material& surface) {
  namespace gm = ::sigil::geometry::mesh;
  Element root =
      Element()
          .key("set")
          .child(Element().key("sun").light(light::sun({-0.2f, -0.3f, -1.0f})))
          .child(Element().key("card").mesh(gm::quad(120, 120)).fill(surface));
  Frame frame(root);
  frame.extent(kExtent).camera(eye()).pass(
      geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  return frame;
}

SkColor4f centreOf(const Frame& frame, const Runtime& runtime) {
  motion::Ticker ticker;
  Scene scene(ticker);
  Frame copy = frame;
  copy.runtime(runtime);
  ticker.tick(1.0 / 60.0);
  scene.render(copy);

  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(kExtent.width(), kExtent.height()));
  SkCanvas canvas(bitmap);
  canvas.clear(SK_ColorBLACK);
  scene.draw(canvas, eye());
  return bitmap.getColor4f(kExtent.width() / 2, kExtent.height() / 2);
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

  // The operands are still its children, so the walk down is unchanged
  // by having been composed.
  EXPECT_EQ(material::stackDepth(stack), 1);
  ASSERT_NE(material::under(stack), &stack);
  EXPECT_EQ(*material::under(stack), red());
}

TEST(Stack, ShadesAsNeitherOperandWhereTheMaskIsHalf) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << on.error;

  const SkColor4f base = centreOf(card(red()), on.runtime);
  const SkColor4f top = centreOf(card(blue()), on.runtime);
  const SkColor4f mixed = centreOf(
      card(material::over(red(), blue(), material::kit::maskConstant(0.5f))),
      on.runtime);

  // Half of each, which is neither of them: the stack has to have run
  // both operands' bodies and mixed what they returned.
  EXPECT_GT(base.fR, top.fR + 0.2f) << "the operands are told apart at all";
  EXPECT_LT(mixed.fR, base.fR - 0.1f);
  EXPECT_GT(mixed.fR, top.fR + 0.1f);
  EXPECT_GT(mixed.fB, base.fB + 0.1f);
  EXPECT_LT(mixed.fB, top.fB - 0.1f);

  // …and at the ends of the mask it IS each of them.
  const SkColor4f none = centreOf(
      card(material::over(red(), blue(), material::kit::maskConstant(0.0f))),
      on.runtime);
  const SkColor4f all = centreOf(
      card(material::over(red(), blue(), material::kit::maskConstant(1.0f))),
      on.runtime);
  EXPECT_NEAR(none.fR, base.fR, 0.02f);
  EXPECT_NEAR(all.fB, top.fB, 0.02f);
}
