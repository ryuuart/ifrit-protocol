/** @file
 * A surface read back one map at a time, unshaded: each role of a material
 * that states its maps, the stock numbers of one that states none, the
 * own-colour reading of an unlit material, a height-derived normal, and
 * the same maps placed for one draw.
 */

#include "SkiaLitTestSupport.h"

namespace {

using texture::Role;

/** An opaque image of one colour, as a map is supplied. */
Material solidImage(SkColor colour) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(8, 8, true);
  bitmap.eraseColor(colour);
  bitmap.setImmutable();
  return image(bitmap.asImage(), {.repeat = Repeat::Pad});
}

void expectPixel(const SkColor4f& actual, float red, float green, float blue,
                 float alpha, float tolerance = .004f) {
  EXPECT_NEAR(actual.fR, red, tolerance);
  EXPECT_NEAR(actual.fG, green, tolerance);
  EXPECT_NEAR(actual.fB, blue, tolerance);
  EXPECT_NEAR(actual.fA, alpha, tolerance);
}

}  // namespace

TEST(SkiaLitMap, EachRoleOfAMaterialWithMapsReadsBackThatMap) {
  const Material material =
      from(solidImage(SkColorSetRGB(204, 102, 51)))
          .surface({.metallic = solidImage(SkColorSetRGB(64, 0, 0)),
                    .roughness = solidImage(SkColorSetRGB(153, 0, 0)),
                    .occlusion = solidImage(SkColorSetRGB(230, 0, 0)),
                    // (0, 0.6, 0.8) encoded, green up the picture.
                    .normal = solidImage(SkColorSetRGB(128, 204, 230)),
                    .emission = {1, 1, 1, 1},
                    .emissionStrength = 1,
                    .emissionMap = solidImage(SkColorSetRGB(26, 51, 77))});
  const skia::LitSurface prepared(material);
  expectPixel(sampled(prepared.asMap(Role::BaseColor)), .8f, .4f, .2f, 1);
  expectPixel(sampled(prepared.asMap(Role::Normal)), .5f, .8f, .9f, 1);
  expectPixel(sampled(prepared.asMap(Role::Roughness)), .6f, .6f, .6f, 1);
  expectPixel(sampled(prepared.asMap(Role::Metallic)), .25f, .25f, .25f, 1);
  expectPixel(sampled(prepared.asMap(Role::Occlusion)), .9f, .9f, .9f, 1);
  expectPixel(sampled(prepared.asMap(Role::Emissive)), .1f, .2f, .3f, 1);
  // The one-call spelling answers the same maps.
  expectPixel(sampled(skia::asMap(material, Role::Roughness)), .6f, .6f, .6f,
              1);
}

TEST(SkiaLitMap, ANormalAuthoredGreenDownComesBackGreenUp) {
  const Material material =
      from(Color{.5f, .5f, .5f, 1})
          .surface({.normal = solidImage(SkColorSetRGB(128, 204, 230)),
                    .normalDirectX = true});
  expectPixel(sampled(skia::asMap(material, Role::Normal)), .5f, .2f, .9f, 1);
}

TEST(SkiaLitMap, ALitMaterialStatingNoMapsReadsTheStockNumbers) {
  const Material material = from(Color{.2f, .4f, .6f, 1}).surface({});
  const skia::LitSurface prepared(material);
  expectPixel(sampled(prepared.asMap(Role::BaseColor)), .2f, .4f, .6f, 1);
  expectPixel(sampled(prepared.asMap(Role::Normal)), .5f, .5f, 1, 1);
  expectPixel(sampled(prepared.asMap(Role::Roughness)), .5f, .5f, .5f, 1);
  expectPixel(sampled(prepared.asMap(Role::Metallic)), 0, 0, 0, 1);
  expectPixel(sampled(prepared.asMap(Role::Occlusion)), 1, 1, 1, 1);
  expectPixel(sampled(prepared.asMap(Role::Emissive)), 0, 0, 0, 1);
}

TEST(SkiaLitMap, StatedNumbersAndEmissionReachTheirMaps) {
  const Material material = from(Color{.2f, .4f, .6f, 1})
                                .surface({.metallic = .75f,
                                          .roughness = .25f,
                                          .occlusion = .5f,
                                          .emission = {.5f, .25f, 1, 1},
                                          .emissionStrength = .5f});
  expectPixel(sampled(skia::asMap(material, Role::Roughness)), .25f, .25f, .25f,
              1);
  expectPixel(sampled(skia::asMap(material, Role::Metallic)), .75f, .75f, .75f,
              1);
  expectPixel(sampled(skia::asMap(material, Role::Occlusion)), .5f, .5f, .5f,
              1);
  expectPixel(sampled(skia::asMap(material, Role::Emissive)), .25f, .125f, .5f,
              1);
}

