/** @file
 * The hub's behavior: mounts and URI resolution, blob/text loads, probe
 * metadata, hot reload, and — with the OpenImageIO backend — EXR decode
 * into float SkImages with layer/channel selection.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkStream.h>
#include <include/encode/SkPngEncoder.h>
#include <sigilloader/hub/Hub.h>

#ifdef SIGILLOADER_HAS_OIIO
#include <OpenImageIO/imageio.h>
#endif

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace sigil::loader;
namespace fs = std::filesystem;

namespace {

struct TempDir {
  fs::path path;
  TempDir() {
    path = fs::temp_directory_path() /
           ("sigilloader_test_" + std::to_string(::getpid()));
    fs::create_directories(path);
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
  void write(const std::string& name, std::string_view content) {
    fs::create_directories((path / name).parent_path());
    std::ofstream(path / name, std::ios::binary) << content;
  }
};

/** A size x size solid PNG, for tests that need a real decodable
 *  image whose dimensions identify which file (or which version of a
 *  file) a decode came from. */
void writePng(const fs::path& path, int size, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(size, size));
  bitmap.eraseColor(color);
  SkFILEWStream stream(path.string().c_str());
  ASSERT_TRUE(SkPngEncoder::Encode(&stream, bitmap.pixmap(), {}));
  stream.flush();
}

}  // namespace

// The hub is the reference ByteSource: the source concepts are
// what every consumer writes against, so the hub must satisfy them.
static_assert(ByteSource<Hub>);
static_assert(ResolvingByteSource<Hub>);

TEST(LoaderSource, HubFetchesAndErasesToAnyByteSource) {
  TempDir dir;
  dir.write("notes/hello.txt", "carry the coal");
  Hub hub;
  hub.mount("res://", dir.path);
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

TEST(LoaderHub, RegisteredDecodersAnswerLoadAndReloadOnPoll) {
  TempDir dir;
  dir.write("live.txt", "one two three");
  Hub hub;
  hub.mount("res://", dir.path);
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
  fs::last_write_time(dir.path / "live.txt", fs::file_time_type::clock::now() +
                                                 std::chrono::seconds(2));
  EXPECT_TRUE(hub.poll());
  auto recounted = hub.load<WordCount>("res://live.txt");
  ASSERT_NE(recounted, nullptr);
  EXPECT_NE(recounted, counted);
  EXPECT_EQ(recounted->words, 2u);
  EXPECT_EQ(hub.text("res://live.txt"), "four five");
}

TEST(LoaderHub, LoadImageAssetIsTheImageView) {
  TempDir dir;
  writePng(dir.path / "logo.png", 3, SK_ColorRED);
  Hub hub;
  hub.mount("res://", dir.path);
  auto image = hub.image("res://logo.png");
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(hub.load<sigil::image::ImageAsset>("res://logo.png"), image);
  EXPECT_EQ(image->width(), 3);
}

TEST(LoaderHub, MountsResolveLongestPrefix) {
  TempDir dir;
  Hub hub;
  hub.mount("res://", dir.path);
  hub.mount("res://deep/", dir.path / "elsewhere");
  EXPECT_EQ(hub.resolve("res://a.txt"), dir.path / "a.txt");
  EXPECT_EQ(hub.resolve("res://deep/b.txt"), dir.path / "elsewhere" / "b.txt");
  EXPECT_TRUE(hub.resolve("other://x").empty());
}

TEST(LoaderHub, BlobAndTextLoadThroughMounts) {
  TempDir dir;
  dir.write("notes/hello.txt", "carry the coal");
  Hub hub;
  hub.mount("res://", dir.path);
  auto text = hub.text("res://notes/hello.txt");
  ASSERT_TRUE(text.has_value());
  EXPECT_EQ(*text, "carry the coal");
  auto bytes = hub.blob("res://notes/hello.txt");
  ASSERT_NE(bytes, nullptr);
  EXPECT_EQ(bytes->bytes.size(), 14u);
  EXPECT_EQ(hub.blob("res://missing.bin"), nullptr);
}

TEST(LoaderHub, MissingFilesHealWithoutStaleCache) {
  TempDir dir;
  Hub hub;
  hub.mount("res://", dir.path);
  EXPECT_EQ(hub.text("res://late.txt"), std::nullopt);
  dir.write("late.txt", "arrived");
  EXPECT_EQ(hub.text("res://late.txt"), "arrived");
}

TEST(LoaderHub, PollReloadsChangedText) {
  TempDir dir;
  dir.write("live.txt", "one");
  Hub hub;
  hub.mount("res://", dir.path);
  EXPECT_EQ(hub.text("res://live.txt"), "one");
  // Filesystem mtime granularity can be coarse; force a distinct stamp.
  dir.write("live.txt", "two");
  fs::last_write_time(dir.path / "live.txt", fs::file_time_type::clock::now() +
                                                 std::chrono::seconds(2));
  EXPECT_TRUE(hub.poll());
  EXPECT_EQ(hub.text("res://live.txt"), "two");
}

// blob(), image(), and channels() are independent views of one
// resource: asking for one must not null a later ask for another.
TEST(LoaderHub, BlobThenImageThenChannelsAllAnswer) {
  TempDir dir;
  writePng(dir.path / "logo.png", 1, SK_ColorRED);
  Hub hub;
  hub.mount("res://", dir.path);
  ASSERT_NE(hub.blob("res://logo.png"), nullptr);
  auto image = hub.image("res://logo.png");
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->width(), 1);
  ASSERT_NE(hub.channels("res://logo.png"), nullptr);
  // The earlier views are still served, not evicted by the later asks.
  EXPECT_NE(hub.blob("res://logo.png"), nullptr);
  EXPECT_NE(hub.image("res://logo.png"), nullptr);
}

