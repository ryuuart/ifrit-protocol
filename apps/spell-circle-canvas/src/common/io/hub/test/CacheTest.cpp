/** @file
 * The cache and what lives in one entry: the blob and each decoded view
 * populated independently, the registered decoders a load runs, the
 * probe that caches nothing, the poll that re-decodes a changed file,
 * and the write that goes back out through the mount it reads by.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <sigilimage/encode/Encode.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Sink.h>

#include <array>
#include <atomic>
#include <barrier>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "MountedHub.h"

using namespace sigil::io;
using sigil::io::test::touchForward;
using sigil::io::test::writePng;
namespace fs = std::filesystem;

/** A decoder for the test's own type: the text, counted. */
struct WordCount {
  size_t words = 0;
};
struct WordCounter {
  std::optional<WordCount> decode(const Bytes& bytes, std::string_view) const {
    WordCount count;
    bool inWord = false;
    for (const char c : bytes.asText()) {
      const bool space = c == ' ' || c == '\n';
      if (!space && !inWord) ++count.words;
      inWord = !space;
    }
    return count;
  }
};
static_assert(Decoder<WordCounter, WordCount>);

TEST_F(IOHub, RegisteredDecodersAnswerLoadAndReloadOnPoll) {
  dir.write("live.txt", "one two three");
  // No decoder for the type: null, and nothing fetched.
  EXPECT_EQ(hub.load<WordCount>("res://live.txt"), nullptr);
  hub.registerDecoder<WordCount>(WordCounter{});
  auto counted = hub.load<WordCount>("res://live.txt");
  ASSERT_NE(counted, nullptr);
  EXPECT_EQ(counted->words, 3u);
  // Cached: the same view answers again, beside the text view.
  EXPECT_EQ(hub.load<WordCount>("res://live.txt"), counted);
  EXPECT_EQ(hub.text("res://live.txt"), "one two three");
  // A changed file re-decodes every populated view from one read.
  dir.write("live.txt", "four five");
  touchForward(dir.path / "live.txt");
  EXPECT_TRUE(hub.poll());
  auto recounted = hub.load<WordCount>("res://live.txt");
  ASSERT_NE(recounted, nullptr);
  EXPECT_NE(recounted, counted);
  EXPECT_EQ(recounted->words, 2u);
  EXPECT_EQ(hub.text("res://live.txt"), "four five");
}

TEST_F(IOHub, ConcurrentColdLoadsPublishOneCachedView) {
  dir.write("shared.txt", "one two three");
  hub.registerDecoder<WordCount>(WordCounter{});
  constexpr size_t kReaders = 8;
  std::barrier start(static_cast<std::ptrdiff_t>(kReaders));
  std::array<std::shared_ptr<const WordCount>, kReaders> views;
  std::array<std::thread, kReaders> readers;
  for (size_t i = 0; i < kReaders; ++i)
    readers[i] = std::thread([&, i] {
      start.arrive_and_wait();
      views[i] = hub.load<WordCount>("res://shared.txt");
    });
  for (std::thread& reader : readers) reader.join();

  ASSERT_NE(views.front(), nullptr);
  EXPECT_EQ(views.front()->words, 3u);
  for (const auto& view : views) EXPECT_EQ(view, views.front());
}

TEST_F(IOHub, LoadImageAssetIsTheImageView) {
  writePng(dir.path / "logo.png", 3, SK_ColorRED);
  auto image = hub.image("res://logo.png");
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(hub.load<sigil::image::ImageAsset>("res://logo.png"), image);
  EXPECT_EQ(image->width(), 3);
}

// blob(), image(), and channels() are independent views of one
// resource: asking for one must not null a later ask for another.
TEST_F(IOHub, BlobThenImageThenChannelsAllAnswer) {
  writePng(dir.path / "logo.png", 1, SK_ColorRED);
  ASSERT_NE(hub.blob("res://logo.png"), nullptr);
  auto image = hub.image("res://logo.png");
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->width(), 1);
  ASSERT_NE(hub.channels("res://logo.png"), nullptr);
  // The earlier views are still served, not evicted by the later asks.
  EXPECT_NE(hub.blob("res://logo.png"), nullptr);
  EXPECT_NE(hub.image("res://logo.png"), nullptr);
}

