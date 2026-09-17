#include <gtest/gtest.h>

#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

#include "Arguments.h"

namespace {
using seer::Arguments;

/** The command line as a caller types it, with the program's own name in
 *  front of it the way the platform hands it over. */
std::optional<Arguments> parse(std::initializer_list<const char*> words) {
  std::vector<std::string> owned{"Seer"};
  owned.insert(owned.end(), words.begin(), words.end());
  std::vector<char*> argv;
  argv.reserve(owned.size());
  for (std::string& word : owned) argv.push_back(word.data());
  return seer::parseArguments((int)argv.size(), argv.data());
}

TEST(SeerArguments, TheListingTakesNothingButItself) {
  const std::optional<Arguments> listing = parse({"--list-textures"});
  ASSERT_TRUE(listing);
  EXPECT_TRUE(listing->listTextures);
  EXPECT_TRUE(listing->texture.empty());

  EXPECT_FALSE(parse({"--list-textures", "smoke"}));
  EXPECT_FALSE(parse({"--list-textures", "--timeout", "2"}));
}

TEST(SeerArguments, ANameOnItsOwnOpensTheWindow) {
  const std::optional<Arguments> window = parse({"--texture", "smoke"});
  ASSERT_TRUE(window);
  EXPECT_EQ(window->texture, "smoke");
  EXPECT_TRUE(window->grabPath.empty());
  EXPECT_FALSE(window->listTextures);
  EXPECT_FALSE(window->frames);
  EXPECT_FALSE(window->timeoutSeconds);
}

TEST(SeerArguments, TheApplicationNarrowsOneName) {
  const std::optional<Arguments> window =
      parse({"--texture", "smoke", "--app", "Sketchbook"});
  ASSERT_TRUE(window);
  EXPECT_EQ(window->texture, "smoke");
  EXPECT_EQ(window->application, "Sketchbook");
}

TEST(SeerArguments, AGrabNamesItsFileAndHowLongItWaits) {
  const std::optional<Arguments> grab =
      parse({"--texture", "smoke", "--grab", "/tmp/received.png", "--frames",
             "4", "--timeout", "2.5"});
  ASSERT_TRUE(grab);
  EXPECT_EQ(grab->texture, "smoke");
  EXPECT_EQ(grab->grabPath, "/tmp/received.png");
  ASSERT_TRUE(grab->frames);
  EXPECT_EQ(*grab->frames, 4);
  ASSERT_TRUE(grab->timeoutSeconds);
  EXPECT_DOUBLE_EQ(*grab->timeoutSeconds, 2.5);
}

TEST(SeerArguments, WhatCannotBeAnsweredIsRefused) {
  // Nothing named to subscribe to, a flag nobody here takes, a count that
  // is not a number, a wait of no time at all, and a wait asked of a
  // window, which draws for as long as it is open and waits for nothing.
  EXPECT_FALSE(parse({"--texture"}));
  EXPECT_FALSE(parse({"--texture", "smoke", "--sketch", "hello"}));
  EXPECT_FALSE(
      parse({"--texture", "smoke", "--grab", "out.png", "--frames", "none"}));
  EXPECT_FALSE(
      parse({"--texture", "smoke", "--grab", "out.png", "--timeout", "0"}));
  EXPECT_FALSE(parse({"--texture", "smoke", "--frames", "2"}));
}

TEST(SeerArguments, EmptyLaunchOpensWithoutConnections) {
  const auto args = parse({});
  ASSERT_TRUE(args);
  EXPECT_TRUE(args->wires.empty());
  EXPECT_TRUE(args->receiver.empty());
  EXPECT_TRUE(args->texture.empty());
}

TEST(SeerArguments, WireAndTextureWindowsCanBeCombined) {
  const auto args =
      parse({"udp://:27020", "--texture", "Canvas", "--shot", "window.png"});
  ASSERT_TRUE(args);
  EXPECT_EQ(args->wires, std::vector<std::string>{"udp://:27020"});
  EXPECT_EQ(args->texture, "Canvas");
  EXPECT_TRUE(args->textures);
  EXPECT_EQ(args->shot, "window.png");
}

TEST(SeerArguments, CaptureRejectsWindowOptionsAndInvalidCounts) {
  EXPECT_FALSE(
      parse({"--texture", "Canvas", "--grab", "frame.png", "udp://:27020"}));
  EXPECT_FALSE(
      parse({"--texture", "Canvas", "--grab", "frame.png", "--frames", "1.5"}));
  EXPECT_FALSE(parse({"--texture", "Canvas", "--grab", "frame.png", "--frames",
                      "999999999999"}));
  EXPECT_FALSE(parse(
      {"--texture", "Canvas", "--grab", "frame.png", "--timeout", "inf"}));
  EXPECT_FALSE(parse(
      {"--texture", "Canvas", "--grab", "frame.png", "--timeout", "nan"}));
  EXPECT_FALSE(parse({"--grab", "frame.png"}));
  EXPECT_FALSE(parse({"--app", "Sketchbook"}));
}

TEST(SeerArguments, SendingRequiresAPeer) {
  EXPECT_FALSE(parse({"--say", "hello"}));
  const auto args =
      parse({"--peer", "udp://127.0.0.1:27020", "--say", "hello"});
  ASSERT_TRUE(args);
  ASSERT_TRUE(args->message);
  EXPECT_EQ(*args->message, "hello");
}

TEST(SeerArguments, MessageTextMayStartWithAHyphen) {
  const auto args = parse({"--peer", "udp://127.0.0.1:27020", "--say", "-12"});
  ASSERT_TRUE(args);
  ASSERT_TRUE(args->message);
  EXPECT_EQ(*args->message, "-12");
}

}  // namespace
