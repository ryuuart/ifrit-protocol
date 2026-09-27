/** @file
 * The surface program: both recipes compile and shade through the Skia
 * backend, an authored colour and a map texel are one number, the
 * dressed program takes a decoded set, a stack asks for its operands'
 * samplers and no more, and a material's stated response lowers into the
 * program with its numbers and maps in place.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/core/Combine.h>
#include <sigilmaterial/core/Program.h>
#include <sigilmaterial/mask/Mask.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilshaders/MaterialSurface.h>

#include <memory>
#include <string>

#include "ShaderTable.h"
#include "support/Shade.h"

using namespace sigil::material;
namespace media = sigil::media;

TEST(Surface, BothRecipesCompileAndShade) {
  surface::SurfaceParameters parameters;
  parameters.baseColor = {0.2f, 0.6f, 0.9f, 1};
  parameters.emissive = {1, 0.5f, 0, 1};
  parameters.emissiveStrength = 0.5f;
  for (const Material& m : {surface::program(parameters), surface::unlit(parameters)}) {
    EXPECT_TRUE(skia::shader(m, {}));
    // Every declared slot is dressed, so no body evaluates an unbound
    // child.
    EXPECT_EQ(m.slots().size(), m.recipe().slots().size());
  }
  EXPECT_TRUE(surface::isSurface(surface::program(parameters)));
  EXPECT_FALSE(surface::isUnlit(surface::program(parameters)));
  EXPECT_TRUE(surface::isUnlit(surface::unlit(parameters)));
  EXPECT_EQ(surface::program(parameters), surface::program(parameters));
  EXPECT_FALSE(surface::program(parameters) == surface::unlit(parameters));
}

TEST(Surface, AnAuthoredColourAndAMapTexelAreOneNumber) {
  // A mid-grey as an author types it. A builder takes it and the
  // parameter holds it, because nothing between the two has a space to
  // convert between.
  const int code = 128;
  const float typed = (float)code / 255.0f;
  const surface::SurfaceParameters authored{
      .baseColor = {typed, typed, typed, 1}, .metallic = 0, .roughness = 0.5f};
  EXPECT_FLOAT_EQ(authored.baseColor.r, typed);
  EXPECT_FLOAT_EQ(authored.baseColor.g, typed);
  EXPECT_FLOAT_EQ(authored.baseColor.b, typed);
  EXPECT_FLOAT_EQ(authored.baseColor.a, 1.0f);

  // The body multiplies the base colour by the sample of the map in its
  // slot, so the colour in the parameter and the SAME colour carried by
  // a flat map have to shade to one place. This is the claim for a real
  // image: what a decoded texel carries is the number the image stores,
  // which is the number the author typed.
  surface::SurfaceParameters white;
  white.baseColor = {1, 1, 1, 1};
  Material sampled = surface::unlit(white);
  sampled.slot(
      surface::kBaseColorSlot,
      Texture(media::PixelSource(
                  test::solid(SkColorSetARGB(255, code, code, code), 4, 4)))
          .tile(Repeat::Pad));
  const SkColor fromParameter =
      test::shade(surface::unlit(authored), 4, 4).getColor(1, 1);
  const SkColor fromMap = test::shade(sampled, 4, 4).getColor(1, 1);
  EXPECT_NEAR((int)SkColorGetR(fromParameter), (int)SkColorGetR(fromMap), 1);
  EXPECT_NEAR((int)SkColorGetG(fromParameter), (int)SkColorGetG(fromMap), 1);
  EXPECT_NEAR((int)SkColorGetB(fromParameter), (int)SkColorGetB(fromMap), 1);

  // And what the surface shows is the colour that was typed: a recipe
  // that is its own light, over a white map, paints the number and not
  // a transformation of it.
  EXPECT_NEAR((int)SkColorGetR(fromParameter), code, 1);
  EXPECT_NEAR((int)SkColorGetG(fromParameter), code, 1);
  EXPECT_NEAR((int)SkColorGetB(fromParameter), code, 1);
}

TEST(Surface, EveryColourFieldReachesTheUniformAsItWasWritten) {
  surface::SurfaceParameters p{.roughness = 0.02f, .transmission = 1,
                               .thickness = 0.35f};
  p.baseColor = {0.9f, 0.4f, 0.1f, 1};
  p.emissive = {0.2f, 0.3f, 0.4f, 1};
  // The Beer-Lambert coefficient is taken per unit of thickness, so a
  // dense channel is a number above one: what would be out of range for
  // a colour is in range for this, and nothing may hold it under one.
  p.absorption = {4.0f, 0.5f, 0.25f, 1};
  const Material m = surface::program(p);
  const Color baseColor = m.get<Color>("baseColor");
  const Color emissive = m.get<Color>("emissive");
  const Color absorption = m.get<Color>("absorption");
  EXPECT_FLOAT_EQ(baseColor.r, 0.9f);
  EXPECT_FLOAT_EQ(baseColor.b, 0.1f);
  EXPECT_FLOAT_EQ(emissive.g, 0.3f);
  EXPECT_FLOAT_EQ(absorption.r, 4.0f);
  EXPECT_FLOAT_EQ(absorption.g, 0.5f);
}

TEST(Surface, DressesADecodedSet) {
  const sk_sp<SkImage> image = [] {
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(2, 2));
    s->getCanvas()->clear(SK_ColorGRAY);
    return s->makeImageSnapshot();
  }();
  texture::TextureMaps maps;
  maps.normalDirectX = true;
  maps.maps[texture::Role::BaseColor] = Texture(media::PixelSource(image));
  maps.maps[texture::Role::Packed] = Texture(media::PixelSource(image));
  maps.maps[texture::Role::Emissive] = Texture(media::PixelSource(image));
  const Material m = surface::program(maps);
  // The packed image stands in for all three channel maps, at glTF's
  // order, and the scalars a map multiplies come up off zero.
  EXPECT_FLOAT_EQ(m.get<float>("occlusionChannel"), 0.0f);
  EXPECT_FLOAT_EQ(m.get<float>("roughnessChannel"), 1.0f);
  EXPECT_FLOAT_EQ(m.get<float>("metallicChannel"), 2.0f);
  EXPECT_FLOAT_EQ(m.get<float>("metallic"), 1.0f);
  EXPECT_FLOAT_EQ(m.get<float>("normalDirectX"), 1.0f);
  EXPECT_FLOAT_EQ(m.get<float>("emissiveStrength"), 1.0f);
  const auto* base = dynamic_cast<const Texture*>(m.leaf(surface::kBaseColorSlot));
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->source(), media::PixelSource(image));
  // A set with no normal map still leaves the slot dressed flat.
  EXPECT_NE(m.leaf(surface::kNormalSlot), nullptr);
}

TEST(Over, StacksTopOverBaseWhereTheMaskSays) {
  surface::SurfaceParameters red;
  red.baseColor = {1, 0, 0, 1};
  surface::SurfaceParameters blue;
  blue.baseColor = {0, 0, 1, 1};
  const auto shade = [&](float coverage) {
    const Material m =
        over(surface::unlit(red), surface::unlit(blue), maskConstant(coverage));
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1, 1));
    skia::fill(*s->getCanvas(), SkPath::Rect(SkRect::MakeWH(1, 1)), m);
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
    s->makeImageSnapshot()->readPixels(nullptr, bm.pixmap(), 0, 0);
    return bm.getColor(0, 0);
  };
  EXPECT_EQ(SkColorGetR(shade(0.0f)), 255u);
  EXPECT_EQ(SkColorGetB(shade(1.0f)), 255u);
  // The stack is one material: the operands are its children.
  const Material stack = over(surface::unlit(red), surface::unlit(blue),
                              maskConstant(1.0f), BlendMode::Multiply);
  EXPECT_EQ(stackDepth(stack), 1);
  EXPECT_EQ(stackDepth(over(stack, surface::unlit(red), maskConstant(1.0f))), 2);
  EXPECT_EQ(*under(stack), surface::unlit(red));
  EXPECT_TRUE(skia::shader(stack, {}));
}

namespace {

/** A recipe whose parameters are one number nothing reads: what a case that
 *  is about slots or bodies rather than values stands a material on. */
