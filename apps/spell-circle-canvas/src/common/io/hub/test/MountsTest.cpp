/** @file
 * Mounts and what a URI resolves to: the longest matching prefix, bytes
 * and text through a mount, a file:// URI stripped to a plain path, the
 * hub answering as a ByteSource, the namespace a mount is — nothing
 * above its directory and no directory answered as bytes — and the
 * catalogue that is one prefix over one directory.
 */

#include <gtest/gtest.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/advanced/Decoding.h>
#include <sigilio/advanced/Places.h>

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
  auto fetched = hub.read("res://notes/hello.txt");
  ASSERT_NE(fetched, nullptr);
  EXPECT_EQ(fetched->asText(), "carry the coal");
  EXPECT_EQ(fetched, hub.read("res://notes/hello.txt"));

  AnyByteSource any(hub);
  ASSERT_TRUE(any);
  EXPECT_EQ(any.read("res://notes/hello.txt"), fetched);
  EXPECT_EQ(any.resolve("res://notes/hello.txt"),
            dir.path / "notes" / "hello.txt");
  EXPECT_EQ(any.read("res://missing.txt"), nullptr);
  EXPECT_FALSE(AnyByteSource{});
}

TEST_F(IOHub, MountsResolveLongestPrefix) {
  mount(hub, "res://deep/", dir.path / "elsewhere");
  EXPECT_EQ(resolve(hub, "res://a.txt"), dir.path / "a.txt");
  EXPECT_EQ(resolve(hub, "res://deep/b.txt"), dir.path / "elsewhere" / "b.txt");
  EXPECT_TRUE(resolve(hub, "other://x").empty());
}

TEST_F(IOHub, FetchAndTextLoadThroughMounts) {
  dir.write("notes/hello.txt", "carry the coal");
  auto text = hub.text("res://notes/hello.txt");
  ASSERT_TRUE(text.has_value());
  EXPECT_EQ(*text, "carry the coal");
  auto bytes = hub.read("res://notes/hello.txt");
  ASSERT_NE(bytes, nullptr);
  EXPECT_EQ(bytes->size(), 14u);
  EXPECT_EQ(hub.read("res://missing.bin"), nullptr);
}

TEST_F(IOHub, MissingFilesHealWithoutStaleCache) {
  EXPECT_EQ(hub.text("res://late.txt"), std::nullopt);
  dir.write("late.txt", "arrived");
  EXPECT_EQ(hub.text("res://late.txt"), "arrived");
}

// A mount is a namespace and not a door into the filesystem around it.
// The file outside the mounted directory is the trap: it exists, and a
// remainder that climbs out through `..` still names nothing, on every
// path a URI takes.
TEST_F(IOHub, AMountNamesNothingAboveItsDirectory) {
  const ScratchDir above("sigilio_above");
  ScratchDir inside("sigilio_inside");
  inside.write("secret.txt", "not through this mount");
  above.write("sibling.txt", "not through this mount either");
  mount(hub, "res://inside/", inside.path);

  const std::string climbing =
      "res://inside/../" + above.path.filename().string() + "/sibling.txt";
  EXPECT_TRUE(resolve(hub, climbing).empty());
  EXPECT_EQ(hub.read(climbing), nullptr);
  EXPECT_EQ(hub.text("res://inside/../inside/secret.txt"), std::nullopt);
  EXPECT_TRUE(select(hub, "res://inside/../*/*.txt").empty());
  EXPECT_TRUE(select(hub, "res://inside/..").empty());
  // The same names, without the climb, are there to be had.
  EXPECT_EQ(hub.text("res://inside/secret.txt"), "not through this mount");
}

// The hub answers bytes, and a directory has none. It has a write time
// like any other filesystem entry, which is what a check that asked only
// whether something is there would have taken for a resource.
TEST_F(IOHub, ADirectoryAnswersNoBytes) {
  dir.write("shaders/a.sksl", "a");
  EXPECT_EQ(hub.read("res://shaders"), nullptr);
  EXPECT_EQ(hub.read("res://shaders/"), nullptr);
  EXPECT_EQ(hub.text("res://shaders"), std::nullopt);
  EXPECT_EQ(hub.load<sigil::image::ImageAsset>("res://shaders"), nullptr);
  EXPECT_FALSE(probe<ResourceInfo>(hub, "res://shaders").has_value());
  // The file beneath it still answers, so nothing was refused wholesale.
  EXPECT_EQ(hub.text("res://shaders/a.sksl"), "a");
}

TEST_F(IOHub, FileUrlsLoadAsLocalPaths) {
  // Nothing about the mount is used: a file:// URI strips to a plain
  // local path.
  dir.write("direct.txt", "no mount needed");
  const std::string url = "file://" + (dir.path / "direct.txt").string();
  auto text = hub.text(url);
  ASSERT_TRUE(text.has_value());
  EXPECT_EQ(*text, "no mount needed");
  auto bytes = hub.read(url);
  ASSERT_NE(bytes, nullptr);
  EXPECT_EQ(bytes->size(), 15u);
}