// blob() never decodes: bytes no image codec accepts still load, and
// the failed image() ask that follows does not disturb them. This is
// the observable face of "asking for bytes costs no decode" — a blob
// ask cannot depend on decodability in any way.
TEST_F(IOHub, BlobAloneDoesNotDecode) {
  dir.write("fake.png", "not an image at all");
  auto bytes = hub.blob("res://fake.png");
  ASSERT_NE(bytes, nullptr);
  EXPECT_EQ(hub.image("res://fake.png"), nullptr);
  EXPECT_NE(hub.blob("res://fake.png"), nullptr);
}

// image() after blob() decodes the bytes the entry already holds:
// with the file deleted in between, the cached bytes are the only
// possible source, and no second read of the source happens.
TEST_F(IOHub, ImageDecodesOnDemandFromCachedBytes) {
  writePng(dir.path / "logo.png", 1, SK_ColorRED);
  ASSERT_NE(hub.blob("res://logo.png"), nullptr);
  fs::remove(dir.path / "logo.png");
  auto image = hub.image("res://logo.png");
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->width(), 1);
}

// A '#' in a filename is URI content, not cache-key syntax. The decoy
// file at the name a '#'-truncating parse would produce is the trap:
// poll() must stat and reload the real file, never the decoy.
TEST_F(IOHub, PollReloadsFilesWhoseNamesContainHash) {
  writePng(dir.path / "tile", 2, SK_ColorGREEN);      // decoy
  writePng(dir.path / "tile#3.png", 1, SK_ColorRED);  // the resource
  auto image = hub.image("res://tile#3.png");
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->width(), 1);
  // Nothing changed: no spurious erase, no reload against the decoy.
  EXPECT_FALSE(hub.poll());
  ASSERT_NE(hub.image("res://tile#3.png"), nullptr);
  EXPECT_EQ(hub.image("res://tile#3.png")->width(), 1);
  // Touch the real file: poll() reloads that same file.
  writePng(dir.path / "tile#3.png", 2, SK_ColorBLUE);
  touchForward(dir.path / "tile#3.png");
  EXPECT_TRUE(hub.poll());
  auto reloaded = hub.image("res://tile#3.png");
  ASSERT_NE(reloaded, nullptr);
  EXPECT_EQ(reloaded->width(), 2);
}

/** A decoder that asks the same hub for a second resource while it
 *  decodes. poll() re-runs it: were the cache lock held across that
 *  decode, the inner ask would wait on its own caller forever. */
struct Concatenation {
  std::string text;
};

TEST_F(IOHub, PollRunsDecodersOutsideTheCacheLock) {
  dir.write("head.txt", "head");
  dir.write("tail.txt", "tail");
  hub.registerDecoder<Concatenation>(
      [this](const Bytes& bytes, std::string_view) {
        auto tail = hub.text("res://tail.txt");
        return Concatenation{std::string(bytes.asText()) + tail.value_or("")};
      });
  auto joined = hub.load<Concatenation>("res://head.txt");
  ASSERT_NE(joined, nullptr);
  EXPECT_EQ(joined->text, "headtail");

  dir.write("head.txt", "HEAD");
  touchForward(dir.path / "head.txt");
  EXPECT_TRUE(hub.poll());
  auto reloaded = hub.load<Concatenation>("res://head.txt");
  ASSERT_NE(reloaded, nullptr);
  EXPECT_EQ(reloaded->text, "HEADtail");
  EXPECT_EQ(joined->text, "headtail");  // the old holder keeps the old
}

TEST_F(IOHub, ReRegisteringADecoderAppliesToLaterAsksOnly) {
  dir.write("live.txt", "one two three");
  hub.registerDecoder<WordCount>(WordCounter{});
  auto counted = hub.load<WordCount>("res://live.txt");
  ASSERT_NE(counted, nullptr);
  EXPECT_EQ(counted->words, 3u);

  // A decoder that counts nothing, registered after the view exists.
  hub.registerDecoder<WordCount>(
      [](const Bytes&, std::string_view) { return WordCount{0}; });
  dir.write("live.txt", "one two three four");
  touchForward(dir.path / "live.txt");
  EXPECT_TRUE(hub.poll());
  // The view keeps the decoder that made it.
  EXPECT_EQ(hub.load<WordCount>("res://live.txt")->words, 4u);
  // A fresh entry takes the new one.
  dir.write("other.txt", "five six");
  EXPECT_EQ(hub.load<WordCount>("res://other.txt")->words, 0u);
}