struct NoParameters {
  float unused = 0;
};

/** A stand-in Slang compiler, so `over()` builds the COMPOSED recipe.
 *  Composition is asked for only where a compiler that needs it is
 *  installed — a language handed one body per material cannot reach a
 *  child material — and a stack built without one carries the plain
 *  three-slot recipe, which never asks what the case below asks. */
std::shared_ptr<Program> slangStandIn(std::shared_ptr<const Recipe> recipe,
                                      Variant variant, std::string&) {
  return std::make_shared<Program>(std::move(recipe), Target::Slang, variant);
}

/** The slots the compiled SkSL program declares, which is one
 *  image sampler each once a GPU backend has inlined it. */
size_t declaredSlots(const Material& m) {
  const auto built = skia::builder(m, {});
  return built ? built->effect()->children().size() : 0u;
}

/** A one-texel texture under its own producer key, so @p key images are
 *  @p key distinct leaves. */
Texture texel(int key) {
  return Texture(media::PixelSource::produce(
      "material.surface.test.texel." + std::to_string(key), [] {
        sk_sp<SkSurface> s =
            SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1, 1));
        s->getCanvas()->clear(SK_ColorWHITE);
        return s->makeImageSnapshot();
      }));
}

}  // namespace

TEST(Over, AStackAsksForItsOperandsSamplersAndNoMore) {
  registerCompiler(Target::Slang, slangStandIn);

  // An undressed surface fills all seven of its slots so no body ever
  // evaluates an unbound child, and the SkSL body samples two of them.
  // The five it never reads are not declared to that program and so cost
  // it no sampler.
  const Material unlit = surface::unlit();
  EXPECT_EQ(unlit.slots().size(), 7u);
  EXPECT_EQ(declaredSlots(unlit), 2u);
  EXPECT_EQ(skia::samplerCount(unlit), 2);

  const Material stack =
      over(surface::unlit(), surface::unlit(), maskConstant(0.5f), BlendMode::Normal);
  // The composed recipe declares a slot per operand's own slot, because
  // the language it was composed for reaches no child material.
  EXPECT_GT(stack.recipe().slots().size(), 3u);
  // SkSL samples the operands themselves, so its program declares those
  // three slots and none of the composed ones.
  EXPECT_EQ(declaredSlots(stack), 3u);
  EXPECT_EQ(skia::samplerCount(stack), 2 * skia::samplerCount(unlit));
  EXPECT_LE(skia::samplerCount(stack), skia::kSamplerLimit);

  // The deepest stack anything here builds: a surface under two.
  Material deep = surface::program();
  for (int i = 0; i < 2; ++i)
    deep = over(std::move(deep), surface::unlit(), maskConstant(0.5f));
  EXPECT_EQ(stackDepth(deep), 2);
  EXPECT_EQ(declaredSlots(deep), 3u);
  EXPECT_EQ(skia::samplerCount(deep),
            skia::samplerCount(surface::program()) + 2 * skia::samplerCount(unlit));
  EXPECT_LE(skia::samplerCount(deep), skia::kSamplerLimit);
  EXPECT_TRUE(skia::shader(deep, {}));
}

