/** @file
 * Preloading and retention: bytes fetched ahead of the first ask, the
 * lease that says how long the hub keeps them, the selector snapshots a
 * lease refreshes, and what a discard of everything unretained leaves
 * standing.
 */

#include <gtest/gtest.h>
#include <sigilcore/schedule/ConcurrentIo.h>
#include <sigilio/hub/Hub.h>

#include <filesystem>
#include <string>
#include <vector>

#include "MountedHub.h"

using namespace sigil::io;
using sigil::io::test::leaseUris;
using sigil::test::ScratchDir;
namespace fs = std::filesystem;

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

/** More resources than a preload fetches at one time, so the batch is
 *  the concurrent path and not the single-item one a handful degenerates
 *  to. Every resource still lands in the cache under its own URI, which
 *  is what a fan-out that mixed two asks up would fail. */
TEST_F(IOHub, PreloadFetchesMoreResourcesThanItFetchesAtOnce) {
  const size_t count = sigil::core::schedule::concurrentIoWidth() * 4 + 3;
  std::vector<std::string> uris;
  for (size_t i = 0; i != count; ++i) {
    const std::string name = "many/" + std::to_string(i) + ".txt";
    dir.write(name, std::to_string(i));
    uris.push_back("res://" + name);
  }
  std::vector<std::string_view> asked(uris.begin(), uris.end());

  EXPECT_EQ(hub.preload(asked), count);

  // Written over afterwards: what comes back is what the preload read,
  // so a URI that was never fetched shows up as the newer text.
  for (size_t i = 0; i != count; ++i)
    dir.write("many/" + std::to_string(i) + ".txt", "after the preload");
  for (size_t i = 0; i != count; ++i)
    EXPECT_EQ(hub.text(uris[i]), std::to_string(i));
}

TEST_F(IOHub, PreloadingADirectoryDiscoversNestedResourcesByUri) {
  dir.write("shaders/a.sksl", "a");
  dir.write("shaders/nested/b.slang", "b");
  hub.mount("shader://", dir.path / "shaders");
  EXPECT_EQ(hub.preload("shader://"), 2u);

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
  ScratchDir plugins("sigilio_plugins");
  dir.write("material/a.sksl", "material");
  dir.write("material/ignored.slang", "ignored");
  plugins.write("compose/nested/b.slang", "compose");
  dir.write("loose.txt", "loose");
  hub.mount("plugin://", plugins.path);
  ResourceLease shaders =
      hub.retain({"res://material/**/*.sksl", "plugin://**/*.slang"});

  EXPECT_EQ(leaseUris(shaders),
            (std::vector<std::string>{"plugin://compose/nested/b.slang",
                                      "res://material/a.sksl"}));
  EXPECT_EQ(shaders.preload(), 2u);
  auto loose = hub.blob("res://loose.txt");
  ASSERT_NE(loose, nullptr);

  dir.write("material/a.sksl", "changed material");
  plugins.write("compose/nested/b.slang", "changed compose");
  dir.write("loose.txt", "changed loose");
  EXPECT_EQ(hub.discardUnretained(), 1u);
  EXPECT_EQ(hub.text("res://material/a.sksl"), "material");
  EXPECT_EQ(hub.text("plugin://compose/nested/b.slang"), "compose");
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
