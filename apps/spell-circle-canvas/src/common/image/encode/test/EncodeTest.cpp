/** @file
 * The encode surface, checked by round trip: every format this build can
 * write is decoded again and compared to what went in.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>

#include <cmath>
#include <string>

#include "sigilimage/decode/ChannelData.h"
#include "sigilimage/decode/Decode.h"
#include "sigilimage/encode/Encode.h"

using namespace sigil::image;

namespace {

constexpr int kSize = 16;

/** Four quadrants of flat colour: enough that a format that dropped a
 *  channel, transposed the image or lost the alpha shows it. */
SkBitmap fixture() {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(kSize, kSize));
  bitmap.eraseColor(SK_ColorBLACK);
  bitmap.erase(SK_ColorRED, SkIRect::MakeXYWH(0, 0, 8, 8));
  bitmap.erase(SK_ColorGREEN, SkIRect::MakeXYWH(8, 0, 8, 8));
  bitmap.erase(SK_ColorBLUE, SkIRect::MakeXYWH(0, 8, 8, 8));
  bitmap.erase(SK_ColorWHITE, SkIRect::MakeXYWH(8, 8, 8, 8));
  return bitmap;
}

/** A float fixture, with values above 1 so a format that clamped to LDR
 *  is caught. */
SkBitmap floatFixture() {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::Make(kSize, kSize, kRGBA_F32_SkColorType,
                                       kPremul_SkAlphaType));
  auto* px = static_cast<float*>(bitmap.getPixels());
  for (int i = 0; i < kSize * kSize; ++i) {
    px[i * 4 + 0] = 4.0f;
    px[i * 4 + 1] = 0.5f;
    px[i * 4 + 2] = 0.25f;
    px[i * 4 + 3] = 1.0f;
  }
  return bitmap;
}

/** The decoded bytes back as a bitmap in a colour type we can index. */
std::optional<SkBitmap> roundTrip(const sk_sp<SkData>& encoded,
                                  SkColorType type) {
  auto asset = decodeImage(static_cast<const std::byte*>(encoded->data()),
                           encoded->size());
  if (!asset || asset->frames().empty()) return std::nullopt;
  const sk_sp<SkImage>& image = asset->frames().front().image;
  SkBitmap out;
  out.allocPixels(SkImageInfo::Make(image->width(), image->height(), type,
                                    kPremul_SkAlphaType));
  if (!image->readPixels(nullptr, out.pixmap(), 0, 0)) return std::nullopt;
  return out;
}

}  // namespace

/** ONE FORMAT AT ONE QUALITY, and whether it promises the pixels back
 *  unchanged. PNG is lossless at every setting; WebP at 100 selects the
 *  format's lossless encoder rather than its lossy one at maximum
 *  quality, and below 100 it is the lossy one. */
struct RoundTripCase {
  Format format;
  int quality;
  bool lossless;
  const char* label;
};

class EncodeRoundTrip : public ::testing::TestWithParam<RoundTripCase> {};

TEST_P(EncodeRoundTrip, TheDecodedPictureIsThePictureThatWentIn) {
  const RoundTripCase& subject = GetParam();
  const SkBitmap src = fixture();
  const sk_sp<SkData> bytes =
      encodeImage(src.pixmap(), subject.format, {.quality = subject.quality});
  ASSERT_TRUE(bytes);
  const std::optional<SkBitmap> back = roundTrip(bytes, kN32_SkColorType);
  ASSERT_TRUE(back);
  EXPECT_EQ(back->width(), kSize);
  EXPECT_EQ(back->height(), kSize);
  if (subject.lossless) {
    EXPECT_EQ(back->getColor(2, 2), SK_ColorRED);
    EXPECT_EQ(back->getColor(12, 2), SK_ColorGREEN);
    EXPECT_EQ(back->getColor(2, 12), SK_ColorBLUE);
    EXPECT_EQ(back->getColor(12, 12), SK_ColorWHITE);
    return;
  }
  // Sampled at a quadrant centre rather than an edge: a lossy codec's
  // chroma subsampling smears the boundary between two flat fields,
  // which is the loss the format is for, not a defect in the encode.
  const SkColor red = back->getColor(3, 3);
  EXPECT_GT(SkColorGetR(red), 200u);
  EXPECT_LT(SkColorGetG(red), 60u);
  const SkColor green = back->getColor(11, 3);
  EXPECT_GT(SkColorGetG(green), 200u);
}

