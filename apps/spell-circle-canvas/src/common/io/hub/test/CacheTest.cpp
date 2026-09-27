/** @file
 * The cache and what lives in one entry: the bytes and each decoded view
 * populated independently, the registered decoders a load runs, the
 * probe that caches nothing, the poll that re-decodes a changed file,
 * and the write that goes back out through the mount it reads by.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <sigilmedia/image/Channels.h>
#include <sigilmedia/image/Decode.h>
#include <sigilmedia/image/Encode.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Sink.h>
#include <sigilio/advanced/Decoding.h>
#include <sigilio/advanced/Places.h>

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
inline std::string_view meaningName(std::type_identity<WordCount>) {
  return "test.WordCount";
}
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

namespace excerpt {
/** A type of the test's own that loads with options: the first `count`
 *  characters of the text, with the options its namespace declares. */
struct Excerpt {
  std::string text;
};
struct ExcerptOptions {
  size_t count = 0;  ///< 0 keeps the whole text.
  bool operator==(const ExcerptOptions&) const = default;
};
inline ExcerptOptions loadOptions(std::type_identity<Excerpt>) { return {}; }
inline std::string_view meaningName(std::type_identity<Excerpt>) {
  return "test.Excerpt";
}
}  // namespace excerpt
static_assert(Configurable<excerpt::Excerpt>);
static_assert(!Configurable<WordCount>);

TEST_F(IOHub, RegisteredDecodersAnswerLoadAndReloadOnPoll) {
  dir.write("live.txt", "one two three");
  // No decoder for the type: null, and nothing fetched.
  EXPECT_EQ(hub.load<WordCount>("res://live.txt"), nullptr);
  registerDecoder<WordCount>(hub, WordCounter{});
  auto counted = hub.load<WordCount>("res://live.txt");
  ASSERT_NE(counted, nullptr);
  EXPECT_EQ(counted->words, 3u);
  // Cached: the same view answers again, beside the text view.
  EXPECT_EQ(hub.load<WordCount>("res://live.txt"), counted);
  EXPECT_EQ(hub.text("res://live.txt"), "one two three");
  // A changed file re-decodes every populated view from one read.
  dir.write("live.txt", "four five");
  touchForward(dir.path / "live.txt");
  EXPECT_TRUE(poll(hub));
  auto recounted = hub.load<WordCount>("res://live.txt");
  ASSERT_NE(recounted, nullptr);
  EXPECT_NE(recounted, counted);
  EXPECT_EQ(recounted->words, 2u);
  EXPECT_EQ(hub.text("res://live.txt"), "four five");
}

TEST_F(IOHub, ConcurrentColdLoadsPublishOneCachedView) {
  dir.write("shared.txt", "one two three");
  registerDecoder<WordCount>(hub, WordCounter{});
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

TEST_F(IOHub, ImagesLoadOnlyOnceSigilMediaRegistersItsDecoders) {
  writePng(dir.path / "logo.png", 3, SK_ColorRED);
  Hub bare;
  mount(bare, "res://", dir.path);
  EXPECT_EQ(bare.load<sigil::media::Image>("res://logo.png"), nullptr);
  EXPECT_NE(bare.read("res://logo.png"), nullptr);
  auto image = hub.load<sigil::media::Image>("res://logo.png");
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->size().width(), 3);
  // Options at their defaults are the plain ask, and share its view.
  EXPECT_EQ(hub.load<sigil::media::Image>("res://logo.png", {}), image);
}

