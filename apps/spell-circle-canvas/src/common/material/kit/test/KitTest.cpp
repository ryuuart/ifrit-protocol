/** @file
 * The stock surfaces: every recipe compiles and shades through the Skia
 * backend, a fill stays inside its path, and the builders fill the slots
 * the recipes declare. The girih panel is the real star and cross and
 * sharpens with its contact angle, the chrome ramps put their hard stop
 * on the horizon, every text paint compiles and moves with the clock,
 * the dressed surface takes a decoded set, and a stack asks for its
 * operands' samplers and no more.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/core/Combine.h>
#include <sigilmaterial/core/Program.h>
#include <sigilmaterial/core/Terms.h>
#include <sigilmaterial/kit/Environments.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/kit/LayerStyles.h>
#include <sigilmaterial/kit/Patterns.h>
#include <sigilmaterial/kit/Pbr.h>
#include <sigilmaterial/kit/Recipes.h>
#include <sigilmaterial/kit/Reflections.h>
#include <sigilmaterial/kit/TextPaint.h>
#include <sigilmaterial/mask/Mask.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Surface.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilshaders/MaterialKit.h>

#include <cmath>
#include <memory>
#include <string>

#include "ShaderTable.h"
#include "support/Shade.h"

using namespace sigil::material;
using sigil::material::test::differing;
using sigil::material::test::luminance;
using sigil::material::test::shade;

TEST(Surfaces, RecipesCompileAndShade) {
  const EnvironmentMap env = kit::studioEnvironment(128);
  ASSERT_TRUE(env.valid());
  const SkPath shape = SkPath::Circle(40, 40, 30);
  const Texture normals = bevelNormals(shape, SkIRect::MakeWH(80, 80), 6);
  ASSERT_TRUE(normals.valid());
  EXPECT_TRUE(skia::shader(kit::gold(normals, env), {}));
  EXPECT_TRUE(skia::shader(kit::chrome(normals, env), {}));
  sk_sp<SkImage> backdrop;
  {
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(80, 80));
    s->getCanvas()->clear(SK_ColorCYAN);
    backdrop = s->makeImageSnapshot();
  }
  EXPECT_TRUE(
      skia::shader(kit::glass(normals, env, Texture::of(backdrop)), {}));
}

TEST(Surfaces, BuildersFillTheDeclaredSlots) {
  const EnvironmentMap env = kit::studioEnvironment(64);
  const Texture normals = bevelNormals(SkPath::Circle(30, 30, 20), 5);
  kit::ChromeParams params;
  params.roughness = 0.5f;
  const Material m = kit::chrome(normals, env, params);
  EXPECT_EQ(m.leaf("normals") != nullptr, true);
  EXPECT_EQ(m.leaf("env") != nullptr, true);
  EXPECT_EQ(m.get<glm::vec2>("envSize"), glm::vec2(64, 32));
  // Roughness picked the blurred level, not the base.
  const auto* envTexture = dynamic_cast<const Texture*>(m.leaf("env"));
  ASSERT_NE(envTexture, nullptr);
  EXPECT_EQ(envTexture->image().get(), env.image(0.5f).get());
  EXPECT_NE(envTexture->image().get(), env.image(0).get());
  // Same inputs, equal materials: what lets a scene prune a repainted
  // badge.
  EXPECT_EQ(m, kit::chrome(normals, env, params));
  EXPECT_FALSE(m == kit::chrome(normals, env));
}

TEST(Surfaces, FillShadesInsideTheShapeOnly) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 120));
  surface->getCanvas()->clear(SK_ColorTRANSPARENT);
  const EnvironmentMap env = kit::studioEnvironment(128);
  const SkPath shape = SkPath::Circle(60, 60, 40);
  skia::fill(*surface->getCanvas(), shape,
             kit::chrome(bevelNormals(shape, 8), env));
  SkBitmap bm;
  bm.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(bm.pixmap(), 0, 0));
  // The shader is clipped to the path: a material fills its shape and
  // leaves the rest of the canvas at whatever was already there. Checked
  // on alpha so it holds whatever colour the environment reflects.
  EXPECT_NE(bm.getColor(60, 60) & 0xff000000, 0u);  // inside: painted
  EXPECT_EQ(bm.getColor(5, 5) & 0xff000000, 0u);    // outside: untouched
}

TEST(Patterns, Girih8IsTheRealStarAndCross) {
  const pattern::Tile tile = kit::girih8(16);
  // s = a(1+sqrt 2): the tile is square and the khatam sits at its centre.
  const float s = 16.0f * (1.0f + 1.41421356f);
  EXPECT_NEAR(tile.size().width(), s, 1e-3f);
  EXPECT_NEAR(tile.size().height(), s, 1e-3f);
  sk_sp<SkImage> img = tile.image();
  ASSERT_TRUE(img);
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(img->width(), img->height()));
  ASSERT_TRUE(img->readPixels(nullptr, bm.pixmap(), 0, 0));
  const kit::GirihPalette pal = kit::fezPalette();
  const auto near = [](SkColor c, Color want) {
    return std::abs((int)SkColorGetR(c) - (int)std::lround(want.r * 255)) < 8 &&
           std::abs((int)SkColorGetB(c) - (int)std::lround(want.b * 255)) < 8;
  };
  // Centre: the star. A point on the diagonal between two arms, inside
  // the octagon but outside the khatam: the ground.
  EXPECT_TRUE(near(bm.getColor(img->width() / 2, img->height() / 2), pal.star));
  EXPECT_TRUE(
      near(bm.getColor((int)(s * 0.25f), (int)(s * 0.02f)), pal.ground));
  EXPECT_FALSE(kit::girih8(16) == kit::girih8(16));  // fresh bakes
}

TEST(Patterns, Girih8ContactAngleSharpensTheStar) {
  // How far the star reaches along the bisector between two arms: the
  // last pixel out from the centre, at 22.5°, in the star's own colour.
  const kit::GirihPalette pal = kit::fezPalette();
  const auto reach = [&](float contactDeg, float strapWidth = 0,
                         float edge = 40) {
    const pattern::Tile tile = kit::girih8(edge, pal, strapWidth, contactDeg);
    sk_sp<SkImage> img = tile.image();
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(img->width(), img->height()));
    img->readPixels(nullptr, bm.pixmap(), 0, 0);
    const float R = tile.size().width() / 2;
    const auto isStar = [&](SkColor c) {
      return std::abs((int)SkColorGetR(c) -
                      (int)std::lround(pal.star.r * 255)) < 8 &&
             std::abs((int)SkColorGetB(c) -
                      (int)std::lround(pal.star.b * 255)) < 8;
    };
    float last = 0;
    for (float r = 0; r < R; r += 0.5f) {
      const int x = (int)std::lround(R + r * std::cos(0.39269908f));
      const int y = (int)std::lround(R + r * std::sin(0.39269908f));
      if (isStar(bm.getColor(x, y))) last = r;
    }
    return std::pair{last / R, bm};
  };
  const auto [shallow, shallowTile] = reach(30);
  const auto [classic, classicTile] = reach(45);
  const auto [steep, steepTile] = reach(60);
  // The rays meet further out the shallower the angle.
  EXPECT_GT(shallow, classic);
  EXPECT_GT(classic, steep);
  // Measured on a large tile under a hairline strap — a stock strap is
  // drawn along the star's own edge and covers the vertex — the 45° inner
  // vertex stands at cos 45° / cos 22.5° of the apothem, which is the
  // closed form the rays answer.
  EXPECT_NEAR(reach(45, 1.0f, 200).first, 0.7654f, 0.02f);
  // The default IS the classic panel, pixel for pixel.
  const pattern::Tile plain = kit::girih8(40, pal);
  sk_sp<SkImage> img = plain.image();
  SkBitmap defaulted;
  defaulted.allocPixels(
      SkImageInfo::MakeN32Premul(img->width(), img->height()));
  img->readPixels(nullptr, defaulted.pixmap(), 0, 0);
  EXPECT_EQ(differing(defaulted, classicTile), 0);
  EXPECT_GT(differing(defaulted, steepTile), 100);
}

TEST(LayerStyles, ChromeRampsStopOnTheHorizon) {
  const std::vector<RampStop> steel =
      kit::chromeRamp(kit::ChromePalette::Steel);
  const std::vector<RampStop> silver =
      kit::chromeRamp(kit::ChromePalette::Silver);
  // Both ramps straddle the horizon with a hard stop at it.
  EXPECT_LT(steel[2].pos, kit::kChromeHorizonFrac);
  EXPECT_GT(steel[3].pos, kit::kChromeHorizonFrac);
  EXPECT_FLOAT_EQ(silver[3].pos, kit::kChromeHorizonFrac);
  EXPECT_EQ(kit::silverChromeText(), silver);
  EXPECT_EQ(kit::sunsetChromeText().size(), 8u);
  const Color tint = kit::aquaTint();
  EXPECT_EQ(kit::aquaBodyRamp(tint)[1].color, tint);
  EXPECT_FLOAT_EQ(kit::aquaGlowRamp(tint, 0.5f).back().color.a, 0.5f);
}

TEST(TextPaint, EveryPaintCompilesAndMovesWithTheClock) {
  const SkRect bounds = SkRect::MakeXYWH(10, 20, 100, 40);
  for (auto make : {kit::water, kit::meshGradient, kit::sparkle, kit::starNest,
                    kit::clouds, kit::tunnel}) {
    const Material a = make(bounds, 0.0f);
    EXPECT_TRUE(skia::shader(a, {}));
    EXPECT_FALSE(a == make(bounds, 1.0f));
    EXPECT_EQ(a, make(bounds, 0.0f));
  }
  const kit::TextPaintParams p = kit::textPaintParams(bounds, 2.0f);
  EXPECT_EQ(p.origin, glm::vec2(10, 20));
  EXPECT_EQ(p.extent, glm::vec2(100, 40));
  EXPECT_FLOAT_EQ(p.motion.x, std::sin(2.0f * 0.83f));
}

TEST(Surface, BothRecipesCompileAndShade) {
  kit::SurfaceParams params;
  params.baseColor = {0.2f, 0.6f, 0.9f, 1};
  params.emissive = {1, 0.5f, 0, 1};
  params.emissiveStrength = 0.5f;
  for (const Material& m : {kit::surface(params), kit::unlit(params)}) {
    EXPECT_TRUE(skia::shader(m, {}));
    // Every declared slot is dressed, so no body evaluates an unbound
    // child.
    EXPECT_EQ(m.children().size(), m.recipe().children().size());
  }
  EXPECT_TRUE(kit::isSurface(kit::surface(params)));
  EXPECT_FALSE(kit::isUnlit(kit::surface(params)));
  EXPECT_TRUE(kit::isUnlit(kit::unlit(params)));
  EXPECT_EQ(kit::surface(params), kit::surface(params));
  EXPECT_FALSE(kit::surface(params) == kit::unlit(params));
}

TEST(Surface, DressesADecodedSet) {
  const sk_sp<SkImage> image = [] {
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(2, 2));
    s->getCanvas()->clear(SK_ColorGRAY);
    return s->makeImageSnapshot();
  }();
  texture::TextureMaps maps;
  maps.normalDirectX = true;
  maps.maps[texture::Role::BaseColor] = Texture::of(image);
  maps.maps[texture::Role::Packed] = Texture::of(image);
  maps.maps[texture::Role::Emissive] = Texture::of(image);
  const Material m = kit::surface(maps);
  // The packed image stands in for all three channel maps, at glTF's
  // order, and the scalars a map multiplies come up off zero.
  EXPECT_FLOAT_EQ(m.get<float>("occlusionChannel"), 0.0f);
  EXPECT_FLOAT_EQ(m.get<float>("roughnessChannel"), 1.0f);
  EXPECT_FLOAT_EQ(m.get<float>("metallicChannel"), 2.0f);
  EXPECT_FLOAT_EQ(m.get<float>("metallic"), 1.0f);
  EXPECT_FLOAT_EQ(m.get<float>("normalDirectX"), 1.0f);
  EXPECT_FLOAT_EQ(m.get<float>("emissiveStrength"), 1.0f);
  const auto* base = dynamic_cast<const Texture*>(m.leaf(kit::kBaseColorSlot));
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->image().get(), image.get());
  // A set with no normal map still leaves the slot dressed flat.
  EXPECT_NE(m.leaf(kit::kNormalSlot), nullptr);
}

TEST(Over, StacksTopOverBaseWhereTheMaskSays) {
  kit::SurfaceParams red;
  red.baseColor = {1, 0, 0, 1};
  kit::SurfaceParams blue;
  blue.baseColor = {0, 0, 1, 1};
  const auto shade = [&](float coverage) {
    const Material m =
        over(kit::unlit(red), kit::unlit(blue), maskConstant(coverage));
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
  const Material stack = over(kit::unlit(red), kit::unlit(blue),
                              maskConstant(1.0f), Blend::Multiply);
  EXPECT_EQ(stackDepth(stack), 1);
  EXPECT_EQ(stackDepth(over(stack, kit::unlit(red), maskConstant(1.0f))), 2);
  EXPECT_EQ(*under(stack), kit::unlit(red));
  EXPECT_TRUE(skia::shader(stack, {}));
}

namespace {

/** A recipe whose params are one number nothing reads: what a case that
 *  is about slots or bodies rather than values stands a material on. */