INSTANTIATE_TEST_SUITE_P(
    EveryFormatThisBuildWrites, EncodeRoundTrip,
    ::testing::Values(RoundTripCase{Format::Png, 100, true, "Png"},
                      RoundTripCase{Format::Jpeg, 95, false, "Jpeg95"},
                      RoundTripCase{Format::Webp, 100, true, "Webp100"},
                      RoundTripCase{Format::Webp, 80, false, "Webp80"}),
    [](const ::testing::TestParamInfo<RoundTripCase>& info) {
      return std::string(info.param.label);
    });

TEST(Encode, TheImageOverloadReadsBackAndEncodes) {
  const SkBitmap src = fixture();
  const sk_sp<SkImage> image = src.asImage();
  ASSERT_TRUE(image);
  const sk_sp<SkData> bytes = encodeImage(*image, Format::Png);
  ASSERT_TRUE(bytes);
  const std::optional<SkBitmap> back = roundTrip(bytes, kN32_SkColorType);
  ASSERT_TRUE(back);
  EXPECT_EQ(back->getColor(2, 2), SK_ColorRED);
}

TEST(Encode, ExrCarriesValuesAboveOne) {
  const SkBitmap src = floatFixture();
  const sk_sp<SkData> bytes = encodeImage(src.pixmap(), Format::Exr);
#ifdef SIGILIMAGE_HAS_OIIO_ENCODE
  ASSERT_TRUE(bytes);
  auto channels = decodeChannels(static_cast<const std::byte*>(bytes->data()),
                                 bytes->size(), "round.exr");
  ASSERT_TRUE(channels);
  EXPECT_EQ(channels->width, kSize);
  const int r = channels->index("R");
  ASSERT_GE(r, 0);
  // Half float, so the tolerance is the format's step near 4, not ours.
  EXPECT_NEAR(channels->at(0, 0, r), 4.0f, 0.01f);
#else
  // The degrade rule: without the backend the format simply fails to
  // encode, the same way it fails to decode.
  EXPECT_FALSE(bytes);
#endif
}

