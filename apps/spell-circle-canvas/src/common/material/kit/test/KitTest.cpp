/** @file
 * The stock surfaces: every recipe compiles and shades through the Skia
 * backend, a fill stays inside its path, and the builders fill the slots
 * the recipes declare.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/kit/Surfaces.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Surface.h>

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
