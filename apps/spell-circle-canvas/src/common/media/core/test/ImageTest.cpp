/** @file
 * The image document and the clock it is read against: a picture wrapped
 * as a one-frame document, frames placed from their durations, and the
 * frame a playback time answers under each timing — looping as encoded,
 * forever, once, offset and scaled.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <sigilmedia/core/Image.h>

#include <chrono>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;
using sigil::media::Frame;
using sigil::media::Image;
using sigil::media::Loop;
using sigil::media::Timing;

sk_sp<SkImage> solid(SkColor color) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(4, 4));
  bitmap.eraseColor(color);
  bitmap.setImmutable();
  return bitmap.asImage();
}

/** Red, green and blue for a tenth of a second each, played
 *  @p repetitions times. */
Image threeFrames(int repetitions = -1) {
  std::vector<Frame> frames(3);
  const SkColor colors[] = {SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE};
  for (size_t index = 0; index < frames.size(); ++index) {
    frames[index].image = solid(colors[index]);
    frames[index].duration = 100ms;
  }
  return Image(std::move(frames), repetitions);
}

TEST(MediaImage, APictureIsAOneFrameDocument) {
  const sk_sp<SkImage> picture = solid(SK_ColorRED);
  const auto image = Image::of(picture);
  ASSERT_TRUE(image);
  EXPECT_EQ(image->size(), SkISize::Make(4, 4));
  EXPECT_FALSE(image->isRunning());
  EXPECT_EQ(image->duration(), 0s);
  EXPECT_EQ(image->frameAt(0s).image, picture);
  EXPECT_EQ(image->frameAt(12s).image, picture);  // a still stands at every time

  const auto empty = Image::of(nullptr);
  ASSERT_TRUE(empty);
  EXPECT_TRUE(empty->frames().empty());
  EXPECT_FALSE(empty->frameAt(0s));
}

TEST(MediaImage, FramesArePlacedFromTheirDurations) {
  const Image image = threeFrames();
  EXPECT_TRUE(image.isRunning());
  EXPECT_DOUBLE_EQ(image.duration().count(), 0.3);
  ASSERT_EQ(image.frames().size(), 3u);
  EXPECT_DOUBLE_EQ(image.frames()[1].time.count(), 0.1);
  EXPECT_EQ(image.frames()[2].index, 2);
}

TEST(MediaImage, AnAnimationLoopsAsItsFileSays) {
  const Image image = threeFrames();
  const auto& frames = image.frames();
  EXPECT_EQ(image.frameAt(0ms).image, frames[0].image);
  EXPECT_EQ(image.frameAt(99ms).image, frames[0].image);
  EXPECT_EQ(image.frameAt(150ms).image, frames[1].image);
  EXPECT_EQ(image.frameAt(250ms).image, frames[2].image);
  EXPECT_EQ(image.frameAt(310ms).image, frames[0].image);  // looped
  EXPECT_EQ(image.frameAt(-5ms).image, frames[0].image);   // held at the start
}

TEST(MediaImage, AFiniteAnimationHoldsItsLastFrame) {
  const Image twice = threeFrames(2);
  const auto& frames = twice.frames();
  EXPECT_EQ(twice.frameAt(350ms).image, frames[0].image);  // second play
  EXPECT_EQ(twice.frameAt(650ms).image, frames[2].image);  // over: held
  EXPECT_EQ(twice.frameAt(30s).image, frames[2].image);
}

TEST(MediaImage, TimingOverridesWhatTheFileSays) {
  const Image twice = threeFrames(2);
  const auto& frames = twice.frames();
  EXPECT_EQ(twice.frameAt(650ms, {.loop = Loop::Forever}).image,
            frames[0].image);
  EXPECT_EQ(twice.frameAt(350ms, {.loop = Loop::Once}).image, frames[2].image);
  EXPECT_EQ(twice.frameAt(0ms, {.start = 150ms}).image, frames[1].image);
  EXPECT_EQ(twice.frameAt(100ms, {.rate = 2.0}).image, frames[2].image);
  EXPECT_EQ(twice.frameAt(-50ms, {.loop = Loop::Forever}).image,
            frames[2].image);  // a loop in both directions
}

TEST(MediaImage, ADocumentWithNoLengthAnswersTheElapsedTime) {
  const Timing timing;
  EXPECT_EQ(timing.documentTime(2.5s, 0s), 2.5s);
  EXPECT_EQ(timing.documentTime(-1s, 0s), 0s);
}

}  // namespace
