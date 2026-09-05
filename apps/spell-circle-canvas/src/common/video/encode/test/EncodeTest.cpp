#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkData.h>
#include <sigilvideo/decode/Decode.h>
#include <sigilvideo/encode/Encode.h>

#include <array>
#include <cmath>
#include <cstddef>

namespace {

SkColor centerColor(const sk_sp<SkImage>& image) {
  if (!image) return SK_ColorTRANSPARENT;
  SkBitmap pixel;
  pixel.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  if (!image->readPixels(nullptr, pixel.pixmap(), image->width() / 2,
                         image->height() / 2))
    return SK_ColorTRANSPARENT;
  return pixel.getColor(0, 0);
}

/** Within what 8-bit 4:2:0 quantization of a flat frame moves a channel.
 *  A mismatched matrix between the encoder's conversion and the decoder's
 *  moves a saturated primary far more than this. */
bool nearChannel(int actual, int expected) {
  return std::abs(actual - expected) <= 8;
}

}  // namespace

TEST(VideoEncode, Mp4RoundTripsFramesAndTiming) {
  constexpr int kWidth = 96;
  constexpr int kHeight = 64;
  constexpr int kFps = 10;
  auto encoder = sigil::video::Encoder::make(
      sigil::video::Format::Mp4,
      {.width = kWidth,
       .height = kHeight,
       .framesPerSecond = kFps,
       .bitRate = 1'000'000,
       .hardware = sigil::video::HardwarePreference::Disabled});
  ASSERT_NE(encoder, nullptr);

  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(kWidth, kHeight));
  constexpr std::array<SkColor, 4> colors = {SK_ColorRED, SK_ColorGREEN,
                                             SK_ColorBLUE, SK_ColorWHITE};
  for (SkColor color : colors) {
    bitmap.eraseColor(color);
    ASSERT_TRUE(encoder->append(bitmap.pixmap())) << encoder->error();
  }
  EXPECT_EQ(encoder->frameCount(), 4);
  sk_sp<SkData> mp4 = encoder->finish();
  ASSERT_NE(mp4, nullptr) << encoder->error();
  EXPECT_GT(mp4->size(), 100u);

  const auto* bytes = static_cast<const std::byte*>(mp4->data());
  const std::optional<sigil::video::VideoProbe> probe =
      sigil::video::probeVideo(bytes, mp4->size(), "roundtrip.mp4");
  ASSERT_TRUE(probe);
  EXPECT_EQ(probe->width, kWidth);
  EXPECT_EQ(probe->height, kHeight);
  EXPECT_NEAR(probe->frameRate, kFps, 0.1);
  EXPECT_GE(probe->durationSeconds, 0.39);

  sigil::video::DecodeOptions decodeOptions;
  decodeOptions.hardware = sigil::video::HardwarePreference::Disabled;
  decodeOptions.cachedFrames = 2;
  std::shared_ptr<sigil::video::Video> video = sigil::video::decodeVideo(
      bytes, mp4->size(), decodeOptions, "roundtrip.mp4");
  ASSERT_NE(video, nullptr);
  const SkColor first = centerColor(video->frameAt(0.02).image);
  const SkColor third = centerColor(video->frameAt(0.22).image);
  const SkColor firstAgain = centerColor(video->frameAt(0.02).image);
  EXPECT_TRUE(nearChannel(SkColorGetR(first), 255)) << SkColorGetR(first);
  EXPECT_TRUE(nearChannel(SkColorGetG(first), 0)) << SkColorGetG(first);
  EXPECT_TRUE(nearChannel(SkColorGetB(first), 0)) << SkColorGetB(first);
  EXPECT_TRUE(nearChannel(SkColorGetB(third), 255)) << SkColorGetB(third);
  EXPECT_TRUE(nearChannel(SkColorGetR(third), 0)) << SkColorGetR(third);
  EXPECT_TRUE(nearChannel(SkColorGetG(third), 0)) << SkColorGetG(third);
  EXPECT_TRUE(nearChannel(SkColorGetR(firstAgain), 255));
}

TEST(VideoEncode, CpuDecodeReadsBackTheColourItWasGiven) {
  // A mid-saturation colour separates the BT.709 matrix the stream is
  // tagged with from the BT.601 default a converter falls back to: the
  // two disagree by tens of counts on green and blue here.
  constexpr int kWidth = 64;
  constexpr int kHeight = 64;
  const SkColor given = SkColorSetRGB(200, 90, 40);
  auto encoder = sigil::video::Encoder::make(
      sigil::video::Format::Mp4,
      {.width = kWidth,
       .height = kHeight,
       .framesPerSecond = 10,
       .bitRate = 1'000'000,
       .hardware = sigil::video::HardwarePreference::Disabled});
  ASSERT_NE(encoder, nullptr);
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(kWidth, kHeight));
  bitmap.eraseColor(given);
  ASSERT_TRUE(encoder->append(bitmap.pixmap())) << encoder->error();
  ASSERT_TRUE(encoder->append(bitmap.pixmap())) << encoder->error();
  sk_sp<SkData> mp4 = encoder->finish();
  ASSERT_NE(mp4, nullptr) << encoder->error();

  sigil::video::DecodeOptions decodeOptions;
  decodeOptions.hardware = sigil::video::HardwarePreference::Disabled;
  std::shared_ptr<sigil::video::Video> video = sigil::video::decodeVideo(
      static_cast<const std::byte*>(mp4->data()), mp4->size(), decodeOptions,
      "colour.mp4");
  ASSERT_NE(video, nullptr);
  const SkColor read = centerColor(video->frameAt(0.0).image);
  EXPECT_TRUE(nearChannel(SkColorGetR(read), SkColorGetR(given)))
      << SkColorGetR(read);
  EXPECT_TRUE(nearChannel(SkColorGetG(read), SkColorGetG(given)))
      << SkColorGetG(read);
  EXPECT_TRUE(nearChannel(SkColorGetB(read), SkColorGetB(given)))
      << SkColorGetB(read);
}

TEST(VideoEncode, RejectsOddDimensions) {
  EXPECT_EQ(sigil::video::Encoder::make(
                sigil::video::Format::Mp4,
                {.width = 63, .height = 64, .framesPerSecond = 30}),
            nullptr);
}

TEST(VideoEncode, RecognizesContainerExtensions) {
  EXPECT_EQ(sigil::video::formatForPath("clip.MP4"), sigil::video::Format::Mp4);
  EXPECT_EQ(sigil::video::extensionFor(sigil::video::Format::Mp4),
            std::string(".mp4"));
  EXPECT_FALSE(sigil::video::formatForPath("clip.png"));
}