TEST(LoaderHub, ImageThenBlobBothAnswer) {
  TempDir dir;
  writePng(dir.path / "logo.png", 1, SK_ColorRED);
  Hub hub;
  hub.mount("res://", dir.path);
  auto image = hub.image("res://logo.png");
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->width(), 1);
  auto bytes = hub.blob("res://logo.png");
  ASSERT_NE(bytes, nullptr);
  EXPECT_GT(bytes->bytes.size(), 0u);
  EXPECT_NE(hub.image("res://logo.png"), nullptr);
}

// blob() never decodes: bytes no image codec accepts still load, and
// the failed image() ask that follows does not disturb them. This is
// the observable face of "asking for bytes costs no decode" — a blob
// ask cannot depend on decodability in any way.
TEST(LoaderHub, BlobAloneDoesNotDecode) {
  TempDir dir;
  dir.write("fake.png", "not an image at all");
  Hub hub;
  hub.mount("res://", dir.path);
  auto bytes = hub.blob("res://fake.png");
  ASSERT_NE(bytes, nullptr);
  EXPECT_EQ(hub.image("res://fake.png"), nullptr);
  EXPECT_NE(hub.blob("res://fake.png"), nullptr);
}

// image() after blob() decodes the bytes the entry already holds:
// with the file deleted in between, the cached bytes are the only
// possible source, and no second read of the source happens.
TEST(LoaderHub, ImageDecodesOnDemandFromCachedBytes) {
  TempDir dir;
  writePng(dir.path / "logo.png", 1, SK_ColorRED);
  Hub hub;
  hub.mount("res://", dir.path);
  ASSERT_NE(hub.blob("res://logo.png"), nullptr);
  fs::remove(dir.path / "logo.png");
  auto image = hub.image("res://logo.png");
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->width(), 1);
}

// A '#' in a filename is URI content, not cache-key syntax. The decoy
// file at the name a '#'-truncating parse would produce is the trap:
// poll() must stat and reload the real file, never the decoy.
TEST(LoaderHub, PollReloadsFilesWhoseNamesContainHash) {
  TempDir dir;
  writePng(dir.path / "tile", 2, SK_ColorGREEN);      // decoy
  writePng(dir.path / "tile#3.png", 1, SK_ColorRED);  // the resource
  Hub hub;
  hub.mount("res://", dir.path);
  auto image = hub.image("res://tile#3.png");
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->width(), 1);
  // Nothing changed: no spurious erase, no reload against the decoy.
  EXPECT_FALSE(hub.poll());
  ASSERT_NE(hub.image("res://tile#3.png"), nullptr);
  EXPECT_EQ(hub.image("res://tile#3.png")->width(), 1);
  // Touch the real file: poll() reloads that same file.
  writePng(dir.path / "tile#3.png", 2, SK_ColorBLUE);
  fs::last_write_time(
      dir.path / "tile#3.png",
      fs::file_time_type::clock::now() + std::chrono::seconds(2));
  EXPECT_TRUE(hub.poll());
  auto reloaded = hub.image("res://tile#3.png");
  ASSERT_NE(reloaded, nullptr);
  EXPECT_EQ(reloaded->width(), 2);
}

