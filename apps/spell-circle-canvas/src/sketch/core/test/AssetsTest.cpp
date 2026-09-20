/** @file
 * The probe a sketch over fetched art answers its availability with:
 * what stands in the IO hub's cache, and what does not.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>
#include <sigildata/decode/Json.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/hub/Network.h>
#include <sigilio/hub/Recording.h>
#include <sigilio/source/Sink.h>
#include <sigilsketch/core/Assets.h>
#include <sigilvideo/encode/Encode.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "ScratchDir.h"

namespace {

using namespace sigil::sketch;

/** One recorded message carrying @p text, for a case that says WHICH
 *  arrival a frame was handed rather than how many bytes it was. */
std::shared_ptr<const sigil::io::Bytes> recorded(std::string_view text) {
  const auto* first = reinterpret_cast<const std::byte*>(text.data());
  sigil::io::Bytes bytes;
  bytes.bytes.assign(first, first + text.size());
  return std::make_shared<const sigil::io::Bytes>(std::move(bytes));
}

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
  const std::string body = "gif";
  ASSERT_TRUE(sigil::io::seedNetworkCache(
      fetched, std::as_bytes(std::span(body.data(), body.size())), cache.path));

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

  // Empty bytes cannot supply the sketch's art.
  ASSERT_TRUE(sigil::io::seedNetworkCache(missing, {}, cache.path));
  EXPECT_FALSE(requireCached({missing}, &why, cache.path));
  EXPECT_NE(why.find(missing), std::string::npos);
}

TEST(RequireCached, AnswersOverAListDecidedWhileItRuns) {
  // The general form: a host asking on behalf of a sketch written in
  // another language holds its URLs in a vector, not in a literal, and
  // must reach the same probe with it.
  sigil::test::ScratchDir cache("sketch_require_cached_span");
  const std::string fetched = "https://sketch.invalid/art/sheet.png";
  const std::string missing = "https://sketch.invalid/art/mask.png";
  const std::string body = "png";
  ASSERT_TRUE(sigil::io::seedNetworkCache(
      fetched, std::as_bytes(std::span(body.data(), body.size())), cache.path));

  std::vector<std::string_view> urls{fetched};
  std::string why;
  EXPECT_TRUE(requireCached(urls, &why, cache.path));
  EXPECT_TRUE(why.empty());

  urls.push_back(missing);
  EXPECT_FALSE(requireCached(urls, &why, cache.path));
  EXPECT_NE(why.find(missing), std::string::npos);

  // An empty run of URLs asks for nothing and is missing nothing,
  // whichever form it came in.
  EXPECT_TRUE(
      requireCached(std::span<const std::string_view>{}, &why, cache.path));
}

TEST(RequireCached, AConfiguredCacheIsNotTheDefaultCache) {
  sigil::test::ScratchDir cache("sketch_require_cached_separate");
  const std::string url = "https://sketch.invalid/art/panel.png?fixture=" +
                          cache.path.filename().string();
  const std::string body = "png";
  ASSERT_TRUE(sigil::io::seedNetworkCache(
      url, std::as_bytes(std::span(body.data(), body.size())), cache.path));
  EXPECT_TRUE(requireCached({url}, nullptr, cache.path));
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

/** A recording standing among a sketch's own files, mounted onto the URI
 *  the sketch listens on: the store answers a feed that plays the file
 *  back, and what a dispatch delivers is everything recorded at or
 *  before the scene time it is handed. This is what a capture does with
 *  the port a window binds. */
TEST(Assets, AMountedRecordingIsAFeedThatReplaysByTheSceneTimeDispatched) {
  sigil::test::ScratchDir dir("sketch_assets_feed");
  const std::filesystem::path recording = dir.path / "data" / "sky.feed";
  std::filesystem::create_directories(recording.parent_path());
  {
    sigil::io::RecordingWriter writer(recording);
    ASSERT_TRUE(writer.good());
    ASSERT_TRUE(writer.append({1, 0.0, recorded("dawn")}));
    ASSERT_TRUE(writer.append({2, 0.5, recorded("noon")}));
    ASSERT_TRUE(writer.append({3, 1.0, recorded("dusk")}));
  }

  Assets assets("");
  assets.mountSketch("sky", dir.path);
  sigil::io::Hub& hub = assets.hub();
  // The two lines a sketch writes while a capture is being taken: the
  // port is mounted onto the file, and the ask for the port opens the
  // recording rather than a socket.
  hub.mount("udp://:27020", hub.resolve("sketch://sky/data/sky.feed"));
  const std::shared_ptr<sigil::io::Feed> feed = hub.feed("udp://:27020");
  ASSERT_NE(feed, nullptr);
  EXPECT_TRUE(feed->error().empty());
  EXPECT_EQ(feed->generation(), 0u);  // nothing arrives until time moves

  assets.dispatch(0.0);
  EXPECT_EQ(feed->generation(), 1u);
  ASSERT_NE(feed->latest(), nullptr);
  EXPECT_EQ(feed->latest()->asText(), "dawn");
  EXPECT_FALSE(feed->closed());

  // One dispatch may cover several arrivals and never covers one that is
  // still ahead: the scene time decides, not the number of calls.
  assets.dispatch(0.6);
  EXPECT_EQ(feed->generation(), 2u);
  EXPECT_EQ(feed->latest()->asText(), "noon");
  EXPECT_FALSE(feed->closed());

  assets.dispatch(2.0);
  EXPECT_EQ(feed->generation(), 3u);
  EXPECT_EQ(feed->latest()->asText(), "dusk");
  EXPECT_TRUE(feed->closed());  // the recording ran out
}

}  // namespace

/** A sketch's words in a file beside it: the document is read whole,
 *  a missing key is a null value rather than a failure, and an edit to
 *  the file is what poll() reports, so the sketch re-describes from the
 *  new words without a rebuild. */
TEST(Assets, ADocumentIsReadWholeAndReloadedWhenItsFileChanges) {
  sigil::test::ScratchDir dir("sketch_assets_json");
  std::filesystem::create_directories(dir.path / "data");
  const auto write = [&](const char* body) {
    std::ofstream(dir.path / "data" / "content.json", std::ios::binary) << body;
  };
  write(R"({"title": "A2", "lines": [{"code": "x1 -= d;", "marked": true}]})");
  Assets assets("");
  assets.mountSketch("study", dir.path);
  const auto document = assets.json("sketch://study/data/content.json");
  ASSERT_NE(document, nullptr);
  EXPECT_EQ((*document)["title"].text(), "A2");
  EXPECT_TRUE((*document)["lines"][0]["marked"].boolean());
  EXPECT_TRUE((*document)["absent"]["deeper"].null());
  EXPECT_EQ((*document)["absent"].text("fallback"), "fallback");
  EXPECT_EQ(assets.json("sketch://study/data/missing.json"), nullptr);

  write(R"({"title": "A3"})");
  std::filesystem::last_write_time(
      dir.path / "data" / "content.json",
      std::filesystem::file_time_type::clock::now() + std::chrono::seconds(2));
  EXPECT_TRUE(assets.poll());
  EXPECT_EQ((*assets.json("sketch://study/data/content.json"))["title"].text(),
            "A3");
}
