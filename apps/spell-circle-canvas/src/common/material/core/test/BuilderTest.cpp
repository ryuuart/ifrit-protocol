/** @file
 * The material built up by composition: a colour converts, layers stack
 * in order, a surface and a base are read back, and two materials built
 * the same way compare equal while any difference in the stack does not.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/core/Material.h>

using namespace sigil::material;

TEST(MaterialBuilder, AColourIsAMaterial) {
  const Material ink = Color{1, 0, 0, 1};
  ASSERT_NE(nullptr, ink.color());
  EXPECT_EQ((Color{1, 0, 0, 1}), *ink.color());
  EXPECT_FALSE(ink.hasProgram());
  EXPECT_TRUE(ink.layers().empty());
  EXPECT_EQ(nullptr, ink.surface());
}

TEST(MaterialBuilder, LayersStackBottomFirstAndCompareByValue) {
  const auto build = [](float opacity) {
    return from(Color{0.2f, 0.2f, 0.2f, 1})
        .layer(Color{1, 1, 1, 1}, {.blend = BlendMode::Screen, .opacity = opacity})
        .layer(Color{0, 0, 1, 1},
               {.mask = Mask{.source = Color{0, 0, 0, 0.5f}}})
        .surface({.metallic = 1.0f, .roughness = 0.2f});
  };
  const Material steel = build(0.3f);
  ASSERT_EQ(2u, steel.layers().size());
  EXPECT_EQ(BlendMode::Screen, steel.layers()[0].options.blend);
  EXPECT_TRUE(steel.layers()[1].options.mask.has_value());
  ASSERT_NE(nullptr, steel.surface());
  EXPECT_EQ(1.0f, std::get<float>(steel.surface()->metallic));

  EXPECT_EQ(steel, build(0.3f));
  EXPECT_NE(steel, build(0.4f));
  EXPECT_NE(steel, steel.base());
  EXPECT_EQ(Material(Color{0.2f, 0.2f, 0.2f, 1}), steel.base());
}

TEST(MaterialBuilder, TheDesignatedFormIsTheChain) {
  const Material chained = from(Color{0.5f, 0.5f, 0.5f, 1})
                               .layer(Color{1, 1, 1, 1}, {.opacity = 0.5f})
                               .surface({.roughness = 0.8f});
  const Material designated{{.base = Color{0.5f, 0.5f, 0.5f, 1},
                             .layers = {{Color{1, 1, 1, 1}, {.opacity = 0.5f}}},
                             .surface = SurfaceOptions{.roughness = 0.8f}}};
  EXPECT_EQ(chained, designated);
}

TEST(MaterialBuilder, AProgramOnlyOperationOnAColourIsIgnored) {
  Material flat = Color{1, 1, 1, 1};
  flat.set("amount", 1.0f);
  EXPECT_EQ(Material(Color{1, 1, 1, 1}), flat);
  EXPECT_EQ(nullptr, flat.resolve(Target::SkSL, {}).program);
}