TEST(LoaderHub, ProbeReportsPlainData) {
  TempDir dir;
  dir.write("table.bin", std::string(64, '\0'));
  Hub hub;
  hub.mount("res://", dir.path);
  auto info = hub.probe("res://table.bin");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->kind, ResourceInfo::Kind::Data);
  EXPECT_EQ(info->byteSize, 64u);
}

TEST(LoaderHub, FileUrlsLoadAsLocalPaths) {
  TempDir dir;
  dir.write("direct.txt", "no mount needed");
  Hub hub;  // note: nothing mounted
  const std::string url = "file://" + (dir.path / "direct.txt").string();
  auto text = hub.text(url);
  ASSERT_TRUE(text.has_value());
  EXPECT_EQ(*text, "no mount needed");
  auto bytes = hub.blob(url);
  ASSERT_NE(bytes, nullptr);
  EXPECT_EQ(bytes->bytes.size(), 15u);
}

TEST(LoaderHub, WriteStoresThroughTheMountItReadsBy) {
  TempDir dir;
  Hub hub;
  hub.mount("res://", dir.path);
  const std::string_view payload = "written through the mount";
  ASSERT_TRUE(hub.write("res://out/note.txt", payload.data(), payload.size()));
  EXPECT_TRUE(fs::exists(dir.path / "out" / "note.txt"));
  auto text = hub.text("res://out/note.txt");
  ASSERT_TRUE(text.has_value());
  EXPECT_EQ(*text, payload);
}

TEST(LoaderHub, WriteReplacesWhatWasCached) {
  TempDir dir;
  dir.write("note.txt", "before");
  Hub hub;
  hub.mount("res://", dir.path);
  ASSERT_EQ(hub.text("res://note.txt"), "before");
  const std::string_view after = "after";
  ASSERT_TRUE(hub.write("res://note.txt", after.data(), after.size()));
  // The cached entry is gone rather than stale, so this reads the file.
  EXPECT_EQ(hub.text("res://note.txt"), "after");
}

TEST(LoaderHub, WrittenImageBytesDecodeBackThroughTheHub) {
  TempDir dir;
  Hub hub;
  hub.mount("res://", dir.path);
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(7, 7));
  bitmap.eraseColor(SK_ColorMAGENTA);
  SkDynamicMemoryWStream stream;
  ASSERT_TRUE(SkPngEncoder::Encode(&stream, bitmap.pixmap(), {}));
  const sk_sp<SkData> encoded = stream.detachAsData();
  ASSERT_TRUE(
      hub.write("res://made/tile.png", encoded->data(), encoded->size()));
  auto image = hub.image("res://made/tile.png");
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->width(), 7);
}

TEST(LoaderHub, NetworkUrisCannotBeWritten) {
  Hub hub;
  const std::string_view payload = "nope";
  EXPECT_FALSE(
      hub.write("https://example.com/x.txt", payload.data(), payload.size()));
}

TEST(LoaderNet, CacheKeyKeepsUrlExtension) {
  const std::string key =
      networkCacheKey("https://fake.invalid/a/logo.png?v=2");
  EXPECT_TRUE(key.ends_with(".png"));
  EXPECT_EQ(key, networkCacheKey("https://fake.invalid/a/logo.png?v=2"));
  EXPECT_NE(key, networkCacheKey("https://fake.invalid/a/other.png?v=2"));
  // No extension in the URL path: bare hash, no trailing dot-noise.
  EXPECT_EQ(networkCacheKey("https://fake.invalid/api/blob").find('.'),
            std::string::npos);
}

TEST(LoaderNet, SeededCacheServesWithoutNetwork) {
  // Hermetic: the cache file is pre-seeded under the same key the hub
  // computes, so the fake host is never contacted.
  TempDir cache;
  const std::string url = "https://fake.invalid/x.txt";
  std::ofstream(cache.path / networkCacheKey(url), std::ios::binary)
      << "from the cache";
  Hub hub;
  hub.setNetworkCacheDir(cache.path);
  auto text = hub.text(url);
  ASSERT_TRUE(text.has_value());
  EXPECT_EQ(*text, "from the cache");
}

