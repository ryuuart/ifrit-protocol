/** @file
 * The hub's behavior: mounts and URI resolution, blob/text loads, probe
 * metadata, hot reload, and — with the OpenImageIO backend — EXR decode
 * into float SkImages with layer/channel selection.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <include/core/SkPixmap.h>
#include <sigilimage/encode/Encode.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/hub/TextLibrary.h>
#include <sigilio/source/Sink.h>

#ifdef SIGILIO_HAS_OIIO
#include <OpenImageIO/imageio.h>
#endif

#include <gtest/gtest.h>

#include <array>
#include <barrier>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "ScratchDir.h"

using namespace sigil::io;
using sigil::test::ScratchDir;
namespace fs = std::filesystem;

namespace {

/** A size x size solid PNG, for tests that need a real decodable
 *  image whose dimensions identify which file (or which version of a
 *  file) a decode came from. */
void writePng(const fs::path& path, int size, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(size, size));
  bitmap.eraseColor(color);
  const sk_sp<SkData> png =
      sigil::image::encodeImage(bitmap.pixmap(), sigil::image::Format::Png);
  ASSERT_TRUE(png);
  ASSERT_TRUE(writeBytes(path, png->data(), png->size()));
}

/** Stamps @p path in the near future, so a reload that watches mtimes
 *  cannot mistake this write for the one it already saw. A filesystem's
 *  timestamp granularity can be coarser than the gap between two writes
 *  in one case; the stamp is set outright rather than waited for,
 *  because waiting would put somebody else's granularity into this
 *  test's running time. */
void touchForward(const fs::path& path) {
  fs::last_write_time(
      path, fs::file_time_type::clock::now() + std::chrono::seconds(2));
}

std::vector<std::string> leaseUris(const ResourceLease& lease) {
  return {lease.uris().begin(), lease.uris().end()};
}

}  // namespace

// The hub is the reference ByteSource: the source concepts are
// what every consumer writes against, so the hub must satisfy them.
static_assert(ByteSource<Hub>);
static_assert(ResolvingByteSource<Hub>);

/** WHAT A HUB NEEDS BEFORE IT CAN BE ASKED ANYTHING: one scratch
 *  directory of its own, mounted at res://. */
class MountedHub : public ::testing::Test {
 protected:
  MountedHub() { hub.mount("res://", dir.path); }

  ScratchDir dir{"sigilio_hub"};
  Hub hub;
};

class IOSource : public MountedHub {};
class IOHub : public MountedHub {};
class IOOiio : public MountedHub {};
class IOChannels : public MountedHub {};

TEST_F(IOSource, HubFetchesAndErasesToAnyByteSource) {
  dir.write("notes/hello.txt", "carry the coal");
  auto fetched = hub.fetch("res://notes/hello.txt");
  ASSERT_NE(fetched, nullptr);
  EXPECT_EQ(fetched->asText(), "carry the coal");
  // fetch() and blob() are one cache entry.
  EXPECT_EQ(fetched, hub.blob("res://notes/hello.txt"));

  AnyByteSource any(hub);
  ASSERT_TRUE(any);
  EXPECT_EQ(any.fetch("res://notes/hello.txt"), fetched);
  EXPECT_EQ(any.resolve("res://notes/hello.txt"),
            dir.path / "notes" / "hello.txt");
  EXPECT_EQ(any.fetch("res://missing.txt"), nullptr);
  EXPECT_FALSE(AnyByteSource{});
}

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

TEST_F(IOHub, MountsResolveLongestPrefix) {
  hub.mount("res://deep/", dir.path / "elsewhere");
  EXPECT_EQ(hub.resolve("res://a.txt"), dir.path / "a.txt");
  EXPECT_EQ(hub.resolve("res://deep/b.txt"), dir.path / "elsewhere" / "b.txt");
  EXPECT_TRUE(hub.resolve("other://x").empty());
}

