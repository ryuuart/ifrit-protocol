/** @file
 * What a sketch's files are through the host's assets: the probe a sketch
 * over fetched art answers its availability with, and the pictures,
 * clips, documents and recordings the hub loads and watches.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>
#include <sigildata/decode/Json.h>
#include <sigilio/advanced/Network.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/hub/Network.h>
#include <sigilio/hub/Recording.h>
#include <sigilio/source/Sink.h>
#include <sigilio/advanced/Time.h>
#include <sigilsketch/core/Assets.h>
#include <sigilmedia/core/Image.h>
#include <sigilmedia/image/Encode.h>
#include <sigilmedia/video/Encoder.h>
#include <sigilmedia/video/Video.h>

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
  return std::make_shared<const sigil::io::Bytes>(
      std::as_bytes(std::span(text)));
}

std::vector<std::byte> solidVideo(SkColor color) {
  constexpr int kWidth = 64;
  constexpr int kHeight = 48;
  sigil::media::Encoder encoder(
      {.width = kWidth,
       .height = kHeight,
       .framesPerSecond = 10,
       .bitRate = 300'000,
       .hardware = sigil::media::HardwarePreference::Disabled});
  if (!encoder) return {};
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(kWidth, kHeight));
  bitmap.eraseColor(color);
  if (!encoder.append(bitmap.pixmap())) return {};
  return encoder.finish();
}

TEST(RequireCached, AnswersFromTheCacheAndNamesTheFirstMissingUrl) {
  sigil::test::ScratchDir cache("sketch_require_cached");
  const char* fetched = "https://sketch.invalid/art/panel.gif";
  const char* missing = "https://sketch.invalid/art/logo.svg";
  const std::string body = "gif";
  ASSERT_TRUE(sigil::io::NetworkCache(cache.path).put(fetched, std::as_bytes(std::span(body.data(), body.size()))));

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
  ASSERT_TRUE(sigil::io::NetworkCache(cache.path).put(missing, {}));
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
  ASSERT_TRUE(sigil::io::NetworkCache(cache.path).put(fetched, std::as_bytes(std::span(body.data(), body.size()))));

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
  ASSERT_TRUE(sigil::io::NetworkCache(cache.path).put(url, std::as_bytes(std::span(body.data(), body.size()))));
  EXPECT_TRUE(requireCached({url}, nullptr, cache.path));
  std::string why;
  EXPECT_FALSE(requireCached({url}, &why));
  EXPECT_NE(why.find(url), std::string::npos);
}

TEST(Assets, AVideoIsCachedByTheHubAndReopenedAfterItsFileChanges) {
  sigil::test::ScratchDir root("sketch_video_asset");
  const std::filesystem::path path = root.path / "clip.mp4";
  const std::vector<std::byte> firstBytes = solidVideo(SK_ColorRED);
  ASSERT_FALSE(firstBytes.empty());
  ASSERT_TRUE(
      sigil::io::writeBytes(path, firstBytes.data(), firstBytes.size()));

  Assets assets(root.path);
  const sigil::media::VideoOptions options{
      .cachedFrames = 2,
      .hardware = sigil::media::HardwarePreference::Disabled};
  const std::shared_ptr<const sigil::media::Video> first =
      assets.hub().load<sigil::media::Video>("res://clip.mp4", options);
  ASSERT_TRUE(first);
  EXPECT_EQ(assets.hub().load<sigil::media::Video>("res://clip.mp4", options),
            first);

  const std::vector<std::byte> secondBytes = solidVideo(SK_ColorBLUE);
  ASSERT_FALSE(secondBytes.empty());
  ASSERT_TRUE(
      sigil::io::writeBytes(path, secondBytes.data(), secondBytes.size()));
  std::error_code ec;
  std::filesystem::last_write_time(
      path,
      std::filesystem::last_write_time(path, ec) +
          std::filesystem::file_time_type::duration(1),
      ec);
  ASSERT_FALSE(ec);
  ASSERT_TRUE(assets.poll());

  const std::shared_ptr<const sigil::media::Video> second =
      assets.hub().load<sigil::media::Video>("res://clip.mp4", options);
  ASSERT_TRUE(second);
  EXPECT_NE(second, first);
}

/** A picture a sketch asks for before its file is there: nothing, then a
 *  poll that says the file appeared — the host's cue to set the sketch up
 *  again — and the picture. */