TEST(LoaderNet, SeededCacheDecodesImagesWithExtensionHint) {
  TempDir cache;
  const std::string url = "https://fake.invalid/red.png";
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  bitmap.eraseColor(SK_ColorRED);
  SkFILEWStream stream((cache.path / networkCacheKey(url)).string().c_str());
  ASSERT_TRUE(SkPngEncoder::Encode(&stream, bitmap.pixmap(), {}));
  stream.flush();
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

TEST(LoaderNet, PollSkipsNetworkEntries) {
  TempDir cache;
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

#ifdef SIGILLOADER_HAS_OIIO

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

TEST(LoaderOiio, ExrDecodesToFloatImage) {
  TempDir dir;
  writeLayeredExr(dir.path / "probe.exr");
  Hub hub;
  hub.mount("res://", dir.path);
  auto image = hub.image("res://probe.exr");
  ASSERT_NE(image, nullptr);
  ASSERT_FALSE(image->frames().empty());
  const sk_sp<SkImage>& sk = image->frames().front().image;
  EXPECT_EQ(sk->width(), 4);
  EXPECT_EQ(sk->colorType(), kRGBA_F32_SkColorType);
}

TEST(LoaderOiio, ExrLayerSelectionReadsHdrChannels) {
  TempDir dir;
  writeLayeredExr(dir.path / "probe.exr");
  Hub hub;
  hub.mount("res://", dir.path);
  auto glow = hub.image("res://probe.exr", {.layer = "glow"});
  ASSERT_NE(glow, nullptr);
  const sk_sp<SkImage>& sk = glow->frames().front().image;
  SkPixmap pixmap;
  ASSERT_TRUE(sk->peekPixels(&pixmap));
  const float* px = (const float*)pixmap.addr(0, 0);
  EXPECT_FLOAT_EQ(px[0], 2.5f);  // HDR value survives (F32)
  EXPECT_FLOAT_EQ(px[1], 0.125f);
}

TEST(LoaderOiio, ProbeListsLayersAndChannels) {
  TempDir dir;
  writeLayeredExr(dir.path / "probe.exr");
  Hub hub;
  hub.mount("res://", dir.path);
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

TEST(LoaderOiio, ChannelsExposeRawFloatData) {
  TempDir dir;
  writeLayeredExr(dir.path / "probe.exr");
  Hub hub;
  hub.mount("res://", dir.path);
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

#endif  // SIGILLOADER_HAS_OIIO

TEST(LoaderChannels, LdrFormatsNormalizeToFloats) {
  // A 1x1 red PNG (encoded by Skia) via the Skia decode path:
  // channels arrive as R/G/B/A in 0..1.
  TempDir dir;
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  bitmap.eraseColor(SK_ColorRED);
  SkFILEWStream stream((dir.path / "red.png").string().c_str());
  ASSERT_TRUE(SkPngEncoder::Encode(&stream, bitmap.pixmap(), {}));
  stream.flush();
  Hub hub;
  hub.mount("res://", dir.path);
  auto channels = hub.channels("res://red.png");
  ASSERT_NE(channels, nullptr);
  ASSERT_EQ(channels->names.size(), 4u);
  EXPECT_FALSE(channels->floatingPoint);
  EXPECT_FLOAT_EQ(channels->at(0, 0, 0), 1.0f);  // R
  EXPECT_FLOAT_EQ(channels->at(0, 0, 3), 1.0f);  // A
}

TEST(LoaderNet, OfflinePolicyServesCacheAndNeverFetches) {
  TempDir cache;
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

TEST(LoaderNet, RefreshPolicyFallsBackToCacheOnFetchFailure) {
  TempDir cache;
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
// only possible source. Pinned to an immutable commit.
//   SIGILLOADER_NET_TESTS=1 ./loader_test
TEST(LoaderNet, LiveFetchThenOfflineRoundTrip) {
  if (!std::getenv("SIGILLOADER_NET_TESTS"))
    GTEST_SKIP() << "set SIGILLOADER_NET_TESTS=1 to run live-network "
                    "tests";
  TempDir cache;
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