TEST_F(IOHub, SelectsFilesDirectoriesAndSegmentAwareGlobs) {
  dir.write("shaders/a.sksl", "a");
  dir.write("shaders/literal*.sksl", "literal");
  dir.write("shaders/nested/b.slang", "b");
  dir.write("shaders/nested/c.sksl", "c");
  const std::vector<std::string> all = {
      "res://shaders/a.sksl", "res://shaders/literal*.sksl",
      "res://shaders/nested/b.slang", "res://shaders/nested/c.sksl"};

  EXPECT_EQ(hub.select("res://shaders/a.sksl"),
            std::vector<std::string>{"res://shaders/a.sksl"});
  EXPECT_EQ(hub.select("res://shaders"), all);
  EXPECT_EQ(hub.select("res://shaders/"), all);
  EXPECT_EQ(hub.select("res://shaders/*.sksl"),
            (std::vector<std::string>{"res://shaders/a.sksl",
                                      "res://shaders/literal*.sksl"}));
  EXPECT_EQ(hub.select("res://shaders/**/*.sksl"),
            (std::vector<std::string>{"res://shaders/a.sksl",
                                      "res://shaders/literal*.sksl",
                                      "res://shaders/nested/c.sksl"}));
  EXPECT_EQ(hub.select("res://shaders/nested/?.slang"),
            std::vector<std::string>{"res://shaders/nested/b.slang"});
  EXPECT_EQ(hub.select("res://shaders/literal\\*.sksl"),
            std::vector<std::string>{"res://shaders/literal*.sksl"});
}

TEST_F(IOHub, SelectionHonorsNestedMounts) {
  dir.write("shaders/base.sksl", "base");
  dir.write("shaders/overlay/hidden.sksl", "hidden by mount");
  const ScratchDir overlay("sigilio_overlay");
  overlay.write("visible.sksl", "visible");
  hub.mount("res://shaders/overlay/", overlay.path);

  EXPECT_EQ(hub.select("res://shaders"),
            (std::vector<std::string>{"res://shaders/base.sksl",
                                      "res://shaders/overlay/visible.sksl"}));
}

TEST_F(IOHub, BlobAndTextLoadThroughMounts) {
  dir.write("notes/hello.txt", "carry the coal");
  auto text = hub.text("res://notes/hello.txt");
  ASSERT_TRUE(text.has_value());
  EXPECT_EQ(*text, "carry the coal");
  auto bytes = hub.blob("res://notes/hello.txt");
  ASSERT_NE(bytes, nullptr);
  EXPECT_EQ(bytes->bytes.size(), 14u);
  EXPECT_EQ(hub.blob("res://missing.bin"), nullptr);
}

TEST_F(IOHub, TextLibraryLoadsRelativeNamesAndPollsChanges) {
  dir.write("shaders/glow.sksl", "half4 main(float2 p) { return half4(1); }");
  TextLibrary library("shader://", dir.path / "shaders");
  EXPECT_EQ(library.text("shader://glow.sksl"),
            "half4 main(float2 p) { return half4(1); }");

  dir.write("shaders/glow.sksl", "half4 main(float2 p) { return half4(0); }");
  touchForward(dir.path / "shaders/glow.sksl");
  EXPECT_TRUE(library.poll());
  EXPECT_EQ(library.text("shader://glow.sksl"),
            "half4 main(float2 p) { return half4(0); }");
}

TEST_F(IOHub, TextLibrariesSharePreloadedRootCache) {
  dir.write("shaders/a.sksl", "a");
  dir.write("shaders/nested/b.slang", "b");
  TextLibrary loading("shader://", dir.path / "shaders");
  EXPECT_EQ(loading.preload(), 2u);

  dir.write("shaders/a.sksl", "changed after preload");
  TextLibrary consuming("shader://", dir.path / "shaders");
  EXPECT_EQ(consuming.text("shader://a.sksl"), "a");
  EXPECT_EQ(consuming.text("shader://nested/b.slang"), "b");
}

TEST_F(IOHub, TextLibrarySelectsAndPreloadsWithinItsPrefix) {
  dir.write("shaders/a.sksl", "a");
  dir.write("shaders/nested/b.slang", "b");
  dir.write("shaders/nested/c.sksl", "c");
  TextLibrary library("shader://", dir.path / "shaders");

  EXPECT_EQ(
      library.select("shader://**/*.sksl"),
      (std::vector<std::string>{"shader://a.sksl", "shader://nested/c.sksl"}));
  EXPECT_TRUE(library.select("res://**").empty());
  EXPECT_EQ(library.preload("shader://**/*.sksl"), 2u);
  dir.write("shaders/a.sksl", "changed after preload");
  dir.write("shaders/nested/b.slang", "not preloaded");
  EXPECT_EQ(library.text("shader://a.sksl"), "a");
  EXPECT_EQ(library.text("shader://nested/b.slang"), "not preloaded");
}