TEST(Assets, APictureAskedForBeforeItsFileIsThereIsAChangeWhenItAppears) {
  sigil::test::ScratchDir root("sketch_image_asset");
  Assets assets(root.path);
  EXPECT_EQ(assets.hub().load<sigil::media::Image>("res://mark.png"), nullptr);
  EXPECT_FALSE(assets.poll());

  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(12, 7));
  bitmap.eraseColor(SK_ColorGREEN);
  const std::vector<std::byte> png =
      sigil::media::encode(bitmap.pixmap(), sigil::media::Format::Png);
  ASSERT_FALSE(png.empty());
  ASSERT_TRUE(sigil::io::writeBytes(root.path / "mark.png", png.data(),
                                    png.size()));
  EXPECT_TRUE(assets.poll());
  const auto image = assets.hub().load<sigil::media::Image>("res://mark.png");
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->size().width(), 12);
  EXPECT_EQ(image->size().height(), 7);
}

/** A recording standing among a sketch's own files, replayed at the URI
 *  the sketch listens on: the store answers a feed that plays the file
 *  back, and what a dispatch delivers is everything recorded at or
 *  before the scene time it is handed. This is what a capture does with
 *  the port a window binds. */
TEST(Assets, AReplayedRecordingIsAFeedThatPlaysByTheSceneTimeDispatched) {
  sigil::test::ScratchDir dir("sketch_assets_feed");
  const std::filesystem::path recording = dir.path / "data" / "sky.feed";
  std::filesystem::create_directories(recording.parent_path());
  {
    sigil::io::RecordingWriter writer(recording);
    ASSERT_TRUE(writer.good());
    ASSERT_TRUE(writer.append(sigil::io::Message(recorded("dawn"), {}, std::chrono::duration<double>(0.0), 1)));
    ASSERT_TRUE(writer.append(sigil::io::Message(recorded("noon"), {}, std::chrono::duration<double>(0.5), 2)));
    ASSERT_TRUE(writer.append(sigil::io::Message(recorded("dusk"), {}, std::chrono::duration<double>(1.0), 3)));
  }

  Assets assets("");
  assets.mountSketch("sky", dir.path);
  sigil::io::Hub& hub = assets.hub();
  // The two lines a sketch writes while a capture is being taken: the
  // port is replayed from the file, and the ask for the port opens the
  // recording rather than a socket.
  hub.replay("udp://:27020", "sketch://sky/data/sky.feed");
  const sigil::io::Feed feed = hub.listen("udp://:27020");
  ASSERT_TRUE(feed);
  EXPECT_TRUE(feed.state().error.empty());
  EXPECT_EQ(feed.state().revision, 0u);  // nothing arrives until time moves

  sigil::io::advance(assets.hub(), std::chrono::duration<double>(0.0));
  EXPECT_EQ(feed.state().revision, 1u);
  ASSERT_TRUE(feed.latest().has_value());
  EXPECT_EQ(feed.latest()->payload->asText(), "dawn");
  EXPECT_NE(feed.state().readiness, sigil::io::ReadyState::Closed);

  // One dispatch may cover several arrivals and never covers one that is
  // still ahead: the scene time decides, not the number of calls.
  sigil::io::advance(assets.hub(), std::chrono::duration<double>(0.6));
  EXPECT_EQ(feed.state().revision, 2u);
  EXPECT_EQ(feed.latest()->payload->asText(), "noon");
  EXPECT_NE(feed.state().readiness, sigil::io::ReadyState::Closed);

  sigil::io::advance(assets.hub(), std::chrono::duration<double>(2.0));
  EXPECT_EQ(feed.state().revision, 3u);
  EXPECT_EQ(feed.latest()->payload->asText(), "dusk");
  EXPECT_EQ(feed.state().readiness, sigil::io::ReadyState::Closed);  // the recording ran out
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
  ASSERT_TRUE(document);
  EXPECT_EQ((*document)["title"].string(), "A2");
  EXPECT_TRUE((*document)["lines"][0]["marked"].boolean());
  EXPECT_TRUE((*document)["absent"]["deeper"].null());
  EXPECT_EQ((*document)["absent"].string("fallback"), "fallback");
  EXPECT_EQ(assets.json("sketch://study/data/missing.json"), nullptr);

  write(R"({"title": "A3"})");
  std::filesystem::last_write_time(
      dir.path / "data" / "content.json",
      std::filesystem::file_time_type::clock::now() + std::chrono::seconds(2));
  EXPECT_TRUE(assets.poll());
  EXPECT_EQ((*assets.json("sketch://study/data/content.json"))["title"].string(),
            "A3");
}