TEST(SkiaLitMap, AnUnlitMaterialIsItsOwnColourWithItsCoverage) {
  // Read back unpremultiplied: each map's value at the colour's alpha.
  const Color colour{.8f, .6f, .4f, .5f};
  for (const Material& material :
       {Material(colour),
        from(colour).surface({.roughness = .1f, .unlit = true})}) {
    expectPixel(sampled(skia::asMap(material, Role::BaseColor)), 0, 0, 0, .5f);
    expectPixel(sampled(skia::asMap(material, Role::Emissive)), .8f, .6f, .4f,
                .5f);
    expectPixel(sampled(skia::asMap(material, Role::Normal)), .5f, .5f, 1, .5f);
    expectPixel(sampled(skia::asMap(material, Role::Roughness)), 1, 1, 1, .5f);
    expectPixel(sampled(skia::asMap(material, Role::Metallic)), 0, 0, 0, .5f);
    expectPixel(sampled(skia::asMap(material, Role::Occlusion)), 1, 1, 1, .5f);
  }
}

TEST(SkiaLitMap, ALitMaterialsCoverageIsEveryMapsAlpha) {
  const Material material =
      from(Color{.2f, .4f, .6f, .5f}).surface({.roughness = .8f});
  expectPixel(sampled(skia::asMap(material, Role::BaseColor)), .2f, .4f, .6f,
              .5f);
  expectPixel(sampled(skia::asMap(material, Role::Roughness)), .8f, .8f, .8f,
              .5f);
  expectPixel(sampled(skia::asMap(material, Role::Normal)), .5f, .5f, 1, .5f);
}

TEST(SkiaLitMap, AHeightDerivedNormalMatchesTheNormalItWasDerivedTo) {
  struct Parameters {
    float width = 64;
  };
  const Material height = shader(
      "half4 main(float2 p) { float h = clamp(p.x / width, 0, 1);"
      " return half4(h, h, h, 1); }",
      Parameters{});
  const Material normal = surface::normalFromHeight(height, {.depth = 12});
  const Material material =
      from(Color{.5f, .5f, .5f, 1}).surface({.normal = normal});
  const SkBitmap actual =
      drawnFloat(skia::asMap(material, Role::Normal), 64, 16);
  const SkBitmap expected = drawnFloat(skia::paint(normal), 64, 16);
  bool tilted = false;
  for (int x : {8, 24, 40, 56}) {
    const SkColor4f a = actual.getColor4f(x, 8), b = expected.getColor4f(x, 8);
    EXPECT_NEAR(a.fR, b.fR, .002f) << x;
    EXPECT_NEAR(a.fG, b.fG, .002f) << x;
    EXPECT_NEAR(a.fB, b.fB, .002f) << x;
    tilted |= std::abs(a.fR - .5f) > .05f;
  }
  EXPECT_TRUE(tilted);
}

TEST(SkiaLitMap, AMapPlacedForOneDrawMatchesTheMapOverTheSameBox) {
  const Material material =
      from(solidImage(SkColorSetRGB(204, 102, 51)))
          .surface({.roughness = solidImage(SkColorSetRGB(153, 0, 0)),
                    .normal = solidImage(SkColorSetRGB(128, 204, 230))});
  const skia::LitSurface prepared(material);
  const glm::mat3 mapping{8, 0, 0, 0, 8, 0, 0, 0, 1};
  for (Role role : {Role::BaseColor, Role::Normal, Role::Roughness}) {
    const auto placed = prepared.mapShader(role, {}, mapping);
    ASSERT_NE(placed, nullptr);
    const SkColor4f expected = sampled(prepared.asMap(role));
    expectPixel(sampled(skia::paint(placed)), expected.fR, expected.fG,
                expected.fB, expected.fA, .0001f);
  }
  EXPECT_EQ(prepared.mapShader(Role::Height, {}, mapping), nullptr);
}

TEST(SkiaLitMap, ARoleWithNoMapPaintsNothing) {
  const Material material = from(Color{.2f, .4f, .6f, 1}).surface({});
  for (Role role : {Role::Unknown, Role::Packed, Role::Height, Role::Opacity,
                    Role::Specular}) {
    EXPECT_FALSE(skia::isSurfaceMapRole(role));
    EXPECT_TRUE(skia::asMap(material, role).isNone());
  }
  EXPECT_TRUE(skia::isSurfaceMapRole(Role::Emissive));
}
