/** @file
 * The pixel reads a device upload takes: a float image comes back as
 * halves with its range intact, and an ordinary eight-bit one is not a
 * float image and reads back as itself. All arithmetic over an SkImage —
 * no device, no context.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <sigilskia/graphite/Pixels.h>

#include <cstdint>
#include <vector>

using sigil::skia::bytePixels;
using sigil::skia::halfFloatPixels;
using sigil::skia::isFloatImage;

namespace {

TEST(SkiaPixels, AFloatImageReadsBackAsHalvesWithItsRangeIntact) {
  // The values above one are the whole reason a panorama is float, and a
  // byte read would put every one of them at white.
  const int w = 4, h = 2;
  std::vector<float> px((size_t)w * h * 4);
  for (size_t i = 0; i < (size_t)w * h; ++i) {
    px[i * 4 + 0] = 6.5f;
    px[i * 4 + 1] = 0.25f;
    px[i * 4 + 2] = 0.5f;
    px[i * 4 + 3] = 1.0f;
  }
  const SkImageInfo info =
      SkImageInfo::Make(w, h, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  sk_sp<SkImage> image = SkImages::RasterFromPixmapCopy(
      {info, px.data(), (size_t)w * 4 * sizeof(float)});
  ASSERT_TRUE(image);
  EXPECT_TRUE(isFloatImage(image));

  const std::vector<uint16_t> halves = halfFloatPixels(image);
  ASSERT_EQ(halves.size(), (size_t)w * h * 4);
  // Half 6.5 is 0x4680; a byte read of the same texel would say 255.
  EXPECT_EQ(halves[0], 0x4680u);
  const std::vector<uint8_t> bytes = bytePixels(image);
  ASSERT_EQ(bytes.size(), (size_t)w * h * 4);
  EXPECT_EQ(bytes[0], 255u);

  // An ordinary 8-bit image is not float and reads back as itself.
  SkBitmap flat;
  flat.allocPixels(SkImageInfo::MakeN32Premul(2, 2));
  flat.eraseColor(SK_ColorGREEN);
  flat.setImmutable();
  EXPECT_FALSE(isFloatImage(flat.asImage()));
  EXPECT_EQ(bytePixels(flat.asImage()).size(), 16u);
}

}  // namespace
