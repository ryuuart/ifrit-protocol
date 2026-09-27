/** @file
 * A shader file as a material: read through a hub, declared from the
 * parameter struct, defined once per text so a re-described material
 * prunes, and a material of nothing where the file is not there.
 */

#include <gtest/gtest.h>
#include <sigilio/advanced/Places.h>
#include <sigilio/hub/Hub.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/program/Shader.h>

#include "ScratchDir.h"

using namespace sigil::material;
using sigil::test::ScratchDir;

namespace {
struct Tint {
  float amount = 0.5f;
};
}  // namespace

TEST(MaterialShader, AFileIsReadThroughAHub) {
  const ScratchDir scratch("material_shader");
  scratch.write("tint.sksl",
                "half4 main(float2 xy) { return half4(amount, 0, uTime, 1); }\n");
  sigil::io::Hub hub;
  sigil::io::mount(hub, "res://", scratch.path);

  const Material first = shader(hub, "res://tint.sksl", Tint{0.25f});
  ASSERT_TRUE(first.hasProgram());
  EXPECT_EQ(0.25f, first.get<float>("amount"));
  EXPECT_TRUE(first.recipe().reads(FrameInput::Time));
  EXPECT_FALSE(first.recipe().reads(FrameInput::Resolution));
  EXPECT_EQ(first, shader(hub, "res://tint.sksl", Tint{0.25f}));

  EXPECT_FALSE(shader(hub, "res://absent.sksl", Tint{}).hasProgram());
}
