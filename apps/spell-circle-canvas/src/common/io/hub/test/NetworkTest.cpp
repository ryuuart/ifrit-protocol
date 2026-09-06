/** @file
 * Network resources: the cache filename a URL maps to, the disk cache
 * in front of every policy, what each policy does when a fetch fails,
 * and how a fetched body persists. Every case but the last one drives a
 * stub transport or a pre-seeded cache, so no case here leaves the
 * machine.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <sigilimage/encode/Encode.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/hub/Network.h>
#include <sigilio/source/Sink.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "ScratchDir.h"

using namespace sigil::io;
using sigil::test::ScratchDir;
namespace fs = std::filesystem;

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
  EXPECT_GT(info->byteSize, 0u);
  auto probed = hub.probe<sigil::image::ImageProbe>(url);
  ASSERT_TRUE(probed.has_value());
  EXPECT_EQ(probed->format, "png");
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
  // A transport that fails every fetch stands in for the network, so no
  // resolver is consulted: Refresh asks it first, then the cache answers.
  size_t asked = 0;
  hub.setNetworkTransport([&asked](std::string_view) {
    ++asked;
    return std::optional<std::vector<std::byte>>{};
  });
  EXPECT_EQ(hub.text(url), "yesterday's copy");
  EXPECT_EQ(asked, 1u);
}

TEST(IONetwork, FetchedBytesPersistWholeOrNotAtAll) {
  const ScratchDir cache("sigilio_net");
  const std::string url = "https://fake.invalid/fresh.bin";
  Hub hub;
  hub.setNetworkCacheDir(cache.path);
  hub.setNetworkTransport([](std::string_view) {
    return std::optional<std::vector<std::byte>>{
        std::vector<std::byte>{std::byte{'o'}, std::byte{'k'}}};
  });
  auto fetched = hub.blob(url);
  ASSERT_NE(fetched, nullptr);
  EXPECT_EQ(fetched->asText(), "ok");
  // Persisted under the cache name, and nothing partial beside it.
  EXPECT_TRUE(fs::is_regular_file(cache.path / networkCacheKey(url)));
  EXPECT_FALSE(fs::exists(cache.path / (networkCacheKey(url) + ".part")));

  Hub offline;
  offline.setNetworkCacheDir(cache.path);
  offline.setNetworkPolicy(NetworkPolicy::Offline);
  EXPECT_EQ(offline.text(url), "ok");
}

// The one case in this file that reaches the network: fetch a URL over
// the built-in transport, then read it back through a fresh hub locked
// Offline, where the persisted cache is the only possible source. It has
// a ctest entry of its own so a default run never leaves the machine,
// and that entry carries the `network` label.
TEST(IONetwork, LiveFetchThenOfflineRoundTrip) {
  const ScratchDir cache("sigilio_net");
  // An immutable commit, so the bytes on the far end never change under
  // the case; how many of them there are is the far end's fact and not
  // this library's, so only the round trip is claimed.
  const std::string url =
      "https://raw.githubusercontent.com/KhronosGroup/"
      "glTF-Sample-Assets/2bac6f8c57bf471df0d2a1e8a8ec023c7801dddf/"
      "Models/Duck/glTF-Binary/Duck.glb";
  Hub online;
  online.setNetworkCacheDir(cache.path);
  auto fetched = online.blob(url);
  if (!fetched) GTEST_SKIP() << "no route to " << url;
  EXPECT_FALSE(fetched->bytes.empty());

  Hub offline;
  offline.setNetworkCacheDir(cache.path);
  offline.setNetworkPolicy(NetworkPolicy::Offline);
  auto replay = offline.blob(url);
  ASSERT_NE(replay, nullptr);
  EXPECT_EQ(replay->bytes, fetched->bytes);
}
