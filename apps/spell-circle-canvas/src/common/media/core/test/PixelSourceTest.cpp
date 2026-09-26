/** @file
 * The one seam pixels cross: a picture, a document under a timing, a
 * producer baked once, and a source written elsewhere — each read through
 * one frameAt(), each compared by value.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <sigilmedia/core/Image.h>
#include <sigilmedia/core/PixelSource.h>

#include <chrono>
#include <cstdint>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;
using sigil::media::Frame;
using sigil::media::Image;
using sigil::media::Loop;
using sigil::media::PixelSource;
using sigil::media::Produced;

sk_sp<SkImage> solid(SkColor color, int size = 4) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(size, size));
  bitmap.eraseColor(color);
  bitmap.setImmutable();
  return bitmap.asImage();
}

std::shared_ptr<const Image> redThenGreen() {
  std::vector<Frame> frames(2);
  frames[0].image = solid(SK_ColorRED);
  frames[0].duration = 100ms;
  frames[1].image = solid(SK_ColorGREEN);
  frames[1].duration = 100ms;
  return std::make_shared<const Image>(std::move(frames));
}

/** A source written outside this library: it counts the frames that
 *  stood, as a publication does. */
struct Counting {
  sk_sp<SkImage> picture;
  uint64_t count = 0;
  Frame frameAt(std::chrono::duration<double>) const {
    Frame frame;
    frame.image = picture;
    return frame;
  }
  bool isRunning() const { return true; }
  uint64_t revision() const { return count; }
  bool operator==(const Counting& other) const {
    return picture.get() == other.picture.get() && count == other.count;
  }
};

TEST(MediaPixelSource, APictureIsTheSameAtEveryTime) {
  const sk_sp<SkImage> picture = solid(SK_ColorBLUE);
  const PixelSource source(picture);
  ASSERT_TRUE(source);
  EXPECT_EQ(source.frameAt(0s).image, picture);
  EXPECT_EQ(source.frameAt(9s).image, picture);
  EXPECT_FALSE(source.isRunning());
  EXPECT_EQ(source.size(), SkISize::Make(4, 4));
  EXPECT_EQ(source, PixelSource(picture));
  EXPECT_FALSE(source == PixelSource(solid(SK_ColorBLUE)));
  EXPECT_FALSE(PixelSource());
  EXPECT_FALSE(PixelSource().frameAt(0s));
}

TEST(MediaPixelSource, ADocumentIsReadUnderItsTiming) {
  const auto document = redThenGreen();
  const PixelSource source(document);
  EXPECT_TRUE(source.isRunning());
  EXPECT_EQ(source.size(), SkISize::Make(4, 4));
  EXPECT_EQ(source.frameAt(50ms).image, document->frames()[0].image);
  EXPECT_EQ(source.frameAt(150ms).image, document->frames()[1].image);
  const PixelSource late(document, {.start = 100ms});
  EXPECT_EQ(late.frameAt(50ms).image, document->frames()[1].image);
  // One document under two timings is two sources.
  EXPECT_EQ(source, PixelSource(document));
  EXPECT_FALSE(source == late);
  EXPECT_FALSE(source == PixelSource(document, {.loop = Loop::Once}));
}

TEST(MediaPixelSource, AProducerBakesOnceUnderItsKey) {
  int calls = 0;
  const PixelSource source = PixelSource::produce("red", [&calls] {
    ++calls;
    return solid(SK_ColorRED);
  });
  ASSERT_TRUE(source.frameAt(0s).image);
  ASSERT_TRUE(source.frameAt(1s).image);
  EXPECT_EQ(calls, 1);
  ASSERT_NE(source.as<Produced>(), nullptr);
  EXPECT_EQ(source.as<Produced>()->key(), "red");
  // The key is the identity.
  EXPECT_EQ(source, PixelSource::produce("red", [] { return nullptr; }));
  EXPECT_FALSE(source == PixelSource(solid(SK_ColorRED)));
}

TEST(MediaPixelSource, ASourceWrittenElsewhereCarriesItsRevision) {
  const sk_sp<SkImage> picture = solid(SK_ColorWHITE);
  const PixelSource first(Counting{picture, 1});
  const PixelSource second(Counting{picture, 2});
  EXPECT_EQ(first.revision(), 1u);
  EXPECT_TRUE(first.isRunning());
  EXPECT_EQ(first.size(), SkISize::Make(4, 4));
  EXPECT_FALSE(first == second);
  EXPECT_EQ(first, PixelSource(Counting{picture, 1}));
  EXPECT_EQ(PixelSource(picture).revision(), 0u);
}

}  // namespace
