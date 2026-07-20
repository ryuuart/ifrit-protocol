#include <sigilimage/ImageAsset.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkData.h>
#include <include/core/SkSurface.h>

#include <gtest/gtest.h>

#include <string>

using sigil::image::ImageAsset;

namespace {

std::string assetPath(const char *name) {
  return std::string(IFRIT_IMAGE_TEST_ASSET_DIR "/") + name;
}

/** Reads the pixel at (x, y) of a decoded frame as unpremultiplied color. */
SkColor pixelAt(const sk_sp<SkImage> &image, int x, int y) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32(image->width(), image->height(),
                                          kUnpremul_SkAlphaType));
  EXPECT_TRUE(image->readPixels(nullptr, bitmap.pixmap(), 0, 0));
  return bitmap.getColor(x, y);
}

void expectNearColor(SkColor actual, SkColor expected, int tolerance,
                     const char *what) {
  EXPECT_NEAR(int(SkColorGetR(actual)), int(SkColorGetR(expected)), tolerance)
      << what;
  EXPECT_NEAR(int(SkColorGetG(actual)), int(SkColorGetG(expected)), tolerance)
      << what;
  EXPECT_NEAR(int(SkColorGetB(actual)), int(SkColorGetB(expected)), tolerance)
      << what;
  EXPECT_EQ(SkColorGetA(actual), SkColorGetA(expected)) << what;
}

TEST(ImageAsset, DecodesEveryStillFormat) {
  // JPEG is lossy and GIF/AVIF quantize, so compare with a small tolerance.
  const struct {
    const char *file;
    int tolerance;
  } kCases[] = {
      {"still.png", 0},  {"still.jpg", 12}, {"still.webp", 0},
      {"still.gif", 0},  {"still.avif", 2},
  };
  for (const auto &testCase : kCases) {
    auto asset = ImageAsset::load(assetPath(testCase.file));
    ASSERT_TRUE(asset.has_value()) << testCase.file;
    EXPECT_EQ(asset->width(), 4) << testCase.file;
    EXPECT_EQ(asset->height(), 4) << testCase.file;
    EXPECT_FALSE(asset->animated()) << testCase.file;
    ASSERT_EQ(asset->frames().size(), 1u) << testCase.file;
    EXPECT_EQ(asset->frames()[0].durationMs, 0.0f) << testCase.file;
    expectNearColor(pixelAt(asset->frames()[0].image, 1, 1), SK_ColorRED,
                    testCase.tolerance, testCase.file);
  }
}

TEST(ImageAsset, DecodesAnimationsWithCompositedFrames) {
  const SkColor kFrameColors[] = {SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE};
  const struct {
    const char *file;
    int tolerance;
  } kCases[] = {{"anim.gif", 0}, {"anim.webp", 0}, {"anim.avif", 2}};
  for (const auto &testCase : kCases) {
    auto asset = ImageAsset::load(assetPath(testCase.file));
    ASSERT_TRUE(asset.has_value()) << testCase.file;
    EXPECT_TRUE(asset->animated()) << testCase.file;
    ASSERT_EQ(asset->frames().size(), 3u) << testCase.file;
    EXPECT_EQ(asset->repetitionCount(), ImageAsset::kInfinite)
        << testCase.file;
    EXPECT_FLOAT_EQ(asset->totalDurationMs(), 300.0f) << testCase.file;
    for (int i = 0; i < 3; ++i) {
      EXPECT_FLOAT_EQ(asset->frames()[i].durationMs, 100.0f)
          << testCase.file << " frame " << i;
      expectNearColor(pixelAt(asset->frames()[i].image, 2, 2), kFrameColors[i],
                      testCase.tolerance, testCase.file);
    }
  }
}

TEST(ImageAsset, FrameAtWalksAndLoopsTheTimeline) {
  auto asset = ImageAsset::load(assetPath("anim.gif"));
  ASSERT_TRUE(asset.has_value());
  const auto &frames = asset->frames();
  EXPECT_EQ(asset->frameAt(0).image, frames[0].image);
  EXPECT_EQ(asset->frameAt(99).image, frames[0].image);
  EXPECT_EQ(asset->frameAt(150).image, frames[1].image);
  EXPECT_EQ(asset->frameAt(250).image, frames[2].image);
  EXPECT_EQ(asset->frameAt(310).image, frames[0].image); // looped
  EXPECT_EQ(asset->frameAt(-5).image, frames[0].image);  // clamped
}

TEST(ImageAsset, FramesDrawOntoACanvas) {
  auto asset = ImageAsset::load(assetPath("anim.webp"));
  ASSERT_TRUE(asset.has_value());

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(8, 8));
  ASSERT_TRUE(surface);
  surface->getCanvas()->clear(SK_ColorBLACK);
  surface->getCanvas()->drawImage(asset->frameAt(150).image, 2, 2);

  SkBitmap readback;
  readback.allocPixels(
      SkImageInfo::MakeN32(8, 8, kUnpremul_SkAlphaType));
  ASSERT_TRUE(surface->readPixels(readback.pixmap(), 0, 0));
  EXPECT_EQ(readback.getColor(0, 0), SK_ColorBLACK);   // outside the image
  EXPECT_EQ(readback.getColor(3, 3), SK_ColorGREEN);   // frame 1 painted
  EXPECT_EQ(readback.getColor(7, 7), SK_ColorBLACK);   // outside again
}

TEST(ImageAsset, RejectsUnsupportedBytes) {
  const char kGarbage[] = "definitely not an image";
  EXPECT_FALSE(
      ImageAsset::decode(SkData::MakeWithCopy(kGarbage, sizeof(kGarbage)))
          .has_value());
  EXPECT_FALSE(ImageAsset::decode(nullptr).has_value());
  EXPECT_FALSE(ImageAsset::load(assetPath("missing.png")).has_value());
}

} // namespace
