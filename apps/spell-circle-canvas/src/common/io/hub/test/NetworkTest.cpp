/** @file
 * Network resources: the cache filename a URL maps to, the disk cache
 * in front of every policy, what each policy does when a fetch fails,
 * how a fetched body persists, and what two fetches of one URL at once
 * leave in the cache directory. Every case but the last one drives a
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

#include <atomic>
#include <barrier>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "../Fetch.h"
#include "ScratchDir.h"

using namespace sigil::io;
using sigil::test::ScratchDir;
namespace fs = std::filesystem;

namespace {

std::span<const std::byte> bytesOf(std::string_view text) {
  return std::as_bytes(std::span(text.data(), text.size()));
}

}  // namespace

TEST(IONetwork, CacheKeyKeepsUrlExtension) {
  const std::string key =
      detail::networkCacheKey("https://fake.invalid/a/logo.png?v=2");
  EXPECT_TRUE(key.ends_with(".png"));
  EXPECT_EQ(key,
            detail::networkCacheKey("https://fake.invalid/a/logo.png?v=2"));
  EXPECT_NE(key,
            detail::networkCacheKey("https://fake.invalid/a/other.png?v=2"));
  // No extension in the URL path: bare hash, no trailing dot-noise.
  EXPECT_EQ(detail::networkCacheKey("https://fake.invalid/api/blob").find('.'),
            std::string::npos);
}

TEST(IONetwork, DefaultCacheDirIsUnderThePlatformCacheLocation) {
  // The OS evicts the temp directory on its own schedule, so a fetch a
  // later run depends on cannot live there.
  const fs::path fallback = fs::temp_directory_path();
  const fs::path defaulted = detail::defaultNetworkCacheDirectory();
#if defined(_WIN32)
  const char* local = std::getenv("LOCALAPPDATA");
  const std::string root = local ? std::string(local) : "";
#elif defined(__APPLE__)
  const char* home = std::getenv("HOME");
  const std::string root = home ? std::string(home) + "/Library/Caches" : "";
#else
  const char* xdg = std::getenv("XDG_CACHE_HOME");
  const char* home = std::getenv("HOME");
  const std::string root = xdg && *xdg
                               ? std::string(xdg)
                               : (home ? std::string(home) + "/.cache" : "");
#endif
  if (root.empty()) {
    // No platform cache location: the temp directory is the door left.
    EXPECT_EQ(defaulted.parent_path().parent_path(), fallback);
  } else {
    EXPECT_EQ(defaulted, fs::path(root) / "SigilIO" / "network");
    EXPECT_NE(defaulted.parent_path().parent_path(), fallback);
  }

  // And a hub told where to put its cache puts it there instead: the
  // fetched body lands under the override, not under the default.
  const ScratchDir cache("sigilio_net");
  const std::string url = "https://fake.invalid/overridden.txt";
  const std::string body = "written where the hub was told";
  Hub hub;
  hub.setNetworkCacheDirectory(cache.path);
  hub.setNetworkTransport([&](std::string_view) {
    const auto* bytes = reinterpret_cast<const std::byte*>(body.data());
    return std::vector<std::byte>(bytes, bytes + body.size());
  });
  ASSERT_EQ(hub.text(url), body);
  EXPECT_EQ(probeNetworkCache(url, cache.path), body.size());
  EXPECT_FALSE(probeNetworkCache(url));
}

TEST(IONetwork, SeededCacheServesWithoutNetwork) {
  const ScratchDir cache("sigilio_net");
  const std::string url = "https://fake.invalid/x.txt";
  ASSERT_TRUE(seedNetworkCache(url, bytesOf("from the cache"), cache.path));
  Hub hub;
  hub.setNetworkCacheDirectory(cache.path);
  size_t requests = 0;
  hub.setNetworkTransport([&](std::string_view) {
    ++requests;
    return std::optional<std::vector<std::byte>>{};
  });
  auto text = hub.text(url);
  ASSERT_TRUE(text.has_value());
  EXPECT_EQ(*text, "from the cache");
  EXPECT_EQ(requests, 0u);
}

TEST(IONetwork, CacheProbeDistinguishesMissingAndEmptyWithoutCreatingFiles) {
  const ScratchDir root("sigilio_net_probe");
  const fs::path directory = root.path / "not-created";
  const std::string url = "https://fake.invalid/empty.txt";
  EXPECT_FALSE(probeNetworkCache(url, directory));
  EXPECT_FALSE(fs::exists(directory));

  ASSERT_TRUE(seedNetworkCache(url, {}, directory));
  EXPECT_EQ(probeNetworkCache(url, directory), 0u);
  EXPECT_FALSE(
      probeNetworkCache("https://fake.invalid/missing.txt", directory));

  Hub offline;
  offline.setNetworkCacheDirectory(directory);
  offline.setNetworkPolicy(NetworkPolicy::Offline);
  size_t requests = 0;
  offline.setNetworkTransport([&](std::string_view) {
    ++requests;
    return std::optional<std::vector<std::byte>>{};
  });
  auto bytes = offline.blob(url);
  ASSERT_NE(bytes, nullptr);
  EXPECT_TRUE(bytes->bytes.empty());
  EXPECT_EQ(requests, 0u);
}

TEST(IONetwork, SeedingUsesTheRequestedDirectoryAndKeepsUrlsDistinct) {
  const ScratchDir root("sigilio_net_seed");
  const fs::path first = root.path / "first";
  const fs::path second = root.path / "second";
  const std::string url = "https://fake.invalid/art.png?v=1";
  const std::string revision = "https://fake.invalid/art.png?v=2";
  ASSERT_TRUE(seedNetworkCache(url, bytesOf("first"), first));
  ASSERT_TRUE(seedNetworkCache(url, bytesOf("second"), second));
  ASSERT_TRUE(seedNetworkCache(revision, bytesOf("revision"), first));
  EXPECT_EQ(probeNetworkCache(url, first), 5u);
  EXPECT_EQ(probeNetworkCache(url, second), 6u);
  EXPECT_EQ(probeNetworkCache(revision, first), 8u);
  EXPECT_FALSE(probeNetworkCache(revision, second));

  Hub offline;
  offline.setNetworkCacheDirectory(first);
  offline.setNetworkPolicy(NetworkPolicy::Offline);
  EXPECT_EQ(offline.text(url), "first");
  EXPECT_EQ(offline.text(revision), "revision");
  ASSERT_TRUE(seedNetworkCache(url, bytesOf("replacement"), first));
  EXPECT_EQ(offline.text(url), "first");
  Hub reopened;
  reopened.setNetworkCacheDirectory(first);
  reopened.setNetworkPolicy(NetworkPolicy::Offline);
  EXPECT_EQ(reopened.text(url), "replacement");
}

TEST(IONetwork, CacheOperationsRefuseNonNetworkUrls) {
  const ScratchDir root("sigilio_net_uri");
  const fs::path directory = root.path / "not-created";
  for (std::string_view url : {"", "file:///tmp/data", "res://data", "data"}) {
    EXPECT_FALSE(seedNetworkCache(url, bytesOf("bytes"), directory));
    EXPECT_FALSE(probeNetworkCache(url, directory));
  }
  EXPECT_FALSE(fs::exists(directory));
}

TEST(IONetwork, FailedSeedLeavesNoPartialResource) {
  const ScratchDir cache("sigilio_net_seed_failure");
  const std::string url = "https://fake.invalid/blocked.txt";
  const fs::path blocked = cache.path / detail::networkCacheKey(url);
  fs::create_directory(blocked);
  ASSERT_TRUE(writeBytes(blocked / "keep", "kept", 4));

  EXPECT_FALSE(seedNetworkCache(url, bytesOf("replacement"), cache.path));
  EXPECT_FALSE(probeNetworkCache(url, cache.path));
  EXPECT_TRUE(fs::is_regular_file(blocked / "keep"));
  EXPECT_EQ(std::distance(fs::directory_iterator(cache.path),
                          fs::directory_iterator{}),
            1);
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
  ASSERT_TRUE(seedNetworkCache(
      url, {static_cast<const std::byte*>(png->data()), png->size()},
      cache.path));
  Hub hub;
  hub.setNetworkCacheDirectory(cache.path);
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
  ASSERT_TRUE(seedNetworkCache(url, bytesOf("abc"), cache.path));
  Hub hub;
  hub.setNetworkCacheDirectory(cache.path);
  ASSERT_NE(hub.blob(url), nullptr);
  EXPECT_FALSE(hub.poll());  // no mtime to watch, nothing erased
  auto again = hub.blob(url);
  ASSERT_NE(again, nullptr);
  EXPECT_EQ(again->bytes.size(), 3u);
}

TEST(IONetwork, OfflinePolicyServesCacheAndNeverFetches) {
  const ScratchDir cache("sigilio_net");
  const std::string cached = "https://fake.invalid/have.txt";
  ASSERT_TRUE(seedNetworkCache(cached, bytesOf("kept"), cache.path));
  Hub hub;
  hub.setNetworkCacheDirectory(cache.path);
  hub.setNetworkPolicy(NetworkPolicy::Offline);
  EXPECT_EQ(hub.text(cached), "kept");
  // A miss fails without touching the network (fake host untried).
  EXPECT_EQ(hub.blob("https://fake.invalid/missing.txt"), nullptr);
}

TEST(IONetwork, RefreshPolicyFallsBackToCacheOnFetchFailure) {
  const ScratchDir cache("sigilio_net");
  const std::string url = "https://fake.invalid/live.txt";
  ASSERT_TRUE(seedNetworkCache(url, bytesOf("yesterday's copy"), cache.path));
  Hub hub;
  hub.setNetworkCacheDirectory(cache.path);
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
  hub.setNetworkCacheDirectory(cache.path);
  hub.setNetworkTransport([](std::string_view) {
    return std::optional<std::vector<std::byte>>{
        std::vector<std::byte>{std::byte{'o'}, std::byte{'k'}}};
  });
  auto fetched = hub.blob(url);
  ASSERT_NE(fetched, nullptr);
  EXPECT_EQ(fetched->asText(), "ok");
  // Persisted under the cache name, and nothing partial beside it.
  EXPECT_EQ(probeNetworkCache(url, cache.path), 2u);
  EXPECT_EQ(std::distance(fs::directory_iterator(cache.path),
                          fs::directory_iterator{}),
            1);

  Hub offline;
  offline.setNetworkCacheDirectory(cache.path);
  offline.setNetworkPolicy(NetworkPolicy::Offline);
  EXPECT_EQ(offline.text(url), "ok");
}

// A cold ask for one URL may happen twice at once — the hub lets it, so
// that a slow fetch never stalls another thread. Both fetches persist,
// and one shared partial file would let each truncate what the other is
// writing. The transport holds both asks inside itself until the second
// arrives, so the two writes are guaranteed to overlap; what the cache
// directory is left holding is one whole file and nothing beside it.
TEST(IONetwork, TwoConcurrentFetchesOfOneUrlCommitOneWholeFile) {
  const ScratchDir cache("sigilio_net");
  const std::string url = "https://fake.invalid/wide.bin";
  // Long enough that one writer is still writing when the other starts.
  const std::string body(512 * 1024, 'x');
  Hub hub;
  hub.setNetworkCacheDirectory(cache.path);
  std::barrier inside(2);
  std::atomic<size_t> fetches{0};
  hub.setNetworkTransport([&](std::string_view) {
    ++fetches;
    inside.arrive_and_wait();
    std::vector<std::byte> bytes(body.size());
    std::memcpy(bytes.data(), body.data(), body.size());
    return std::optional<std::vector<std::byte>>{std::move(bytes)};
  });

  std::shared_ptr<const Bytes> fetched[2];
  std::thread askers[2];
  for (int i = 0; i != 2; ++i)
    askers[i] = std::thread([&, i] { fetched[i] = hub.blob(url); });
  for (std::thread& asker : askers) asker.join();

  EXPECT_EQ(fetches.load(), 2u);
  for (const auto& answer : fetched) {
    ASSERT_NE(answer, nullptr);
    EXPECT_EQ(answer->asText(), body);
  }

  // One file under the cache name, no partial left over, and every byte
  // of it there: neither writer wrote into the other's file.
  std::vector<std::string> left;
  for (const auto& entry : fs::directory_iterator(cache.path))
    left.push_back(entry.path().filename().string());
  EXPECT_EQ(left, std::vector<std::string>{detail::networkCacheKey(url)});

  Hub offline;
  offline.setNetworkCacheDirectory(cache.path);
  offline.setNetworkPolicy(NetworkPolicy::Offline);
  EXPECT_EQ(offline.text(url), body);
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
  online.setNetworkCacheDirectory(cache.path);
  auto fetched = online.blob(url);
  if (!fetched) GTEST_SKIP() << "no route to " << url;
  EXPECT_FALSE(fetched->bytes.empty());

  Hub offline;
  offline.setNetworkCacheDirectory(cache.path);
  offline.setNetworkPolicy(NetworkPolicy::Offline);
  auto replay = offline.blob(url);
  ASSERT_NE(replay, nullptr);
  EXPECT_EQ(replay->bytes, fetched->bytes);
}