TEST_F(IOHub, MissingFilesHealWithoutStaleCache) {
  EXPECT_EQ(hub.text("res://late.txt"), std::nullopt);
  dir.write("late.txt", "arrived");
  EXPECT_EQ(hub.text("res://late.txt"), "arrived");
}

TEST_F(IOHub, PreloadFetchesDistinctUrisIntoTheByteCache) {
  dir.write("one.sksl", "one");
  dir.write("two.slang", "two");
  const std::string_view uris[] = {"res://one.sksl", "res://two.slang",
                                   "res://one.sksl", "res://missing.sksl"};
  EXPECT_EQ(hub.preload(uris), 2u);

  dir.write("one.sksl", "changed on disk");
  EXPECT_EQ(hub.text("res://one.sksl"), "one");
  EXPECT_EQ(hub.text("res://two.slang"), "two");
}

TEST_F(IOHub, PreloadDirectoryDiscoversNestedResourcesByUri) {
  dir.write("shaders/a.sksl", "a");
  dir.write("shaders/nested/b.slang", "b");
  hub.mount("shader://", dir.path / "shaders");
  EXPECT_EQ(hub.preloadDirectory("shader://"), 2u);

  dir.write("shaders/nested/b.slang", "changed after preload");
  EXPECT_EQ(hub.text("shader://a.sksl"), "a");
  EXPECT_EQ(hub.text("shader://nested/b.slang"), "b");
}

TEST_F(IOHub, PreloadSelectorCachesOnlyMatchingResources) {
  dir.write("shaders/a.sksl", "a");
  dir.write("shaders/nested/b.slang", "b");
  dir.write("shaders/nested/c.sksl", "c");
  EXPECT_EQ(hub.preload("res://shaders/**/*.sksl"), 2u);

  dir.write("shaders/a.sksl", "changed after preload");
  dir.write("shaders/nested/b.slang", "not preloaded");
  EXPECT_EQ(hub.text("res://shaders/a.sksl"), "a");
  EXPECT_EQ(hub.text("res://shaders/nested/c.sksl"), "c");
  EXPECT_EQ(hub.text("res://shaders/nested/b.slang"), "not preloaded");
}

TEST_F(IOHub, ResourceLeaseRetainsTheUnionOfMultipleSelectors) {
  dir.write("material/a.sksl", "material");
  dir.write("material/ignored.slang", "ignored");
  dir.write("compose/nested/b.slang", "compose");
  dir.write("loose.txt", "loose");
  ResourceLease shaders =
      hub.retain({"res://material/**/*.sksl", "res://compose/**/*.slang"});

  EXPECT_EQ(leaseUris(shaders),
            (std::vector<std::string>{"res://compose/nested/b.slang",
                                      "res://material/a.sksl"}));
  EXPECT_EQ(shaders.preload(), 2u);
  auto loose = hub.blob("res://loose.txt");
  ASSERT_NE(loose, nullptr);

  dir.write("material/a.sksl", "changed material");
  dir.write("compose/nested/b.slang", "changed compose");
  dir.write("loose.txt", "changed loose");
  EXPECT_EQ(hub.discardUnretained(), 1u);
  EXPECT_EQ(hub.text("res://material/a.sksl"), "material");
  EXPECT_EQ(hub.text("res://compose/nested/b.slang"), "compose");
  EXPECT_EQ(hub.text("res://loose.txt"), "changed loose");
  EXPECT_EQ(loose->asText(), "loose");
}

TEST_F(IOHub, ResourceLeaseRefreshesItsSelectorSnapshots) {
  dir.write("shaders/a.sksl", "a");
  ResourceLease shaders = hub.retain("res://shaders/**/*.sksl");
  ASSERT_EQ(shaders.preload(), 1u);
  EXPECT_EQ(leaseUris(shaders),
            std::vector<std::string>{"res://shaders/a.sksl"});

  dir.write("shaders/b.sksl", "b");
  EXPECT_EQ(leaseUris(shaders),
            std::vector<std::string>{"res://shaders/a.sksl"});
  EXPECT_EQ(shaders.refresh(), 2u);
  EXPECT_EQ(leaseUris(shaders),
            (std::vector<std::string>{"res://shaders/a.sksl",
                                      "res://shaders/b.sksl"}));
  EXPECT_EQ(shaders.preload(), 2u);

  fs::remove(dir.path / "shaders" / "a.sksl");
  EXPECT_EQ(shaders.refresh(), 1u);
  EXPECT_EQ(leaseUris(shaders),
            std::vector<std::string>{"res://shaders/b.sksl"});
  EXPECT_EQ(hub.discardUnretained(), 1u);
}