// Options are the decoder's own, bound into the decode a load runs:
// equal options share one view, different ones are a decode of their
// own, and poll() re-runs each with the options it was made with.
TEST_F(IOHub, LoadWithOptionsDecodesOncePerDistinctOptions) {
  using excerpt::Excerpt;
  dir.write("poem.txt", "whose woods these are");
  std::atomic<int> decodes = 0;
  registerDecoder<Excerpt>(hub, [&decodes](const Bytes& bytes, std::string_view,
                 const excerpt::ExcerptOptions& options) {
        ++decodes;
        const std::string_view text = bytes.asText();
        return std::optional<Excerpt>(Excerpt{std::string(
            options.count ? text.substr(0, options.count) : text)});
      });
  auto whole = hub.load<Excerpt>("res://poem.txt");
  auto first = hub.load<Excerpt>("res://poem.txt", {.count = 5});
  ASSERT_NE(whole, nullptr);
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(whole->text, "whose woods these are");
  EXPECT_EQ(first->text, "whose");
  EXPECT_EQ(hub.load<Excerpt>("res://poem.txt", {.count = 5}), first);
  EXPECT_EQ(hub.load<Excerpt>("res://poem.txt", {.count = 0}), whole);
  EXPECT_EQ(decodes.load(), 2);

  dir.write("poem.txt", "stopping by woods");
  touchForward(dir.path / "poem.txt");
  EXPECT_TRUE(poll(hub));
  EXPECT_EQ(hub.load<Excerpt>("res://poem.txt", {.count = 5})->text, "stopp");
  EXPECT_EQ(hub.load<Excerpt>("res://poem.txt")->text, "stopping by woods");
}

// fetch(), load<ImageAsset>(), and load<ChannelData>() are independent views of one
// resource: asking for one must not null a later ask for another.
TEST_F(IOHub, FetchThenImageThenChannelsAllAnswer) {
  writePng(dir.path / "logo.png", 1, SK_ColorRED);
  ASSERT_NE(hub.read("res://logo.png"), nullptr);
  auto image = hub.load<sigil::media::Image>("res://logo.png");
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->size().width(), 1);
  ASSERT_NE(hub.load<sigil::media::Channels>("res://logo.png"), nullptr);
  // The earlier views are still served, not evicted by the later asks.
  EXPECT_NE(hub.read("res://logo.png"), nullptr);
  EXPECT_NE(hub.load<sigil::media::Image>("res://logo.png"), nullptr);
}

// fetch() never decodes: bytes no image codec accepts still load, and
// the failed load<ImageAsset>() ask that follows does not disturb them. This is
// the observable face of "asking for bytes costs no decode" — a fetch
// ask cannot depend on decodability in any way.
TEST_F(IOHub, FetchAloneDoesNotDecode) {
  dir.write("fake.png", "not an image at all");
  auto bytes = hub.read("res://fake.png");
  ASSERT_NE(bytes, nullptr);
  EXPECT_EQ(hub.load<sigil::media::Image>("res://fake.png"), nullptr);
  EXPECT_NE(hub.read("res://fake.png"), nullptr);
}

// load<ImageAsset>() after fetch() decodes the bytes the entry already holds:
// with the file deleted in between, the cached bytes are the only
// possible source, and no second read of the source happens.
TEST_F(IOHub, ImageDecodesOnDemandFromCachedBytes) {
  writePng(dir.path / "logo.png", 1, SK_ColorRED);
  ASSERT_NE(hub.read("res://logo.png"), nullptr);
  fs::remove(dir.path / "logo.png");
  auto image = hub.load<sigil::media::Image>("res://logo.png");
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->size().width(), 1);
}

// A '#' in a filename is URI content, not cache-key syntax. The decoy
// file at the name a '#'-truncating parse would produce is the trap:
// poll() must stat and reload the real file, never the decoy.
TEST_F(IOHub, PollReloadsFilesWhoseNamesContainHash) {
  writePng(dir.path / "tile", 2, SK_ColorGREEN);      // decoy
  writePng(dir.path / "tile#3.png", 1, SK_ColorRED);  // the resource
  auto image = hub.load<sigil::media::Image>("res://tile#3.png");
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->size().width(), 1);
  // Nothing changed: no spurious erase, no reload against the decoy.
  EXPECT_FALSE(poll(hub));
  ASSERT_NE(hub.load<sigil::media::Image>("res://tile#3.png"), nullptr);
  EXPECT_EQ(hub.load<sigil::media::Image>("res://tile#3.png")->size().width(), 1);
  // Touch the real file: poll() reloads that same file.
  writePng(dir.path / "tile#3.png", 2, SK_ColorBLUE);
  touchForward(dir.path / "tile#3.png");
  EXPECT_TRUE(poll(hub));
  auto reloaded = hub.load<sigil::media::Image>("res://tile#3.png");
  ASSERT_NE(reloaded, nullptr);
  EXPECT_EQ(reloaded->size().width(), 2);
}

