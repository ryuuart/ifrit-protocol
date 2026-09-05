/** @file
 * The stock surfaces: every recipe compiles and shades through the Skia
 * backend, a fill stays inside its path, and the builders fill the slots
 * the recipes declare. The girih panel is the real star and cross and
 * sharpens with its contact angle, the grained surfaces shade their bed,
 * arrises, ladder and tooth and re-roll on their seed, the bank folds a
 * field's seeds into a bounded number of instances, the chrome ramps put
 * their hard stop on the horizon, and every text paint compiles and moves
 * with the clock.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/core/Combine.h>
#include <sigilmaterial/kit/Bank.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/kit/LayerStyles.h>
#include <sigilmaterial/kit/Mask.h>
#include <sigilmaterial/kit/Patterns.h>
#include <sigilmaterial/kit/Recipes.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilmaterial/kit/Surfaces.h>
#include <sigilmaterial/kit/Terms.h>
#include <sigilmaterial/kit/TextPaint.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Surface.h>
#include <sigilshaders/MaterialKit.h>

#include <cmath>
#include <string>

#include "ShaderTable.h"

using namespace sigil::material;

namespace {

/** ONE TERM, EVALUATED. The term source is prepended to a body that
 *  returns the expression asked about, the material is compiled through
 *  the Skia backend and drawn over one texel of a float surface, so what
 *  comes back is what the term computed rather than what an 8-bit
 *  channel could hold. */
struct NoParams {
  float unused = 0;
};

SkColor4f term(const std::string& expression) {
  skia::install();
  static int serial = 0;
  const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<NoParams>("term." + std::to_string(serial++))
          .body(Target::SkSL, kit::termsSource(Target::SkSL) +
                                  "half4 main(float2 xy) { return half4(" +
                                  expression + "); }"));
  sk_sp<SkShader> shader = skia::shader(Material(recipe, NoParams{}), {});
  EXPECT_TRUE(shader) << expression;
  if (!shader) return {0, 0, 0, 0};
  sk_sp<SkSurface> surface = SkSurfaces::Raster(
      SkImageInfo::Make(1, 1, kRGBA_F32_SkColorType, kPremul_SkAlphaType));
  SkPaint paint;
  paint.setShader(shader);
  paint.setBlendMode(SkBlendMode::kSrc);
  surface->getCanvas()->drawPaint(paint);
  float px[4] = {0, 0, 0, 0};
  const SkImageInfo one =
      SkImageInfo::Make(1, 1, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  EXPECT_TRUE(surface->readPixels(SkPixmap(one, px, sizeof(px)), 0, 0));
  return {px[0], px[1], px[2], px[3]};
}

/** The red channel of a term that answers one number. */
float scalar(const std::string& expression) {
  return term(expression + ", 0.0, 0.0, 1.0").fR;
}

/** @p m shaded over a @p w × @p h rect, read back. */
SkBitmap shade(const Material& m, int w, int h) {
  skia::install();
  sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
  s->getCanvas()->clear(SK_ColorBLACK);
  skia::fill(*s->getCanvas(), SkPath::Rect(SkRect::MakeWH((float)w, (float)h)),
             m);
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  s->makeImageSnapshot()->readPixels(nullptr, bm.pixmap(), 0, 0);
  return bm;
}

int luminance(SkColor c) {
  return ((int)SkColorGetR(c) * 299 + (int)SkColorGetG(c) * 587 +
          (int)SkColorGetB(c) * 114) /
         1000;
}

int differing(const SkBitmap& a, const SkBitmap& b) {
  int n = 0;
  for (int y = 0; y < a.height(); ++y)
    for (int x = 0; x < a.width(); ++x)
      n += a.getColor(x, y) != b.getColor(x, y);
  return n;
}

}  // namespace

