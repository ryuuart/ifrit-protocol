/** @file
 * THE PLAYBACK SOURCES ON THE 2D BACKEND: one channel of a texture that
 * holds data read as stored, a flipbook frame-blended halfway between two
 * cells, and a flow map with no strength standing still. The same values
 * a 3D body plays, painted by Skia.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/texture/Channel.h>
#include <sigilmaterial/texture/Flipbook.h>
#include <sigilmaterial/texture/Flowmap.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmedia/advanced/Skia.h>

#include <cstdint>

#include "support/Shade.h"

namespace sigil::material {
namespace {

/** One texel of straight colour, repeated over a @p size square. */
Texture straightTexels(uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                       int size = 4) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::Make(size, size, kRGBA_8888_SkColorType,
                                       kUnpremul_SkAlphaType));
  for (int y = 0; y < size; ++y)
    for (int x = 0; x < size; ++x) {
      auto* texel = static_cast<uint8_t*>(bitmap.getAddr(x, y));
      texel[0] = r;
      texel[1] = g;
      texel[2] = b;
      texel[3] = a;
    }
  bitmap.setImmutable();
  return Texture(media::PixelSource(bitmap.asImage()));
}

}  // namespace

TEST(SkiaPlayback, AChannelOfADataTextureIsReadAsStored) {
  // A texel whose alpha is zero still holds its red: as data it is read
  // exactly, where as colour its alpha would have taken the red with it.
  const Texture packed =
      straightTexels(200, 100, 50, 0).colorSpace(media::ColorSpace::Data);
  const SkBitmap red =
      test::render(channel(image(packed), ColorChannel::Red), 4, 4);
  EXPECT_NEAR((int)SkColorGetR(red.getColor(2, 2)), 200, 1);
  EXPECT_NEAR((int)SkColorGetA(red.getColor(2, 2)), 255, 0);
  const SkBitmap green =
      test::render(channel(image(packed), ColorChannel::Green), 4, 4);
  EXPECT_NEAR((int)SkColorGetG(green.getColor(2, 2)), 100, 1);
}

TEST(SkiaPlayback, AFlipbookBlendedHalfwayIsTheMeanOfTwoCells) {
  SkBitmap sheet;
  sheet.allocN32Pixels(8, 4);
  for (int y = 0; y < 4; ++y)
    for (int x = 0; x < 8; ++x)
      *sheet.getAddr32(x, y) = x < 4 ? SkPreMultiplyARGB(255, 200, 0, 40)
                                     : SkPreMultiplyARGB(255, 0, 100, 240);
  sheet.setImmutable();
  const Material book = flipbook(
      Texture(media::PixelSource(sheet.asImage())),
      {.columns = 2, .rows = 1, .framesPerSecond = 1, .blendFrames = true});
  const SkBitmap shown = test::render(
      skia::shader(book, {.seconds = 0.5, .resolution = {8, 8}}), 8, 8);
  const SkColor centre = shown.getColor(4, 4);
  EXPECT_NEAR((int)SkColorGetR(centre), 100, 2);
  EXPECT_NEAR((int)SkColorGetG(centre), 50, 2);
  EXPECT_NEAR((int)SkColorGetB(centre), 140, 2);
}

TEST(SkiaPlayback, AFlowWithNoStrengthIsItsSource) {
  const Material source = image(straightTexels(30, 160, 90, 255, 8));
  const Material still = flowmap(
      source, {.flow = straightTexels(255, 0, 128, 255, 8), .strength = 0.0f});
  const SkBitmap a = test::render(
      skia::shader(still, {.seconds = 0.3, .resolution = {8, 8}}), 8, 8);
  const SkBitmap b = test::render(source, 8, 8);
  EXPECT_EQ(a.getColor(4, 4), b.getColor(4, 4));
}

}  // namespace sigil::material
