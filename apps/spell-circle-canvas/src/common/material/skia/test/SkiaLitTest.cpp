/** @file
 * A lit surface in 2D: a normal map turns the surface toward or away from
 * the light, so the same fill lit from two sides is brightest on opposite
 * halves; a surface with no lighting is its colours; a light that moves
 * makes the pass live while the colours beneath stay one lowered paint;
 * and a surface's own lighting stands over the scene's.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <sigilmaterial/skia/Lit.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmedia/advanced/Skia.h>

using namespace sigil::material;

namespace {

/** A normal map whose left half faces left and whose right half faces
 *  right, both tilted 45 degrees out of the page. */
sk_sp<SkImage> twoFacedNormals(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height, true);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      const bool left = x < width / 2;
      // (±0.707, 0, 0.707) encoded as (n + 1) / 2.
      const uint8_t red = left ? 37 : 218;
      *bitmap.getAddr32(x, y) = SkPreMultiplyARGB(255, red, 128, 218);
    }
  bitmap.setImmutable();
  return bitmap.asImage();
}

Material relief(int width, int height) {
  return from(Color{0.6f, 0.6f, 0.6f, 1})
      .surface({.roughness = 0.8f,
                .normal = image(twoFacedNormals(width, height))});
}

/** @p paint drawn over a box, read back. */
SkBitmap drawn(const Paint& paint, int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height, true);
  SkCanvas canvas(bitmap);
  FrameData frame;
  frame.resolution = {(float)width, (float)height};
  SkPaint fill;
  fill.setShader(skia::shader(paint, frame));
  canvas.drawRect(SkRect::MakeWH((float)width, (float)height), fill);
  return bitmap;
}

float brightness(const SkBitmap& bitmap, int x, int y) {
  const SkColor colour = bitmap.getColor(x, y);
  return (float)(SkColorGetR(colour) + SkColorGetG(colour) +
                 SkColorGetB(colour));
}

}  // namespace

TEST(SkiaLit, TheSideANormalMapTurnsTowardTheLightIsTheBrighterOne) {
  const Material surface = relief(64, 32);
  ASSERT_TRUE(skia::isLit(surface));
  const SkBitmap fromLeft =
      drawn(skia::lit(surface, studio({.direction = 180.0f, .elevation = 30.0f})),
            64, 32);
  const SkBitmap fromRight =
      drawn(skia::lit(surface, studio({.direction = 0.0f, .elevation = 30.0f})),
            64, 32);
  EXPECT_GT(brightness(fromLeft, 12, 16), brightness(fromLeft, 52, 16) + 60)
      << "lit from the left, the half facing left is the brighter";
  EXPECT_GT(brightness(fromRight, 52, 16), brightness(fromRight, 12, 16) + 60)
      << "lit from the right, the half facing right is the brighter";
  // Where the normal faces the same way under both lights the two
  // pictures swap, pixel for pixel, across the middle.
  EXPECT_NEAR(brightness(fromLeft, 12, 16), brightness(fromRight, 52, 16), 3);
}

TEST(SkiaLit, WithNoLightingASurfaceIsItsColours) {
  const Material surface = relief(16, 16);
  const SkBitmap flat = drawn(skia::lit(surface, Lighting{}), 16, 16);
  const SkBitmap colours = drawn(skia::paint(surface), 16, 16);
  EXPECT_EQ(colours.getColor(4, 8), flat.getColor(4, 8));
  EXPECT_EQ(colours.getColor(12, 8), flat.getColor(12, 8));
  // An unlit surface ignores a light too.
  const Material unlit =
      from(Color{0.6f, 0.6f, 0.6f, 1}).surface({.unlit = true});
  EXPECT_FALSE(skia::isLit(unlit));
  EXPECT_EQ(drawn(skia::paint(unlit), 4, 4).getColor(2, 2),
            drawn(skia::lit(unlit, studio()), 4, 4).getColor(2, 2));
}

TEST(SkiaLit, AMovingLightMakesThePassLiveAndOnlyThePass) {
  const Material surface = relief(32, 16);
  sigil::motion::Animatable<float> sun = sigil::motion::animatable(0.0f);
  const Paint turning = skia::lit(surface, studio({.direction = sun}));
  EXPECT_TRUE(turning.isRunning());
  EXPECT_FALSE(skia::paint(surface).isRunning())
      << "the colours beneath do not move";
  EXPECT_FALSE(skia::lit(surface, studio({.direction = 90.0f})).isRunning());
  sun = 180.0f;
  const SkBitmap left = drawn(turning, 32, 16);
  sun = 0.0f;
  const SkBitmap right = drawn(turning, 32, 16);
  EXPECT_GT(brightness(left, 4, 8), brightness(left, 28, 8));
  EXPECT_GT(brightness(right, 28, 8), brightness(right, 4, 8));
}

TEST(SkiaLit, ASurfacesOwnLightingStandsOverTheScenes) {
  const Material scene = relief(8, 8);
  const Lighting sceneLight = studio({.direction = 0.0f});
  EXPECT_EQ(sceneLight, skia::lightingFor(scene, sceneLight));
  const Material own =
      from(Color{0.6f, 0.6f, 0.6f, 1})
          .surface({.lighting = Lighting(studio({.direction = 180.0f}))});
  const Lighting chosen = skia::lightingFor(own, sceneLight);
  ASSERT_TRUE(chosen.light);
  EXPECT_EQ(Lighting(studio({.direction = 180.0f})), chosen);
  EXPECT_FALSE(skia::lightingFor(from(Color{1, 1, 1, 1}), sceneLight))
      << "a material that states no surface takes no light";
}

TEST(SkiaLit, AnEnvironmentAloneLightsAMetal) {
  // An environment bright above the horizon and dark below: a flat metal
  // reflects the band straight toward the viewer, which is neither the
  // black of no light nor the colour painted flat.
  SkBitmap around;
  around.allocN32Pixels(64, 32, true);
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 64; ++x)
      *around.getAddr32(x, y) =
          y < 16 ? SkPreMultiplyARGB(255, 240, 240, 240)
                 : SkPreMultiplyARGB(255, 20, 20, 20);
  around.setImmutable();
  const Material metal = from(Color{0.9f, 0.7f, 0.3f, 1})
                             .surface({.metallic = 1.0f, .roughness = 0.1f});
  const SkBitmap lit =
      drawn(skia::lit(metal, environment(around.asImage())), 8, 8);
  EXPECT_GT(brightness(lit, 4, 4), 30.0f);
}
