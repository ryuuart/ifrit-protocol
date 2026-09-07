/** @file
 * The probe a sketch over fetched art answers its availability with:
 * what stands in the IO hub's cache, and what does not.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>
#include <sigilio/hub/Network.h>
#include <sigilio/source/Sink.h>
#include <sigilsketch/core/Assets.h>
#include <sigilvideo/encode/Encode.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "ScratchDir.h"

namespace {

using namespace sigil::sketch;

sk_sp<SkData> solidVideo(SkColor color) {
  constexpr int kWidth = 64;
  constexpr int kHeight = 48;
  auto encoder = sigil::video::Encoder::make(
      sigil::video::Format::Mp4,
      {.width = kWidth,
       .height = kHeight,
       .framesPerSecond = 10,
       .bitRate = 300'000,
       .hardware = sigil::video::HardwarePreference::Disabled});
  if (!encoder) return nullptr;
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(kWidth, kHeight));
  bitmap.eraseColor(color);
  if (!encoder->append(bitmap.pixmap())) return nullptr;
  return encoder->finish();
}

TEST(RequireCached, AnswersFromTheCacheAndNamesTheFirstMissingUrl) {
  sigil::test::ScratchDir cache("sketch_require_cached");
  const char* fetched = "https://sketch.invalid/art/panel.gif";
  const char* missing = "https://sketch.invalid/art/logo.svg";
  // A fetch that happened is a file under the URL's own cache key, and
  // nothing else about it is consulted.
  std::ofstream(cache.path / sigil::io::networkCacheKey(fetched),
                std::ios::binary)
      << "gif";

  std::string why;
  EXPECT_TRUE(requireCached({fetched}, &why, cache.path));
  EXPECT_TRUE(why.empty());

  EXPECT_FALSE(requireCached({fetched, missing}, &why, cache.path));
  EXPECT_NE(why.find(missing), std::string::npos);
  EXPECT_EQ(why.find(fetched), std::string::npos);

  // Nothing asked for is nothing missing, and a reason nobody wants is
  // not written anywhere.
  EXPECT_TRUE(requireCached({}, nullptr, cache.path));
  EXPECT_FALSE(requireCached({missing}, nullptr, cache.path));

  // A FILE THAT IS THERE AND HOLDS NOTHING is not the art: a fetch that
  // was interrupted leaves one, and a sketch handed it draws its
  // stand-in exactly as if nothing were cached at all.
  std::ofstream(cache.path / sigil::io::networkCacheKey(missing),
                std::ios::binary);
  EXPECT_FALSE(requireCached({missing}, &why, cache.path));
  EXPECT_NE(why.find(missing), std::string::npos);
}

TEST(RequireCached, AsksTheDirectoryTheIoHubPersistsFetchesTo) {
  // With no directory named, the probe asks where a hub given none of
  // its own puts them — the two cannot drift, because there is one
  // answer and both read it.
  const char* url = "https://sketch.invalid/art/nothing-has-fetched-this.png";
  std::error_code ec;
  std::filesystem::remove(
      sigil::io::defaultNetworkCacheDir() / sigil::io::networkCacheKey(url),
      ec);
  std::string why;
  EXPECT_FALSE(requireCached({url}, &why));
  EXPECT_NE(why.find(url), std::string::npos);
}

TEST(Assets, VideoUsesTheClipCacheAndInvalidatesAfterSourceChange) {
  sigil::test::ScratchDir root("sketch_video_asset");
  const std::filesystem::path path = root.path / "clip.mp4";
  const sk_sp<SkData> firstBytes = solidVideo(SK_ColorRED);
  ASSERT_NE(firstBytes, nullptr);
  ASSERT_TRUE(
      sigil::io::writeBytes(path, firstBytes->data(), firstBytes->size()));

  Assets assets(root.path);
  const sigil::video::DecodeOptions options{
      .hardware = sigil::video::HardwarePreference::Disabled,
      .cachedFrames = 2};
  const std::shared_ptr<sigil::video::Video> first =
      assets.video("clip.mp4", options);
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(assets.video("clip.mp4", options), first);

  const sk_sp<SkData> secondBytes = solidVideo(SK_ColorBLUE);
  ASSERT_NE(secondBytes, nullptr);
  ASSERT_TRUE(
      sigil::io::writeBytes(path, secondBytes->data(), secondBytes->size()));
  std::error_code ec;
  std::filesystem::last_write_time(
      path,
      std::filesystem::last_write_time(path, ec) +
          std::filesystem::file_time_type::duration(1),
      ec);
  ASSERT_FALSE(ec);
  ASSERT_TRUE(assets.poll());

  const std::shared_ptr<sigil::video::Video> second =
      assets.video("clip.mp4", options);
  ASSERT_NE(second, nullptr);
  EXPECT_NE(second, first);
}

}  // namespace