TEST_F(IOHub, OverlappingResourceLeasesRetainIndependently) {
  dir.write("shared.sksl", "shared");
  ResourceLease first = hub.retain("res://shared.sksl");
  ResourceLease second = hub.retain("res://shared.sksl");
  ASSERT_EQ(first.preload(), 1u);

  first = ResourceLease{};
  EXPECT_EQ(hub.discardUnretained(), 0u);
  second = ResourceLease{};
  EXPECT_EQ(hub.discardUnretained(), 1u);
}

TEST_F(IOHub, EmptyResourceLeaseCanIncludeSeveralSelectors) {
  dir.write("material/a.sksl", "a");
  dir.write("compose/b.slang", "b");
  ResourceLease shaders = hub.retain();

  EXPECT_EQ(shaders.include("res://material/**/*.sksl"), 1u);
  EXPECT_EQ(shaders.include("res://compose/**/*.slang"), 2u);
  EXPECT_EQ(leaseUris(shaders),
            (std::vector<std::string>{"res://compose/b.slang",
                                      "res://material/a.sksl"}));
}

TEST(IOResourceLease, MayBeDestroyedAfterItsHub) {
  ResourceLease lease;
  {
    Hub hub;
    lease = hub.retain();
  }
  lease = ResourceLease{};
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

TEST_F(IOHub, ProbeReportsPlainData) {
  dir.write("table.bin", std::string(64, '\0'));
  auto info = hub.probe("res://table.bin");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->kind, ResourceInfo::Kind::Data);
  EXPECT_EQ(info->byteSize, 64u);
}

TEST_F(IOHub, FileUrlsLoadAsLocalPaths) {
  // Nothing about the mount is used: a file:// URI strips to a plain
  // local path.
  dir.write("direct.txt", "no mount needed");
  const std::string url = "file://" + (dir.path / "direct.txt").string();
  auto text = hub.text(url);
  ASSERT_TRUE(text.has_value());
  EXPECT_EQ(*text, "no mount needed");
  auto bytes = hub.blob(url);
  ASSERT_NE(bytes, nullptr);
  EXPECT_EQ(bytes->bytes.size(), 15u);
}

TEST_F(IOHub, FileUrlsSelectDirectoriesAndGlobsWithoutAMount) {
  dir.write("files/a.sksl", "a");
  dir.write("files/nested/b.slang", "b");
  dir.write("files/nested/c.sksl", "c");
  const std::string base =
      "file://" + (dir.path / "files").lexically_normal().generic_string();

  EXPECT_EQ(hub.select(base), (std::vector<std::string>{
                                  base + "/a.sksl", base + "/nested/b.slang",
                                  base + "/nested/c.sksl"}));
  EXPECT_EQ(
      hub.select(base + "/**/*.sksl"),
      (std::vector<std::string>{base + "/a.sksl", base + "/nested/c.sksl"}));

  const std::string plain =
      (dir.path / "files").lexically_normal().generic_string();
  EXPECT_EQ(hub.select(plain + "/nested/*.sksl"),
            std::vector<std::string>{plain + "/nested/c.sksl"});
}

