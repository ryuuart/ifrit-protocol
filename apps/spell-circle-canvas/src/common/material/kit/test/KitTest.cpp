/** @file
 * The stock surfaces: every recipe compiles and shades through the Skia
 * backend, a fill stays inside its path, and the builders fill the slots
 * the recipes declare. The girih panel is the real star and cross, the
 * chrome ramps put their hard stop on the horizon, and every text paint
 * compiles and moves with the clock.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/kit/LayerStyles.h>
#include <sigilmaterial/kit/Patterns.h>
#include <sigilmaterial/kit/Surfaces.h>
#include <sigilmaterial/kit/TextPaint.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Surface.h>

#include <cmath>

using namespace sigil::material;

TEST(Surfaces, RecipesCompileAndShade) {
  skia::install();
  const Environment env = Environment::studio(128);
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
  const Environment env = Environment::studio(64);
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
  skia::install();
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 120));
  surface->getCanvas()->clear(SK_ColorTRANSPARENT);
  const Environment env = Environment::studio(128);
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

TEST(LayerStyles, ChromeRampsStopOnTheHorizon) {
  const std::vector<kit::RampStop> steel =
      kit::chromeRamp(kit::ChromePalette::Steel);
  const std::vector<kit::RampStop> silver =
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
  skia::install();
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