TEST(Terms, EachTermMeetsItsClosedForm) {
  // The transcendentals are polynomials, so they are held to what a
  // polynomial of their degree can do rather than to the library's.
  EXPECT_NEAR(scalar("atan2P(1.0, 1.0)"), std::atan2(1.0f, 1.0f), 2e-3f);
  EXPECT_NEAR(scalar("atan2P(1.0, -1.0)"), std::atan2(1.0f, -1.0f), 2e-3f);
  EXPECT_NEAR(scalar("atan2P(-0.3, 0.7)"), std::atan2(-0.3f, 0.7f), 2e-3f);
  EXPECT_NEAR(scalar("acosP(0.5)"), std::acos(0.5f), 2e-3f);
  EXPECT_NEAR(scalar("acosP(-0.9)"), std::acos(-0.9f), 2e-3f);

  // LAMBERT at 45 degrees is the cosine of 45 degrees.
  EXPECT_NEAR(scalar("lambert(float3(0.0, 0.0, 1.0), "
                     "normalize(float3(1.0, 0.0, 1.0)))"),
              std::sqrt(0.5f), 2e-3f);
  // Head on, all of it; edge on, none.
  EXPECT_NEAR(scalar("lambert(float3(0.0, 0.0, 1.0), float3(0.0, 0.0, 1.0))"),
              1.0f, 2e-3f);
  EXPECT_NEAR(scalar("lambert(float3(0.0, 0.0, 1.0), float3(1.0, 0.0, 0.0))"),
              0.0f, 2e-3f);

  // BLINN with the light and the eye together: the half vector is the
  // normal and the highlight is at its peak whatever the exponent.
  EXPECT_NEAR(scalar("blinn(float3(0.0, 0.0, 1.0), float3(0.0, 0.0, 1.0), "
                     "float3(0.0, 0.0, 1.0), 48.0)"),
              1.0f, 2e-3f);
  // Light at 45 degrees, eye head on: the half vector stands at 22.5.
  EXPECT_NEAR(scalar("blinn(float3(0.0, 0.0, 1.0), "
                     "normalize(float3(1.0, 0.0, 1.0)), "
                     "float3(0.0, 0.0, 1.0), 4.0)"),
              std::pow(std::cos(3.14159265f / 8.0f), 4.0f), 3e-3f);

  // FRESNEL: a dielectric's four per cent head on, and white at the rim.
  EXPECT_NEAR(scalar("fresnel(float3(0.04, 0.04, 0.04), 1.0).r"), 0.04f, 2e-3f);
  EXPECT_NEAR(scalar("fresnel(float3(0.04, 0.04, 0.04), 0.0).r"), 1.0f, 2e-3f);
  // A metal's reflectance IS its base colour; a dielectric's is four
  // per cent whatever colour it is.
  EXPECT_NEAR(scalar("specularColor(float3(0.9, 0.6, 0.3), 1.0).g"), 0.6f,
              2e-3f);
  EXPECT_NEAR(scalar("specularColor(float3(0.9, 0.6, 0.3), 0.0).g"), 0.04f,
              2e-3f);

  // THE SPLIT SUM at its one exact point: a mirror seen head on returns
  // the radiance it was handed, scale plus bias summing to one.
  EXPECT_NEAR(scalar("environmentSpecular(float3(1.0, 1.0, 1.0), "
                     "float3(1.0, 1.0, 1.0), 0.0, 1.0).r"),
              1.0f, 3e-3f);
  // And a rough surface takes less of it than a smooth one.
  EXPECT_LT(scalar("environmentSpecular(float3(1.0, 1.0, 1.0), "
                   "float3(0.04, 0.04, 0.04), 1.0, 1.0).r"),
            scalar("environmentSpecular(float3(1.0, 1.0, 1.0), "
                   "float3(0.04, 0.04, 0.04), 0.0, 1.0).r"));

  // ADDITIVE reflection is the radiance at its weight and nothing else.
  EXPECT_NEAR(scalar("environmentReflection(float3(0.5, 0.5, 0.5), 0.6).r"),
              0.3f, 2e-3f);

  // BEER-LAMBERT over half a unit of a medium that takes one per unit.
  EXPECT_NEAR(scalar("attenuate(float3(1.0, 1.0, 1.0), "
                     "float3(1.0, 1.0, 1.0), 0.5).r"),
              std::exp(-0.5f), 2e-3f);
  // Nothing absorbed is everything through.
  EXPECT_NEAR(scalar("attenuate(float3(0.8, 0.8, 0.8), "
                     "float3(0.0, 0.0, 0.0), 100.0).r"),
              0.8f, 2e-3f);

  // OCCLUSION believed in full, and not at all.
  EXPECT_NEAR(scalar("occlusion(0.5, 1.0)"), 0.5f, 2e-3f);
  EXPECT_NEAR(scalar("occlusion(0.5, 0.0)"), 1.0f, 2e-3f);
  // EMISSION is the colour times the map times the strength.
  EXPECT_NEAR(
      scalar("emission(float3(0.5, 0.5, 0.5), 2.0, float3(0.4, 0.4, 0.4)).r"),
      0.4f, 2e-3f);

  // REFRACTION through no change of index is the ray it was given.
  EXPECT_NEAR(scalar("-refraction(float3(0.0, 0.0, -1.0), "
                     "float3(0.0, 0.0, 1.0), 1.0).z"),
              1.0f, 2e-3f);
  // Past total internal reflection there is no refracted ray.
  EXPECT_NEAR(scalar("length(refraction(normalize(float3(1.0, 0.0, -0.05)), "
                     "float3(0.0, 0.0, 1.0), 1.6))"),
              0.0f, 2e-3f);

  // THE PANORAMA'S CONVENTION: u = 0.5 looks along -z, v = 0 is the
  // zenith, and a direction and a coordinate round trip.
  const SkColor4f forward =
      term("equirectUv(float3(0.0, 0.0, -1.0)), 0.0, 1.0");
  EXPECT_NEAR(forward.fR, 0.5f, 2e-3f);
  EXPECT_NEAR(forward.fG, 0.5f, 2e-3f);
  EXPECT_NEAR(scalar("equirectUv(float3(0.0, 1.0, 0.0)).y"), 0.0f, 2e-3f);
  EXPECT_NEAR(scalar("equirectDirection(equirectUv("
                     "normalize(float3(0.3, 0.5, -0.8)))).y"),
              0.5f / std::sqrt(0.09f + 0.25f + 0.64f), 4e-3f);

  // A roughness reads across the chain it was prefiltered into.
  EXPECT_NEAR(scalar("roughnessLevel(0.5, 9.0)"), 4.0f, 2e-3f);
  EXPECT_NEAR(scalar("roughnessLevel(1.0, 9.0)"), 8.0f, 2e-3f);

  // LUMINANCE: the three weights sum to one, so a grey reads at its own
  // value and a colour reads between its channels.
  EXPECT_NEAR(scalar("luminance(float3(1.0, 1.0, 1.0))"), 1.0f, 2e-3f);
  EXPECT_NEAR(scalar("luminance(float3(0.0, 1.0, 0.0))"), 0.715160f, 2e-3f);

  // THE TONE CURVE. Black stays black, and a grey lands at its own value
  // over one plus itself — white at a half.
  EXPECT_NEAR(scalar("toneMap(float3(0.0, 0.0, 0.0), 1.0).r"), 0.0f, 2e-3f);
  EXPECT_NEAR(scalar("toneMap(float3(1.0, 1.0, 1.0), 1.0).r"), 0.5f, 2e-3f);
  EXPECT_NEAR(scalar("toneMap(float3(0.25, 0.25, 0.25), 1.0).r"), 0.2f, 2e-3f);
  // The exposure multiplies the radiance BEFORE the curve, so twice the
  // exposure over half the radiance is the same colour.
  EXPECT_NEAR(scalar("toneMap(float3(0.5, 0.5, 0.5), 2.0).r"),
              scalar("toneMap(float3(1.0, 1.0, 1.0), 1.0).r"), 2e-3f);
  // NOTHING CLIPS: a radiance a hundred times over white lands just
  // under one rather than flat on it, which is the whole point of the
  // curve. And a negative radiance answers black rather than a negative
  // colour.
  EXPECT_NEAR(scalar("toneMap(float3(100.0, 100.0, 100.0), 1.0).r"),
              100.0f / 101.0f, 2e-3f);
  EXPECT_LT(scalar("toneMap(float3(100.0, 100.0, 100.0), 1.0).r"), 1.0f);
  EXPECT_NEAR(scalar("toneMap(float3(-1.0, -1.0, -1.0), 1.0).r"), 0.0f, 2e-3f);
}