TEST_F(IOHub, NetworkSelectorsAreExactAndCannotGlob) {
  EXPECT_TRUE(hub.select("https://example.invalid/**/*.webm").empty());
  EXPECT_EQ(hub.select("https://example.invalid/assets/"),
            std::vector<std::string>{"https://example.invalid/assets/"});
  EXPECT_EQ(
      hub.select("https://example.invalid/assets/clip.webm"),
      std::vector<std::string>{"https://example.invalid/assets/clip.webm"});
  EXPECT_EQ(hub.select("https://example.invalid/image.png?v=2"),
            std::vector<std::string>{"https://example.invalid/image.png?v=2"});
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

TEST(IONetwork, CacheKeyKeepsUrlExtension) {
  const std::string key =
      networkCacheKey("https://fake.invalid/a/logo.png?v=2");
  EXPECT_TRUE(key.ends_with(".png"));
  EXPECT_EQ(key, networkCacheKey("https://fake.invalid/a/logo.png?v=2"));
  EXPECT_NE(key, networkCacheKey("https://fake.invalid/a/other.png?v=2"));
  // No extension in the URL path: bare hash, no trailing dot-noise.
  EXPECT_EQ(networkCacheKey("https://fake.invalid/api/blob").find('.'),
            std::string::npos);
}

TEST(IONetwork, SeededCacheServesWithoutNetwork) {
  // Hermetic: the cache file is pre-seeded under the same key the hub
  // computes, so the fake host is never contacted.
  const ScratchDir cache("sigilio_net");
  const std::string url = "https://fake.invalid/x.txt";
  std::ofstream(cache.path / networkCacheKey(url), std::ios::binary)
      << "from the cache";
  Hub hub;
  hub.setNetworkCacheDir(cache.path);
  auto text = hub.text(url);
  ASSERT_TRUE(text.has_value());
  EXPECT_EQ(*text, "from the cache");
}

TEST(IONetwork, SeededCacheDecodesImagesWithExtensionHint) {
  const ScratchDir cache("sigilio_net");
  const std::string url = "https://fake.invalid/red.png";
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  bitmap.eraseColor(SK_ColorRED);
  const sk_sp<SkData> png =
      sigil::image::encodeImage(bitmap.pixmap(), sigil::image::Format::Png);
  ASSERT_TRUE(png);
  ASSERT_TRUE(
      writeBytes(cache.path / networkCacheKey(url), png->data(), png->size()));
  Hub hub;
  hub.setNetworkCacheDir(cache.path);
  auto image = hub.image(url);
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->width(), 1);
  auto info = hub.probe(url);
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->kind, ResourceInfo::Kind::Image);
  EXPECT_EQ(info->format, "png");
}

TEST(IONetwork, PollSkipsNetworkEntries) {
  const ScratchDir cache("sigilio_net");
  const std::string url = "https://fake.invalid/data.bin";
  std::ofstream(cache.path / networkCacheKey(url), std::ios::binary) << "abc";
  Hub hub;
  hub.setNetworkCacheDir(cache.path);
  ASSERT_NE(hub.blob(url), nullptr);
  EXPECT_FALSE(hub.poll());  // no mtime to watch, nothing erased
  auto again = hub.blob(url);
  ASSERT_NE(again, nullptr);
  EXPECT_EQ(again->bytes.size(), 3u);
}

#ifdef SIGILIO_HAS_OIIO

namespace {

/** Writes a tiny EXR with layered channels: default RGBA plus a
 *  "glow" layer whose red channel is 2.5 (HDR range). */
void writeLayeredExr(const fs::path& path) {
  using namespace OIIO;
  constexpr int kSize = 4;
  ImageSpec spec(kSize, kSize, 8, TypeDesc::FLOAT);
  spec.channelnames = {"R",      "G",      "B",      "A",
                       "glow.R", "glow.G", "glow.B", "glow.A"};
  auto out = ImageOutput::create(path.string());
  ASSERT_TRUE(out);
  ASSERT_TRUE(out->open(path.string(), spec));
  std::vector<float> pixels(static_cast<size_t>(kSize) * kSize * 8);
  for (int px = 0; px < kSize * kSize; ++px) {
    float* p = pixels.data() + static_cast<ptrdiff_t>(px) * 8;
    p[0] = 0.25f;
    p[1] = 0.5f;
    p[2] = 0.75f;
    p[3] = 1.0f;  // base RGBA
    p[4] = 2.5f;
    p[5] = 0.125f;
    p[6] = 0.0f;
    p[7] = 1.0f;  // glow.*
  }
  ASSERT_TRUE(out->write_image(TypeDesc::FLOAT, pixels.data()));
  ASSERT_TRUE(out->close());
}

}  // namespace

TEST_F(IOOiio, ExrDecodesToFloatImage) {
  writeLayeredExr(dir.path / "probe.exr");
  auto image = hub.image("res://probe.exr");
  ASSERT_NE(image, nullptr);
  ASSERT_FALSE(image->frames().empty());
  const sk_sp<SkImage>& sk = image->frames().front().image;
  EXPECT_EQ(sk->width(), 4);
  EXPECT_EQ(sk->colorType(), kRGBA_F32_SkColorType);
}

TEST_F(IOOiio, ExrLayerSelectionReadsHdrChannels) {
  writeLayeredExr(dir.path / "probe.exr");
  auto glow = hub.image("res://probe.exr", {.layer = "glow"});
  ASSERT_NE(glow, nullptr);
  const sk_sp<SkImage>& sk = glow->frames().front().image;
  SkPixmap pixmap;
  ASSERT_TRUE(sk->peekPixels(&pixmap));
  const float* px = (const float*)pixmap.addr(0, 0);
  EXPECT_FLOAT_EQ(px[0], 2.5f);  // HDR value survives (F32)
  EXPECT_FLOAT_EQ(px[1], 0.125f);
}

