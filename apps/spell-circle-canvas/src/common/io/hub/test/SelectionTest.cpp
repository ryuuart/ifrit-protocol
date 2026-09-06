/** @file
 * Selection: one selector into a sorted, duplicate-free URI snapshot —
 * exact files, recursive directories, segment-aware globs, the mount a
 * URI belongs to when mounts overlap, file URLs and plain paths, and
 * the network URL that selects itself but cannot be globbed.
 */

#include <gtest/gtest.h>
#include <sigilio/hub/Hub.h>

#include <filesystem>
#include <string>
#include <vector>

#include "MountedHub.h"

using namespace sigil::io;
using sigil::test::ScratchDir;
namespace fs = std::filesystem;

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