TEST(Over, ATreeOverTheSamplerBudgetIsRefusedRatherThanDrawn) {
  // A device rejects a fragment program past its sampler indices after
  // Skia has accepted it, so the draw paints nothing and names nobody.
  // Refused here, the material that asked is the one reported.
  const int tooMany = skia::kSamplerLimit + 1;
  Recipe recipe = Recipe::of<NoParameters>("surface.test.overBudget");
  std::string body = "half4 main(float2 p) { return ";
  for (int i = 0; i < tooMany; ++i) {
    const std::string slot = "uMap" + std::to_string(i);
    recipe.slot(slot);
    body += (i ? " + " : "");
    body += slot + ".eval(p)";
  }
  recipe.body(Target::SkSL, body + "; }");
  Material m(std::make_shared<const Recipe>(std::move(recipe)), NoParameters{});
  for (int i = 0; i < tooMany; ++i)
    m.slot("uMap" + std::to_string(i), texel(i));
  EXPECT_EQ(skia::samplerCount(m), tooMany);
  EXPECT_FALSE(skia::shader(m, {}));
}

// ---- the lowering ----------------------------------------------------------

TEST(Surface, LowersAColourAndItsResponseIntoTheProgram) {
  const Material lowered = surface::lower(
      from(Color{0.2f, 0.4f, 0.6f, 1}).surface({.metallic = 0.3f, .roughness = 0.85f}));
  ASSERT_TRUE(surface::isSurface(lowered));
  EXPECT_FALSE(surface::isUnlit(lowered));
  EXPECT_EQ(lowered.get<Color>("baseColor"), (Color{0.2f, 0.4f, 0.6f, 1}));
  EXPECT_FLOAT_EQ(lowered.get<float>("metallic"), 0.3f);
  EXPECT_FLOAT_EQ(lowered.get<float>("roughness"), 0.85f);
  // A surface program is already what it states, so it lowers to itself.
  EXPECT_EQ(surface::lower(lowered), lowered);
}

TEST(Surface, LowersUnlitAndNoResponseToTheUnlitProgram) {
  const Color red{1, 0, 0, 1};
  const Material asked = surface::lower(from(red).surface({.unlit = true}));
  EXPECT_TRUE(surface::isUnlit(asked));
  EXPECT_EQ(asked.get<Color>("baseColor"), red);
  const Material flat = surface::lower(Material(red));
  EXPECT_TRUE(surface::isUnlit(flat));
  EXPECT_EQ(flat.get<Color>("baseColor"), red);
}

TEST(Surface, LowersAnImageChannelIntoItsMapSlot) {
  const sk_sp<SkImage> grey = test::solid(SK_ColorGRAY, 2, 2);
  const Material lowered = surface::lower(
      from(Color{1, 1, 1, 1})
          .surface({.roughness = image(media::PixelSource(grey))}));
  // The map multiplies the factor, so the factor lets it through whole.
  EXPECT_FLOAT_EQ(lowered.get<float>("roughness"), 1.0f);
  const Texture* map = surface::map(lowered, surface::kRoughnessSlot);
  ASSERT_NE(map, nullptr);
  EXPECT_EQ(map->source(), media::PixelSource(grey));
  // An image base is the base colour's map in the same way.
  const Material based =
      surface::lower(from(image(media::PixelSource(grey))).surface({}));
  const Texture* base = surface::map(based, surface::kBaseColorSlot);
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->source(), media::PixelSource(grey));
}

// ---- the embedded shader table --------------------------------------------

TEST(SurfaceShaderTable, HoldsEveryFileTheShaderDirectoryDoes) {
  sigil::test::expectShaderTableIsWholeDirectory(
      surface::shaderSources(), SIGIL_MATERIAL_SURFACE_SHADER_DIR);
}
