/** @file
 * The ocio feature: an exponent transform bakes to a per-channel
 * response row that darkens mid-grey by the expected amount and leaves
 * white alone; that row lowers to a table on an eight-bit surface and
 * paints what the program paints, while a float surface and an unknown
 * one keep the program; and a transform that mixes channels bakes the
 * volume and keeps the program everywhere. A config that cannot be read
 * fails soft, which needs no OpenColorIO to ask; every other case here
 * skips where the transforms are unavailable, and the binary carries the
 * `ocio` label that says so.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkSamplingOptions.h>
#include <sigilmaterial/ocio/Ocio.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Texture.h>

#include <cmath>
#include <cstdlib>

using namespace sigil::material;

TEST(Ocio, AConfigThatCannotBeReadFailsSoftWithAnEmptyLut) {
  // No OCIO needed and none asked for: a transform the library cannot
  // bake answers a material with nothing in its LUT slot rather than
  // throwing, so a caller naming a config that is not there paints the
  // content it was handed.
  const Material bad = ocio::viewTransform("ocio://no-such-config", "x", "y");
  EXPECT_EQ(bad.leaf("lut"), nullptr);
}

TEST(Ocio, AnExponentBakesToAResponseRowThatGradesTheContent) {
  if (!ocio::available()) GTEST_SKIP() << "OCIO raw config unavailable";
  skia::install();
  Material grade = ocio::exponent(2.2f);
  ASSERT_NE(grade.leaf("lut"), nullptr);
  // An exponent's channels are independent, so the bake is one row of
  // 256 samples and the recipe says so.
  EXPECT_EQ(grade.recipe().channelwiseSlot(), "lut");
  EXPECT_FLOAT_EQ(grade.get<float>("lutSize"), 256.0f);
  // Content: a 4x4 mid-grey with a white pixel.
  SkBitmap content;
  content.allocPixels(SkImageInfo::MakeN32Premul(4, 4));
  content.eraseColor(SkColorSetARGB(255, 128, 128, 128));
  content.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 0, 1, 1));
  content.setImmutable();
  grade.child("content", Texture::of(content.asImage()));
  SkBitmap out;
  out.allocPixels(SkImageInfo::MakeN32Premul(4, 4));
  SkCanvas canvas(out);
  SkPaint paint;
  paint.setShader(skia::shader(grade, {}));
  canvas.drawPaint(paint);
  const int grey = (int)SkColorGetR(out.getColor(2, 2));
  const int expected = (int)std::lround(255.0 * std::pow(128.0 / 255.0, 2.2));
  EXPECT_NEAR(grey, expected, 3);
  EXPECT_EQ(SkColorGetR(out.getColor(0, 0)), 255u);
}

namespace {

/** A row of 256 opaque colours whose three channels never agree, so a
 *  table that crossed its channels could not pass unnoticed. */
SkBitmap colourRamp() {
  SkBitmap ramp;
  ramp.allocPixels(SkImageInfo::MakeN32Premul(256, 1));
  for (int i = 0; i < 256; ++i)
    *ramp.getAddr32(i, 0) = SkPreMultiplyColor(
        SkColorSetARGB(255, (uint32_t)i, (uint32_t)(255 - i),
                       (uint32_t)((i * 7) % 256)));
  ramp.setImmutable();
  return ramp;
}

}  // namespace