/** A decoder that asks the same hub for a second resource while it
 *  decodes. poll() re-runs it: were the cache lock held across that
 *  decode, the inner ask would wait on its own caller forever. */
struct Concatenation {
  std::string text;
};
inline std::string_view meaningName(std::type_identity<Concatenation>) {
  return "test.Concatenation";
}

TEST_F(IOHub, PollRunsDecodersOutsideTheCacheLock) {
  dir.write("head.txt", "head");
  dir.write("tail.txt", "tail");
  registerDecoder<Concatenation>(hub, [this](const Bytes& bytes) {
        auto tail = hub.text("res://tail.txt");
        return Concatenation{std::string(bytes.asText()) + tail.value_or("")};
      });
  auto joined = hub.load<Concatenation>("res://head.txt");
  ASSERT_NE(joined, nullptr);
  EXPECT_EQ(joined->text, "headtail");

  dir.write("head.txt", "HEAD");
  touchForward(dir.path / "head.txt");
  EXPECT_TRUE(poll(hub));
  auto reloaded = hub.load<Concatenation>("res://head.txt");
  ASSERT_NE(reloaded, nullptr);
  EXPECT_EQ(reloaded->text, "HEADtail");
  EXPECT_EQ(joined->text, "headtail");  // the old holder keeps the old
}

/** A decoder that reads the bytes and nothing else — the hint is offered,
 *  so a format that never needed the name does not name it. */
struct ByteCounter {
  std::optional<WordCount> decode(const Bytes& bytes) const {
    return WordCount{(uint32_t)bytes.asText().size()};
  }
};
static_assert(Decoder<ByteCounter, WordCount>);

TEST_F(IOHub, ADecoderThatNeedsNoHintDoesNotNameOne) {
  // Both registrations offer the hint and take what the decoder names: an
  // object whose decode() reads the bytes alone satisfies the concept, and a
  // callable that names only the bytes is as good as one that names both.
  dir.write("live.txt", "abcd");
  registerDecoder<WordCount>(hub, ByteCounter{});
  auto counted = hub.load<WordCount>("res://live.txt");
  ASSERT_NE(counted, nullptr);
  EXPECT_EQ(counted->words, 4u);

  registerDecoder<Concatenation>(hub, [](const Bytes& bytes) {
    return Concatenation{"<" + std::string(bytes.asText()) + ">"};
  });
  auto wrapped = hub.load<Concatenation>("res://live.txt");
  ASSERT_NE(wrapped, nullptr);
  EXPECT_EQ(wrapped->text, "<abcd>");
}

TEST_F(IOHub, ReRegisteringADecoderAppliesToLaterAsksOnly) {
  dir.write("live.txt", "one two three");
  registerDecoder<WordCount>(hub, WordCounter{});
  auto counted = hub.load<WordCount>("res://live.txt");
  ASSERT_NE(counted, nullptr);
  EXPECT_EQ(counted->words, 3u);

  // A decoder that counts nothing, registered after the view exists.
  registerDecoder<WordCount>(hub, [] { return WordCount{0}; });
  dir.write("live.txt", "one two three four");
  touchForward(dir.path / "live.txt");
  EXPECT_TRUE(poll(hub));
  // The view keeps the decoder that made it.
  EXPECT_EQ(hub.load<WordCount>("res://live.txt")->words, 4u);
  // A fresh entry takes the new one.
  dir.write("other.txt", "five six");
  EXPECT_EQ(hub.load<WordCount>("res://other.txt")->words, 0u);
}