TEST(Surfaces, RecipesCompileAndShade) {
  skia::install();
  const EnvironmentMap env = EnvironmentMap::studio(128);
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
  const EnvironmentMap env = EnvironmentMap::studio(64);
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
  const EnvironmentMap env = EnvironmentMap::studio(128);
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

TEST(Surface, BothRecipesCompileAndShade) {
  skia::install();
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

TEST(Mask, ShapesWhatItReads) {
  skia::install();
  const Material half = kit::maskConstant(0.5f);
  EXPECT_TRUE(skia::shader(half, {}));
  EXPECT_FLOAT_EQ(kit::invert(half).get<float>("inverted"), 1.0f);
  EXPECT_FLOAT_EQ(kit::invert(kit::invert(half)).get<float>("inverted"), 0.0f);
  const Material fitted = kit::fit(half, 0.25f, 0.75f);
  EXPECT_FLOAT_EQ(fitted.get<float>("low"), 0.25f);
  EXPECT_FLOAT_EQ(fitted.get<float>("high"), 0.75f);

  sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(2, 2));
  s->getCanvas()->clear(SK_ColorWHITE);
  const Texture map = Texture::of(s->makeImageSnapshot());
  for (const Material& m :
       {kit::maskMap(map), kit::maskVertexColor(map, 1),
        kit::maskSlope(map, {0, 1, 0}), kit::maskHeight(map, 0, 1)})
    EXPECT_TRUE(skia::shader(m, {}));
}

TEST(Mask, ReshapingSomethingThatIsNotAMaskChangesNothing) {
  // A material with no range to move and no answer to flip cannot be
  // reshaped, and a stack whose coverage silently stayed as it was looks
  // exactly like a stack whose fit was wrong — so both hand the material
  // straight back, with a report on stderr naming the rule.
  kit::SurfaceParams red;
  red.baseColor = {1, 0, 0, 1};
  const Material paint = kit::unlit(red);
  EXPECT_EQ(kit::fit(paint, 0.25f, 0.75f), paint);
  EXPECT_EQ(kit::invert(paint), paint);
}

TEST(Over, StacksTopOverBaseWhereTheMaskSays) {
  skia::install();
  kit::SurfaceParams red;
  red.baseColor = {1, 0, 0, 1};
  kit::SurfaceParams blue;
  blue.baseColor = {0, 0, 1, 1};
  const auto shade = [&](float coverage) {
    const Material m =
        over(kit::unlit(red), kit::unlit(blue), kit::maskConstant(coverage));
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
                              kit::maskConstant(1.0f), Blend::Multiply);
  EXPECT_EQ(stackDepth(stack), 1);
  EXPECT_EQ(stackDepth(over(stack, kit::unlit(red), kit::maskConstant(1.0f))),
            2);
  EXPECT_EQ(*under(stack), kit::unlit(red));
  EXPECT_TRUE(skia::shader(stack, {}));
}

// ---------------------------------------------------------------------------
// The grained surfaces and the bank that bounds a field of them.

TEST(Grained, EveryRecipeCompilesAndTwoSeedsAreTwoPieces) {
  skia::install();
  for (const Material& m :
       {kit::stone(), kit::timber(), kit::latten(), kit::board()}) {
    EXPECT_TRUE(skia::shader(m, {}));
    EXPECT_TRUE(m.recipe().has(Target::SkSL));
    EXPECT_TRUE(m.recipe().has(Target::Slang));
  }
  kit::StoneParams a;
  kit::StoneParams b = a;
  b.seed = 3;
  // One seed twice is one material; two seeds are two pieces of one
  // quarry — the same tones, different flecks and veins.
  EXPECT_EQ(kit::stone(a), kit::stone(a));
  EXPECT_FALSE(kit::stone(a) == kit::stone(b));
  const SkBitmap first = shade(kit::stone(a), 64, 64);
  const SkBitmap second = shade(kit::stone(b), 64, 64);
  EXPECT_GT(differing(first, second), 200);
  EXPECT_EQ(differing(first, shade(kit::stone(a), 64, 64)), 0);
  // The grain is luminance: a coloured stone stays its own hue.
  kit::StoneParams red;
  red.hi = {0.8f, 0.2f, 0.2f, 1};
  red.lo = {0.5f, 0.1f, 0.1f, 1};
  red.speckle = 0;
  const SkBitmap ruddy = shade(kit::stone(red), 32, 32);
  for (int y = 0; y < 32; y += 5)
    for (int x = 0; x < 32; x += 5) {
      const SkColor c = ruddy.getColor(x, y);
      EXPECT_GT(SkColorGetR(c), SkColorGetG(c) * 2);
    }
}

TEST(Grained, TimberLightsTheNearArrisAndFlipLightsTheFar) {
  kit::TimberParams t;
  t.span = 40;
  t.tooth = 0;
  t.figure = 0;
  const SkBitmap near = shade(kit::timber(t), 60, 40);
  // The lit arris along the top, the shaded one along the bottom.
  EXPECT_GT(luminance(near.getColor(30, 1)), luminance(near.getColor(30, 20)));
  EXPECT_LT(luminance(near.getColor(30, 38)), luminance(near.getColor(30, 20)));
  t.flip = 1;
  const SkBitmap far = shade(kit::timber(t), 60, 40);
  EXPECT_LT(luminance(far.getColor(30, 1)), luminance(far.getColor(30, 20)));
  EXPECT_GT(luminance(far.getColor(30, 38)), luminance(far.getColor(30, 20)));
  // Turned to run down y, the arrises stand at the sides.
  t.flip = 0;
  t.along = 1;
  const SkBitmap post = shade(kit::timber(t), 40, 60);
  EXPECT_GT(luminance(post.getColor(1, 30)), luminance(post.getColor(20, 30)));
  EXPECT_LT(luminance(post.getColor(38, 30)), luminance(post.getColor(20, 30)));
}

TEST(Grained, LattenSitsOnItsLadderAndSheensAlongItsRun) {
  kit::LattenParams p;
  p.tooth = 0;
  p.from = {0, 0};
  p.to = {64, 0};
  p.sheen = 0.2f;
  p.level = 0.1f;
  const SkBitmap low = shade(kit::latten(p), 64, 8);
  p.level = 0.9f;
  const SkBitmap high = shade(kit::latten(p), 64, 8);
  // A high level is brighter than a low one at every pixel …
  EXPECT_GT(luminance(high.getColor(32, 4)),
            luminance(low.getColor(32, 4)) + 40);
  // … and along the run the sheen climbs the ladder.
  EXPECT_GT(luminance(low.getColor(60, 4)), luminance(low.getColor(3, 4)));
  // A patina is flecks of its colour, at its alpha.
  p.patina = 1.0f;
  p.patinaCell = 8;
  p.patinaColor = {0, 1, 0, 1};
  const SkBitmap green = shade(kit::latten(p), 64, 8);
  int greened = 0;
  for (int x = 0; x < 64; ++x)
    greened += SkColorGetG(green.getColor(x, 4)) > 200 &&
               SkColorGetR(green.getColor(x, 4)) < 60;
  EXPECT_GT(greened, 4);
}

TEST(Grained, BoardIsItsPaintUnderATooth) {
  kit::BoardParams b;
  b.paint = {0.5f, 0.5f, 0.5f, 1};
  const SkBitmap card = shade(kit::board(b), 48, 48);
  int lo = 255, hi = 0;
  for (int y = 0; y < 48; y += 3)
    for (int x = 0; x < 48; x += 3) {
      const int l = luminance(card.getColor(x, y));
      lo = std::min(lo, l);
      hi = std::max(hi, l);
    }
  // Around the paint, and varying: a tooth, not a flat.
  EXPECT_GT(lo, 100);
  EXPECT_LT(hi, 156);
  EXPECT_GT(hi - lo, 4);
  b.tooth = 0;
  b.wear = 0;
  const SkBitmap flat = shade(kit::board(b), 8, 8);
  EXPECT_EQ(luminance(flat.getColor(4, 4)), luminance(flat.getColor(1, 1)));
}

TEST(Bank, FoldsSeedsIntoBucketsAndKeysOnTheRecipeAndParams) {
  kit::Bank bank(24);
  kit::StoneParams p;
  const Material& first = bank.get(kit::stoneRecipe(), p, 5);
  // The bucket IS the seed the recipe reads, and pieces in one bucket
  // are one instance.
  EXPECT_FLOAT_EQ(first.get<float>("seed"), 5.0f);
  EXPECT_EQ(&bank.get(kit::stoneRecipe(), p, 5 + 24), &first);
  EXPECT_NE(&bank.get(kit::stoneRecipe(), p, 6), &first);
  for (uint32_t seed = 0; seed < 1000; ++seed)
    (void)bank.get(kit::stoneRecipe(), p, seed);
  EXPECT_EQ(bank.size(), 24u);
  // A seed the caller left in the params does not reach the key.
  p.seed = 99;
  EXPECT_EQ(&bank.get(kit::stoneRecipe(), p, 5), &first);
  // Another tone is another species, and another recipe another bank row.
  p.hi = {1, 0, 0, 1};
  EXPECT_NE(&bank.get(kit::stoneRecipe(), p, 5), &first);
  EXPECT_EQ(bank.size(), 25u);
  (void)bank.get(kit::boardRecipe(), kit::BoardParams{}, 5);
  EXPECT_EQ(bank.size(), 26u);
  bank.clear();
  EXPECT_EQ(bank.size(), 0u);

  // The maker form banks whatever the caller builds per bucket — a
  // blend, a recipe over a jittered tone — and builds it once.
  kit::Bank makers(4);
  int made = 0;
  for (uint32_t seed = 0; seed < 40; ++seed)
    (void)makers.get(kit::boardRecipe(), kit::BoardParams{}, seed,
                     [&](uint32_t bucket) {
                       ++made;
                       kit::BoardParams q;
                       q.seed = (float)bucket * 7;
                       return kit::board(q);
                     });
  EXPECT_EQ(made, 4);
  EXPECT_EQ(makers.size(), 4u);
  EXPECT_FLOAT_EQ(makers
                      .get(kit::boardRecipe(), kit::BoardParams{}, 9,
                           [](uint32_t) { return kit::board(); })
                      .get<float>("seed"),
                  7.0f);
}

// ---- the embedded shader table --------------------------------------------

TEST(ShaderTable, EveryStockBodyCompiles) {
  skia::install();
  for (const Material& m : kit::everyRecipe()) {
    if (!m.recipe().has(Target::SkSL)) continue;
    EXPECT_TRUE(skia::shader(m, {.resolution = {64, 64}})) << m.recipe().name();
  }
}

TEST(ShaderTable, HoldsEveryFileTheShaderDirectoryDoes) {
  sigil::test::expectShaderTableIsWholeDirectory(kit::shaderSources(),
                                                 SIGIL_MATERIAL_KIT_SHADER_DIR);
}