TEST(Ocio, AChannelwiseViewLowersToATableThatPaintsWhatTheProgramPaints) {
  if (!ocio::available()) GTEST_SKIP() << "OCIO raw config unavailable";
  skia::install();
  const Material grade = ocio::exponent(1.08f);
  ASSERT_EQ(grade.recipe().channelwiseSlot(), "lut");

  const skia::Effect lowered = skia::Effect::recipe(grade, kN32_SkColorType);
  ASSERT_NE(lowered.colorFilter(), nullptr)
      << "an eight-bit surface must lower a channelwise view to a table";
  EXPECT_EQ(lowered.imageFilter(), nullptr)
      << "the table replaces the program, it does not join it";

  const SkBitmap ramp = colourRamp();
  const SkImageInfo info = SkImageInfo::MakeN32Premul(256, 1);

  // What the program paints: the recipe with the ramp in its content slot.
  Material shaded = grade;
  shaded.child("content", Texture::of(ramp.asImage()));
  SkBitmap viaProgram;
  viaProgram.allocPixels(info);
  SkCanvas programCanvas(viaProgram);
  SkPaint programPaint;
  programPaint.setShader(skia::shader(shaded, {}));
  programCanvas.drawPaint(programPaint);

  // What the table paints: the same ramp through the lowered filter.
  SkBitmap viaTable;
  viaTable.allocPixels(info);
  SkCanvas tableCanvas(viaTable);
  SkPaint tablePaint;
  tablePaint.setColorFilter(lowered.colorFilter());
  tableCanvas.drawImage(ramp.asImage(), 0, 0, SkSamplingOptions{}, &tablePaint);

  // A 1.08 exponent moves the mid-tones by several codes, so a table that
  // had silently become the identity would fail this.
  const SkColor midProgram = viaProgram.getColor(128, 0);
  EXPECT_NE((int)SkColorGetR(midProgram), 128)
      << "the grade did not change the content at all";

  for (int i = 0; i < 256; ++i) {
    const SkColor a = viaProgram.getColor(i, 0);
    const SkColor b = viaTable.getColor(i, 0);
    ASSERT_LE(std::abs((int)SkColorGetR(a) - (int)SkColorGetR(b)), 1)
        << "red parts at ramp entry " << i;
    ASSERT_LE(std::abs((int)SkColorGetG(a) - (int)SkColorGetG(b)), 1)
        << "green parts at ramp entry " << i;
    ASSERT_LE(std::abs((int)SkColorGetB(a) - (int)SkColorGetB(b)), 1)
        << "blue parts at ramp entry " << i;
    ASSERT_EQ(SkColorGetA(b), 255u) << "alpha moved at ramp entry " << i;
  }
}

TEST(Ocio, ASurfaceATableCannotCarryKeepsTheProgram) {
  if (!ocio::available()) GTEST_SKIP() << "OCIO raw config unavailable";
  skia::install();
  const Material grade = ocio::exponent(1.08f);
  // More precision than 256 codes per channel, and a canvas backed by
  // neither raster nor GPU, which is what kUnknown says.
  for (const SkColorType surface :
       {kRGBA_F16_SkColorType, kRGBA_F32_SkColorType, kUnknown_SkColorType}) {
    const skia::Effect kept = skia::Effect::recipe(grade, surface);
    EXPECT_EQ(kept.colorFilter(), nullptr) << "colour type " << (int)surface;
    EXPECT_NE(kept.imageFilter(), nullptr) << "colour type " << (int)surface;
  }
}

TEST(Ocio, AViewThatMixesChannelsBakesTheVolumeAndKeepsTheProgram) {
  if (!ocio::available()) GTEST_SKIP() << "OCIO raw config unavailable";
  skia::install();
  // A primary conversion: its matrix reads all three input channels into
  // each output channel, so the per-channel proof must fail.
  const Material mix =
      ocio::convert("ocio://default", "ACEScg", "sRGB - Texture");
  if (mix.leaf("lut") == nullptr)
    GTEST_SKIP() << "the built-in config offers no such conversion";
  EXPECT_TRUE(mix.recipe().channelwiseSlot().empty())
      << "a channel-mixing transform must bake the volume";
  for (const SkColorType surface : {kN32_SkColorType, kRGBA_F16_SkColorType}) {
    const skia::Effect kept = skia::Effect::recipe(mix, surface);
    EXPECT_EQ(kept.colorFilter(), nullptr) << "colour type " << (int)surface;
    EXPECT_NE(kept.imageFilter(), nullptr) << "colour type " << (int)surface;
  }
}