TEST_F(IOHub, ProbeReportsHowManyBytesAndWhereTheyAre) {
  dir.write("table.bin", std::string(64, '\0'));
  auto info = hub.probe("res://table.bin");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->byteSize, 64u);
  EXPECT_EQ(info->path, dir.path / "table.bin");
  // Bytes are all the hub answers for. What they mean is asked of the
  // library that owns the meaning, and these bytes are not an image.
  EXPECT_FALSE(hub.probe<sigil::image::ImageProbe>("res://table.bin"));
  EXPECT_FALSE(hub.probe("res://nothing.bin"));
}

TEST_F(IOHub, WriteStoresThroughTheMountItReadsBy) {
  const std::string_view payload = "written through the mount";
  ASSERT_TRUE(hub.write("res://out/note.txt", payload.data(), payload.size()));
  EXPECT_TRUE(fs::exists(dir.path / "out" / "note.txt"));
  auto text = hub.text("res://out/note.txt");
  ASSERT_TRUE(text.has_value());
  EXPECT_EQ(*text, payload);
}

TEST_F(IOHub, WriteReplacesWhatWasCached) {
  dir.write("note.txt", "before");
  ASSERT_EQ(hub.text("res://note.txt"), "before");
  const std::string_view after = "after";
  ASSERT_TRUE(hub.write("res://note.txt", after.data(), after.size()));
  // The cached entry is gone rather than stale, so this reads the file.
  EXPECT_EQ(hub.text("res://note.txt"), "after");
}

TEST_F(IOHub, WrittenImageBytesDecodeBackThroughTheHub) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(7, 7));
  bitmap.eraseColor(SK_ColorMAGENTA);
  const sk_sp<SkData> encoded =
      sigil::image::encodeImage(bitmap.pixmap(), sigil::image::Format::Png);
  ASSERT_TRUE(encoded);
  ASSERT_TRUE(
      hub.write("res://made/tile.png", encoded->data(), encoded->size()));
  auto image = hub.image("res://made/tile.png");
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->width(), 7);
}

TEST_F(IOHub, NetworkUrisCannotBeWritten) {
  const std::string_view payload = "nope";
  EXPECT_FALSE(
      hub.write("https://example.com/x.txt", payload.data(), payload.size()));
}

// A poll() and a write() on one URI are one entry's fate decided twice:
// the write drops what was cached, and the poll commits a reload only
// into an entry nobody replaced meanwhile. Whichever order they land
// in, the cache is left holding one version of the resource, and it is
// the version the file holds.
TEST_F(IOHub, PollBesideAWriteLeavesOneCoherentVersion) {
  constexpr size_t kWrites = 60;
  const std::string uri = "res://raced.txt";
  dir.write("raced.txt", "version 0");
  ASSERT_EQ(hub.text(uri), "version 0");

  std::atomic<bool> writing{true};
  std::thread poller([&] {
    while (writing.load()) hub.poll();
  });
  std::string last;
  for (size_t i = 1; i <= kWrites; ++i) {
    last = "version " + std::to_string(i);
    ASSERT_TRUE(hub.write(uri, last.data(), last.size()));
  }
  writing = false;
  poller.join();

  // The last write dropped the entry it overwrote, so this ask reads
  // the file — and reads it whole.
  EXPECT_EQ(hub.text(uri), last);
  std::ifstream file(dir.path / "raced.txt", std::ios::binary);
  std::ostringstream onDisk;
  onDisk << file.rdbuf();
  EXPECT_EQ(onDisk.str(), last);
}

TEST_F(IOChannels, LdrFormatsNormalizeToFloats) {
  // A 1x1 red PNG (encoded by Skia) via the Skia decode path:
  // channels arrive as R/G/B/A in 0..1.
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  bitmap.eraseColor(SK_ColorRED);
  const sk_sp<SkData> png =
      sigil::image::encodeImage(bitmap.pixmap(), sigil::image::Format::Png);
  ASSERT_TRUE(png);
  ASSERT_TRUE(writeBytes(dir.path / "red.png", png->data(), png->size()));
  auto channels = hub.channels("res://red.png");
  ASSERT_NE(channels, nullptr);
  ASSERT_EQ(channels->names.size(), 4u);
  EXPECT_FALSE(channels->floatingPoint);
  EXPECT_FLOAT_EQ(channels->at(0, 0, 0), 1.0f);  // R
  EXPECT_FLOAT_EQ(channels->at(0, 0, 3), 1.0f);  // A
}
