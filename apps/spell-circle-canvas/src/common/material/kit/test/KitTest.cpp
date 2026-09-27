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
  kit::ChromeParameters parameters;
  parameters.roughness = 0.5f;
  const Material m = kit::chrome(normals, env, parameters);
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
  EXPECT_EQ(m, kit::chrome(normals, env, parameters));
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
  const std::vector<ColorStop> steel =
      kit::chromeRamp(kit::ChromePalette::Steel);
  const std::vector<ColorStop> silver =
      kit::chromeRamp(kit::ChromePalette::Silver);
  // Both ramps straddle the horizon with a hard stop at it.
  EXPECT_LT(steel[2].offset, kit::kChromeHorizonFraction);
  EXPECT_GT(steel[3].offset, kit::kChromeHorizonFraction);
  EXPECT_FLOAT_EQ(silver[3].offset, kit::kChromeHorizonFraction);
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
  const kit::TextPaintParameters p = kit::textPaintParameters(bounds, 2.0f);
  EXPECT_EQ(p.origin, glm::vec2(10, 20));
  EXPECT_EQ(p.extent, glm::vec2(100, 40));
  EXPECT_FLOAT_EQ(p.motion.x, std::sin(2.0f * 0.83f));
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
