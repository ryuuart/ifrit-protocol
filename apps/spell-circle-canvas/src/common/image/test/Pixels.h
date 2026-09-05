#pragma once

/** @file
 * What every image test reads a fixture by: where the committed files
 * stand, one pixel out of a decoded frame, and a comparison a lossy
 * format can pass.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkRefCnt.h>

#include <string>

namespace sigil::image::test {

/** The committed fixture named @p name — 4x4 px files, one still per
 *  format plus a three-frame animation for each animated format,
 *  reached through a compile definition so a test runs from any working
 *  directory. */
inline std::string assetPath(const char* name) {
  return std::string(SIGILIMAGE_TEST_ASSET_DIR "/") + name;
}

/** The pixel at (@p x, @p y) of a decoded frame, unpremultiplied so a
 *  colour reads as the one the file names rather than as one scaled by
 *  its own alpha. */
inline SkColor pixelAt(const sk_sp<SkImage>& image, int x, int y) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32(image->width(), image->height(),
                                          kUnpremul_SkAlphaType));
  EXPECT_TRUE(image->readPixels(nullptr, bitmap.pixmap(), 0, 0));
  return bitmap.getColor(x, y);
}

/** Colour equality within @p tolerance per colour channel and exact on
 *  alpha, which is what a lossy or quantizing format can promise.
 *  @p what names the subject in the failure. */
inline void expectNearColor(SkColor actual, SkColor expected, int tolerance,
                            const char* what) {
  EXPECT_NEAR(int(SkColorGetR(actual)), int(SkColorGetR(expected)), tolerance)
      << what;
  EXPECT_NEAR(int(SkColorGetG(actual)), int(SkColorGetG(expected)), tolerance)
      << what;
  EXPECT_NEAR(int(SkColorGetB(actual)), int(SkColorGetB(expected)), tolerance)
      << what;
  EXPECT_EQ(SkColorGetA(actual), SkColorGetA(expected)) << what;
}

}  // namespace sigil::image::test