TEST(Encode, NamedChannelsGoOutAsLayersAndComeBackByName) {
  // Two groups and a lone depth in one file, which is the shape a
  // renderer's AOVs have and the shape DecodeOptions::layer exists to
  // read: the names ARE the layers.
  ChannelData channels;
  channels.width = kSize;
  channels.height = kSize;
  channels.floatingPoint = true;
  channels.names = {"diffuse.R", "diffuse.G", "diffuse.B",
                    "glow.R",    "glow.G",    "glow.B",
                    "depth.Z"};
  channels.data.assign((size_t)kSize * kSize * channels.names.size(), 0.0f);
  for (int y = 0; y < kSize; ++y)
    for (int x = 0; x < kSize; ++x) {
      float* px = channels.data.data() +
                  ((size_t)y * kSize + x) * channels.names.size();
      px[0] = 0.25f;  // diffuse: a colour under one
      px[1] = 0.5f;
      px[2] = 0.75f;
      px[3] = 4.0f;  // glow: a colour above it, so LDR clamping shows
      px[4] = 2.0f;
      px[5] = 1.0f;
      px[6] = (float)x;  // depth: a ramp, so a transpose shows
    }

  const sk_sp<SkData> bytes = encodeImage(channels, Format::Exr);
#ifdef SIGILIMAGE_HAS_OIIO_ENCODE
  ASSERT_TRUE(bytes);
  const auto back = decodeChannels(static_cast<const std::byte*>(bytes->data()),
                                   bytes->size(), "layers.exr");
  ASSERT_TRUE(back);
  EXPECT_EQ(back->width, kSize);
  EXPECT_EQ(back->height, kSize);
  EXPECT_TRUE(back->floatingPoint);
  for (const std::string& name : channels.names)
    EXPECT_GE(back->index(name), 0) << name << " did not come back";
  // Half float, so the tolerance is the format's step and not ours.
  EXPECT_NEAR(back->at(3, 3, back->index("glow.R")), 4.0f, 0.01f);
  EXPECT_NEAR(back->at(3, 3, back->index("diffuse.B")), 0.75f, 0.001f);
  EXPECT_NEAR(back->at(5, 2, back->index("depth.Z")), 5.0f, 0.01f);

  // AND THE LAYER SELECTION READS IT. This is the door the decode side
  // has carried all along with nothing in this tree able to write its
  // fixture.
  const sk_sp<SkImage> lit = back->makeImage("glow");
  ASSERT_TRUE(lit);
  const sk_sp<SkImage> flat = back->makeImage("diffuse");
  ASSERT_TRUE(flat);
  SkBitmap read;
  ASSERT_TRUE(read.tryAllocPixels(
      SkImageInfo::Make(kSize, kSize, kRGBA_F32_SkColorType,
                        kPremul_SkAlphaType)));
  ASSERT_TRUE(lit->readPixels(nullptr, read.pixmap(), 0, 0));
  const float* pixel = (const float*)read.getAddr(4, 4);
  EXPECT_NEAR(pixel[0], 4.0f, 0.01f);
  EXPECT_NEAR(pixel[1], 2.0f, 0.01f);
  EXPECT_NEAR(pixel[2], 1.0f, 0.01f);
  // No alpha in the group, so the composite fills it.
  EXPECT_NEAR(pixel[3], 1.0f, 0.001f);
  ASSERT_TRUE(flat->readPixels(nullptr, read.pixmap(), 0, 0));
  pixel = (const float*)read.getAddr(4, 4);
  EXPECT_NEAR(pixel[0], 0.25f, 0.001f);
  // A layer the file does not carry is nothing, not a black picture.
  EXPECT_FALSE(back->makeImage("specular"));
#else
  EXPECT_FALSE(bytes);
#endif
}

TEST(Encode, OnlyExrHoldsNamedChannels) {
  ChannelData channels;
  channels.width = 2;
  channels.height = 2;
  channels.names = {"R", "G", "B"};
  channels.data.assign(2 * 2 * 3, 0.5f);
  // Three or four channels with fixed meanings have nothing to do with a
  // name, so the other formats decline rather than dropping them.
  EXPECT_FALSE(encodeImage(channels, Format::Png));
  EXPECT_FALSE(encodeImage(channels, Format::Jpeg));
  EXPECT_FALSE(encodeImage(channels, Format::Webp));
  // …and a value whose names and planes disagree describes no file.
  ChannelData ragged = channels;
  ragged.names.push_back("A");
  EXPECT_FALSE(encodeImage(ragged, Format::Exr));
}

TEST(Encode, EmptyPixelsEncodeToNothing) {
  const SkPixmap none;
  EXPECT_FALSE(encodeImage(none, Format::Png));
}

TEST(Encode, AFilenameNamesItsFormat) {
  EXPECT_EQ(formatForPath("plate.png"), Format::Png);
  EXPECT_EQ(formatForPath("shot.JPG"), Format::Jpeg);
  EXPECT_EQ(formatForPath("shot.jpeg"), Format::Jpeg);
  EXPECT_EQ(formatForPath("tile.webp"), Format::Webp);
  EXPECT_EQ(formatForPath("sky.exr"), Format::Exr);
  EXPECT_EQ(formatForPath("notes.txt"), std::nullopt);
  EXPECT_EQ(formatForPath("noextension"), std::nullopt);
  EXPECT_STREQ(extensionFor(Format::Png), ".png");
  EXPECT_STREQ(extensionFor(Format::Exr), ".exr");
}