struct NoParams {
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

/** The child slots the compiled SkSL program declares, which is one
 *  image sampler each once a GPU backend has inlined it. */
size_t declaredSlots(const Material& m) {
  const auto built = skia::builder(m, {});
  return built ? built->effect()->children().size() : 0u;
}

/** A one-texel texture under its own producer key, so @p key images are
 *  @p key distinct leaves. */
Texture texel(int key) {
  return Texture::produce("material.kit.test.texel." + std::to_string(key), [] {
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1, 1));
    s->getCanvas()->clear(SK_ColorWHITE);
    return s->makeImageSnapshot();
  });
}

}  // namespace

TEST(Over, AStackAsksForItsOperandsSamplersAndNoMore) {
  registerCompiler(Target::Slang, slangStandIn);

  // An undressed surface fills all seven of its slots so no body ever
  // evaluates an unbound child, and the SkSL body samples two of them.
  // The five it never reads are not declared to that program and so cost
  // it no sampler.
  const Material unlit = kit::unlit();
  EXPECT_EQ(unlit.children().size(), 7u);
  EXPECT_EQ(declaredSlots(unlit), 2u);
  EXPECT_EQ(skia::samplerCount(unlit), 2);

  const Material stack =
      over(kit::unlit(), kit::unlit(), maskConstant(0.5f), Blend::Mix);
  // The composed recipe declares a slot per operand's own slot, because
  // the language it was composed for reaches no child material.
  EXPECT_GT(stack.recipe().children().size(), 3u);
  // SkSL samples the operands themselves, so its program declares those
  // three slots and none of the composed ones.
  EXPECT_EQ(declaredSlots(stack), 3u);
  EXPECT_EQ(skia::samplerCount(stack), 2 * skia::samplerCount(unlit));
  EXPECT_LE(skia::samplerCount(stack), skia::kSamplerLimit);

  // The deepest stack anything here builds: a surface under two.
  Material deep = kit::surface();
  for (int i = 0; i < 2; ++i)
    deep = over(std::move(deep), kit::unlit(), maskConstant(0.5f));
  EXPECT_EQ(stackDepth(deep), 2);
  EXPECT_EQ(declaredSlots(deep), 3u);
  EXPECT_EQ(skia::samplerCount(deep),
            skia::samplerCount(kit::surface()) + 2 * skia::samplerCount(unlit));
  EXPECT_LE(skia::samplerCount(deep), skia::kSamplerLimit);
  EXPECT_TRUE(skia::shader(deep, {}));
}

