/** @file
 * Mounts and what a URI resolves to: the longest matching prefix, bytes
 * and text through a mount, a file:// URI stripped to a plain path, the
 * hub answering as a ByteSource, and the catalogue that is one prefix
 * over one directory.
 */

#include <gtest/gtest.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/hub/TextCatalog.h>

#include <filesystem>
#include <string>
#include <vector>

#include "MountedHub.h"

using namespace sigil::io;
using sigil::test::ScratchDir;
namespace fs = std::filesystem;

// The hub is the reference ByteSource: the source concepts are
// what every consumer writes against, so the hub must satisfy them.
static_assert(ByteSource<Hub>);
static_assert(ResolvingByteSource<Hub>);

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

TEST_F(IOHub, MountsResolveLongestPrefix) {
  hub.mount("res://deep/", dir.path / "elsewhere");
  EXPECT_EQ(hub.resolve("res://a.txt"), dir.path / "a.txt");
  EXPECT_EQ(hub.resolve("res://deep/b.txt"), dir.path / "elsewhere" / "b.txt");
  EXPECT_TRUE(hub.resolve("other://x").empty());
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

TEST_F(IOHub, MissingFilesHealWithoutStaleCache) {
  EXPECT_EQ(hub.text("res://late.txt"), std::nullopt);
  dir.write("late.txt", "arrived");
  EXPECT_EQ(hub.text("res://late.txt"), "arrived");
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

TEST(IOTextCatalog, MountsOneDirectoryAndAnswersByName) {
  const ScratchDir shaders("sigilio_catalog");
  shaders.write("Glow.sksl", "half4 main(float2 p) { return half4(1); }");
  shaders.write("nested/Mask.sksl", "mask");
  TextCatalog catalog("shader://glow/", shaders.path);
  EXPECT_EQ(catalog.prefix(), "shader://glow/");
  EXPECT_EQ(catalog.preload(), 2u);
  EXPECT_EQ(catalog.text("nested/Mask.sksl"), "mask");
  EXPECT_EQ(catalog.text("Missing.sksl"), std::nullopt);
  // The same cache the hub's own asks use.
  EXPECT_EQ(catalog.hub().text("shader://glow/nested/Mask.sksl"), "mask");
}