TEST_F(IOHub, ProbeReportsHowManyBytesAndWhereTheyAre) {
  dir.write("table.bin", std::string(64, '\0'));
  auto info = probe<ResourceInfo>(hub, "res://table.bin");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->byteSize, 64u);
  EXPECT_EQ(info->path, dir.path / "table.bin");
  // Bytes are all the hub answers for. What they mean is asked of the
  // library that owns the meaning, and these bytes are not an image.
  EXPECT_FALSE(probe<sigil::media::Metadata>(hub, "res://table.bin"));
  EXPECT_FALSE(probe<ResourceInfo>(hub, "res://nothing.bin"));
}

TEST_F(IOHub, WriteStoresThroughTheMountItReadsBy) {
  const std::string_view payload = "written through the mount";
  ASSERT_TRUE(hub.write("res://out/note.txt", std::as_bytes(std::span(payload))));
  EXPECT_TRUE(fs::exists(dir.path / "out" / "note.txt"));
  auto text = hub.text("res://out/note.txt");
  ASSERT_TRUE(text.has_value());
  EXPECT_EQ(*text, payload);
}

TEST_F(IOHub, WriteReplacesWhatWasCached) {
  dir.write("note.txt", "before");
  ASSERT_EQ(hub.text("res://note.txt"), "before");
  const std::string_view after = "after";
  ASSERT_TRUE(hub.write("res://note.txt", std::as_bytes(std::span(after))));
  // The cached entry is gone rather than stale, so this reads the file.
  EXPECT_EQ(hub.text("res://note.txt"), "after");
}

TEST_F(IOHub, WrittenImageBytesDecodeBackThroughTheHub) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(7, 7));
  bitmap.eraseColor(SK_ColorMAGENTA);
  const std::vector<std::byte> encoded =
      sigil::media::encode(bitmap.pixmap(), sigil::media::Format::Png);
  ASSERT_FALSE(encoded.empty());
  ASSERT_TRUE(hub.write("res://made/tile.png", encoded));
  auto image = hub.load<sigil::media::Image>("res://made/tile.png");
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->size().width(), 7);
}

TEST_F(IOHub, ASavedImageIsEncodedByItsNameAndDecodesBack) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(5, 3));
  bitmap.eraseColor(SK_ColorCYAN);
  const auto picture = sigil::media::Image::of(bitmap.asImage());
  // The name's extension picks the format; the library that owns the
  // value encodes it, and the hub writes where it mounts.
  ASSERT_TRUE(hub.save("res://made/saved.png", picture));
  auto image = hub.load<sigil::media::Image>("res://made/saved.png");
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->size(), SkISize::Make(5, 3));
  // A name that names nothing the library writes is refused, and so is
  // nothing at all.
  EXPECT_FALSE(hub.save("res://made/saved.txt", picture));
  EXPECT_FALSE(
      hub.save("res://made/none.png", std::shared_ptr<const sigil::media::Image>()));
}

TEST_F(IOHub, NetworkUrisCannotBeWritten) {
  const std::string_view payload = "nope";
  EXPECT_FALSE(
      hub.write("https://example.com/x.txt", std::as_bytes(std::span(payload))));
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
    while (writing.load()) poll(hub);
  });
  std::string last;
  for (size_t i = 1; i <= kWrites; ++i) {
    last = "version " + std::to_string(i);
    ASSERT_TRUE(hub.write(uri, std::as_bytes(std::span(last))));
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
  const std::vector<std::byte> png =
      sigil::media::encode(bitmap.pixmap(), sigil::media::Format::Png);
  ASSERT_FALSE(png.empty());
  ASSERT_TRUE(writeBytes(dir.path / "red.png", png.data(), png.size()));
  auto channels = hub.load<sigil::media::Channels>("res://red.png");
  ASSERT_NE(channels, nullptr);
  ASSERT_EQ(channels->names.size(), 4u);
  EXPECT_FALSE(channels->floatingPoint);
  EXPECT_FLOAT_EQ(channels->at(0, 0, 0), 1.0f);  // R
  EXPECT_FLOAT_EQ(channels->at(0, 0, 3), 1.0f);  // A
}