TEST(Over, ATreeOverTheSamplerBudgetIsRefusedRatherThanDrawn) {
  // A device rejects a fragment program past its sampler indices after
  // Skia has accepted it, so the draw paints nothing and names nobody.
  // Refused here, the material that asked is the one reported.
  const int tooMany = skia::kSamplerLimit + 1;
  Recipe recipe = Recipe::of<NoParams>("kit.test.overBudget");
  std::string body = "half4 main(float2 p) { return ";
  for (int i = 0; i < tooMany; ++i) {
    const std::string slot = "uMap" + std::to_string(i);
    recipe.child(slot);
    body += (i ? " + " : "");
    body += slot + ".eval(p)";
  }
  recipe.body(Target::SkSL, body + "; }");
  Material m(std::make_shared<const Recipe>(std::move(recipe)), NoParams{});
  for (int i = 0; i < tooMany; ++i)
    m.child("uMap" + std::to_string(i), texel(i));
  EXPECT_EQ(skia::samplerCount(m), tooMany);
  EXPECT_FALSE(skia::shader(m, {}));
}

// ---------------------------------------------------------------------------
// The grained surfaces and the bank that bounds a field of them.

// ---- the embedded shader table --------------------------------------------

TEST(KitShaderTable, EveryStockBodyCompiles) {
  for (const Material& m : kit::everyRecipe()) {
    if (!m.recipe().has(Target::SkSL)) continue;
    EXPECT_TRUE(skia::shader(m, {.resolution = {64, 64}})) << m.recipe().name();
  }
}

TEST(KitShaderTable, HoldsEveryFileTheShaderDirectoryDoes) {
  sigil::test::expectShaderTableIsWholeDirectory(kit::shaderSources(),
                                                 SIGIL_MATERIAL_KIT_SHADER_DIR);
}
