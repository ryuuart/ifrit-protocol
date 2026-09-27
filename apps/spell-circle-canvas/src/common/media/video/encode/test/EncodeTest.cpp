#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/advanced/Formats.h>
#include <sigilmedia/video/Encoder.h>
#include <sigilmedia/video/Video.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <memory>
#include <vector>

namespace {

namespace media = sigil::media;
using namespace std::chrono_literals;

/** The movie opened on the CPU, keeping @p cachedFrames around the
 *  playhead. */
std::shared_ptr<const media::Video> openVideo(
    const std::vector<std::byte>& bytes, const char* name,
    size_t cachedFrames = 4) {
  return media::decode<media::Video>(
      bytes,
      {.cachedFrames = cachedFrames,
       .hardware = media::HardwarePreference::Disabled},
      name);
}

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
  media::Encoder encoder({.width = kWidth,
       .height = kHeight,
       .framesPerSecond = kFps,
       .bitRate = 1'000'000,
       .hardware = media::HardwarePreference::Disabled});
  ASSERT_TRUE(encoder) << encoder.error();

  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(kWidth, kHeight));
  constexpr std::array<SkColor, 4> colors = {SK_ColorRED, SK_ColorGREEN,
                                             SK_ColorBLUE, SK_ColorWHITE};
  for (SkColor color : colors) {
    bitmap.eraseColor(color);
    ASSERT_TRUE(append(encoder, bitmap.pixmap())) << encoder.error();
  }
  EXPECT_EQ(encoder.frameCount(), 4);
  const std::vector<std::byte> mp4 = encoder.finish();
  ASSERT_FALSE(mp4.empty()) << encoder.error();
  EXPECT_GT(mp4.size(), 100u);

  const std::optional<media::Metadata> probe =
      probeDocument(std::type_identity<media::Video>{}, mp4, "roundtrip.mp4");
  ASSERT_TRUE(probe);
  EXPECT_EQ(probe->width, kWidth);
  EXPECT_EQ(probe->height, kHeight);
  EXPECT_NEAR(probe->frameRate, kFps, 0.1);
  EXPECT_GE(probe->duration.count(), 0.39);

  const auto video = openVideo(mp4, "roundtrip.mp4", 2);
  ASSERT_NE(video, nullptr);
  const SkColor first = centerColor(video->frameAt(20ms).image);
  const SkColor third = centerColor(video->frameAt(220ms).image);
  const SkColor firstAgain = centerColor(video->frameAt(20ms).image);
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
  media::Encoder encoder({.width = kWidth,
       .height = kHeight,
       .framesPerSecond = 10,
       .bitRate = 1'000'000,
       .hardware = media::HardwarePreference::Disabled});
  ASSERT_TRUE(encoder) << encoder.error();
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(kWidth, kHeight));
  bitmap.eraseColor(given);
  ASSERT_TRUE(append(encoder, bitmap.pixmap())) << encoder.error();
  ASSERT_TRUE(append(encoder, bitmap.pixmap())) << encoder.error();
  const std::vector<std::byte> mp4 = encoder.finish();
  ASSERT_FALSE(mp4.empty()) << encoder.error();

  const auto video = openVideo(mp4, "colour.mp4");
  ASSERT_NE(video, nullptr);
  const SkColor read = centerColor(video->frameAt(0s).image);
  EXPECT_TRUE(nearChannel(SkColorGetR(read), SkColorGetR(given)))
      << SkColorGetR(read);
  EXPECT_TRUE(nearChannel(SkColorGetG(read), SkColorGetG(given)))
      << SkColorGetG(read);
  EXPECT_TRUE(nearChannel(SkColorGetB(read), SkColorGetB(given)))
      << SkColorGetB(read);
}

TEST(VideoEncode, FinishingIsTerminal) {
  constexpr int kWidth = 64;
  constexpr int kHeight = 64;
  media::Encoder encoder({.width = kWidth,
       .height = kHeight,
       .framesPerSecond = 10,
       .bitRate = 1'000'000,
       .hardware = media::HardwarePreference::Disabled});
  ASSERT_TRUE(encoder) << encoder.error();
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(kWidth, kHeight));
  bitmap.eraseColor(SK_ColorGREEN);
  ASSERT_TRUE(append(encoder, bitmap.pixmap())) << encoder.error();
  ASSERT_FALSE(encoder.finish().empty()) << encoder.error();
  EXPECT_EQ(encoder.frameCount(), 1);

  // The muxed bytes are already handed out, so neither another frame nor
  // another trailer can join them.
  EXPECT_FALSE(append(encoder, bitmap.pixmap()));
  EXPECT_FALSE(encoder.error().empty());
  EXPECT_EQ(encoder.frameCount(), 1);
  EXPECT_TRUE(encoder.finish().empty());
  EXPECT_FALSE(encoder.error().empty());
}

TEST(VideoEncode, RefusesToFinishWithNoFrames) {
  media::Encoder encoder({.width = 64,
       .height = 64,
       .framesPerSecond = 10,
       .bitRate = 1'000'000,
       .hardware = media::HardwarePreference::Disabled});
  ASSERT_TRUE(encoder) << encoder.error();
  EXPECT_TRUE(encoder.finish().empty());
  EXPECT_FALSE(encoder.error().empty());
  EXPECT_EQ(encoder.frameCount(), 0);

  // A refused finish is still a finish: the encoder is closed either way.
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(64, 64));
  bitmap.eraseColor(SK_ColorGREEN);
  EXPECT_FALSE(append(encoder, bitmap.pixmap()));
  EXPECT_EQ(encoder.frameCount(), 0);
}

TEST(VideoEncode, RejectsOddDimensions) {
  media::Encoder encoder({.width = 63, .height = 64, .framesPerSecond = 30});
  EXPECT_FALSE(encoder);
  EXPECT_FALSE(encoder.error().empty());
  // An encoder that never opened takes no frame and finishes as nothing.
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(64, 64));
  EXPECT_FALSE(append(encoder, bitmap.pixmap()));
  EXPECT_TRUE(encoder.finish().empty());
}

TEST(VideoEncode, WritesMp4Only) {
  EXPECT_FALSE(media::Encoder({.width = 64, .height = 64,
                               .format = media::Format::Png}));
}

TEST(VideoEncode, RecognizesContainerExtensions) {
  EXPECT_EQ(media::formatForPath("clip.MP4"), media::Format::Mp4);
  EXPECT_EQ(media::extensionFor(media::Format::Mp4), std::string(".mp4"));
  EXPECT_NE(media::formatForPath("clip.png"), media::Format::Mp4);
}