TEST_F(IOOiio, ProbeListsLayersAndChannels) {
  writeLayeredExr(dir.path / "probe.exr");
  auto info = hub.probe("res://probe.exr");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->kind, ResourceInfo::Kind::Image);
  EXPECT_EQ(info->format, "openexr");
  EXPECT_EQ(info->image.width, 4);
  EXPECT_TRUE(info->image.floatingPoint);
  EXPECT_EQ(info->image.channels, 8);
  ASSERT_EQ(info->image.layers.size(), 1u);
  EXPECT_EQ(info->image.layers[0], "glow");
}

TEST_F(IOOiio, ChannelsExposeRawFloatData) {
  writeLayeredExr(dir.path / "probe.exr");
  auto channels = hub.channels("res://probe.exr");
  ASSERT_NE(channels, nullptr);
  EXPECT_EQ(channels->width, 4);
  EXPECT_TRUE(channels->floatingPoint);
  ASSERT_EQ(channels->names.size(), 8u);
  const int glowR = channels->index("glow.R");
  ASSERT_GE(glowR, 0);
  EXPECT_FLOAT_EQ(channels->at(0, 0, glowR), 2.5f);
  // And the Skia composition helper agrees.
  sk_sp<SkImage> composed = channels->makeImage("glow");
  ASSERT_NE(composed, nullptr);
  EXPECT_EQ(composed->colorType(), kRGBA_F32_SkColorType);
}

#endif  // SIGILIO_HAS_OIIO

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

TEST(IONetwork, OfflinePolicyServesCacheAndNeverFetches) {
  const ScratchDir cache("sigilio_net");
  const std::string cached = "https://fake.invalid/have.txt";
  std::ofstream(cache.path / networkCacheKey(cached), std::ios::binary)
      << "kept";
  Hub hub;
  hub.setNetworkCacheDir(cache.path);
  hub.setNetworkPolicy(NetworkPolicy::Offline);
  EXPECT_EQ(hub.text(cached), "kept");
  // A miss fails without touching the network (fake host untried).
  EXPECT_EQ(hub.blob("https://fake.invalid/missing.txt"), nullptr);
}

TEST(IONetwork, RefreshPolicyFallsBackToCacheOnFetchFailure) {
  const ScratchDir cache("sigilio_net");
  const std::string url = "https://fake.invalid/live.txt";
  std::ofstream(cache.path / networkCacheKey(url), std::ios::binary)
      << "yesterday's copy";
  Hub hub;
  hub.setNetworkCacheDir(cache.path);
  hub.setNetworkPolicy(NetworkPolicy::Refresh);
  // .invalid never resolves: the refetch fails, the cache answers.
  EXPECT_EQ(hub.text(url), "yesterday's copy");
}

// Opt-in live-network round trip: fetch once, then read the same URL
// back through a fresh hub locked Offline — the persisted cache is the
// only possible source. Pinned to an immutable commit. It is skipped
// unless SIGILIO_NET_TESTS is set in the environment, so a default
// run needs no connectivity.
TEST(IONetwork, LiveFetchThenOfflineRoundTrip) {
  if (!std::getenv("SIGILIO_NET_TESTS"))
    GTEST_SKIP() << "set SIGILIO_NET_TESTS=1 to run live-network "
                    "tests";
  const ScratchDir cache("sigilio_net");
  const std::string url =
      "https://raw.githubusercontent.com/KhronosGroup/"
      "glTF-Sample-Assets/2bac6f8c57bf471df0d2a1e8a8ec023c7801dddf/"
      "Models/Duck/glTF-Binary/Duck.glb";
  Hub online;
  online.setNetworkCacheDir(cache.path);
  auto fetched = online.blob(url);
  ASSERT_NE(fetched, nullptr);
  EXPECT_EQ(fetched->bytes.size(), 120484u);

  Hub offline;
  offline.setNetworkCacheDir(cache.path);
  offline.setNetworkPolicy(NetworkPolicy::Offline);
  auto replay = offline.blob(url);
  ASSERT_NE(replay, nullptr);
  EXPECT_EQ(replay->bytes.size(), fetched->bytes.size());
}
