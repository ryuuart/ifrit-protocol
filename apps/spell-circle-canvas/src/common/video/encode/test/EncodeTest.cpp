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

bool nearChannel(int actual, int expected) {
  return std::abs(actual - expected) < 32;
}

}  // namespace

TEST(VideoEncode, Mp4RoundTripsFramesAndTiming) {
  constexpr int kWidth = 96;
  constexpr int kHeight = 64;
  constexpr int kFps = 10;
  auto encoder = sigil::video::Encoder::make(sigil::video::Format::Mp4,
                                             {.width = kWidth,
                                              .height = kHeight,
                                              .framesPerSecond = kFps,
                                              .bitRate = 1'000'000});
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
  EXPECT_TRUE(nearChannel(SkColorGetR(first), 255));
  EXPECT_TRUE(nearChannel(SkColorGetG(first), 0));
  EXPECT_TRUE(nearChannel(SkColorGetB(third), 255));
  EXPECT_TRUE(nearChannel(SkColorGetR(firstAgain), 255));
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
