/** @file
 * What the receiver can be asked with nothing publishing: the command
 * line, and the file a frame is written to — checked over a texture this
 * case drew itself, because what the write has to keep is the texture's
 * own row order and channel order and neither of those is the protocol's.
 */

#import <Metal/Metal.h>

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <sigilimage/decode/Decode.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

#include "ScratchDir.h"
#include "receiver/Arguments.h"
#include "receiver/Capture.h"

namespace {

using receiver::Arguments;

/** The command line as a caller types it, with the program's own name in
 *  front of it the way the platform hands it over. */
std::optional<Arguments> parse(std::initializer_list<const char *> words) {
  std::vector<std::string> owned{"Receiver"};
  owned.insert(owned.end(), words.begin(), words.end());
  std::vector<char *> argv;
  argv.reserve(owned.size());
  for (std::string &word : owned) argv.push_back(word.data());
  return receiver::parseArguments((int)argv.size(), argv.data());
}

TEST(ReceiverArguments, TheListingTakesNothingButItself) {
  const std::optional<Arguments> listing = parse({"--list"});
  ASSERT_TRUE(listing);
  EXPECT_TRUE(listing->list);
  EXPECT_TRUE(listing->server.empty());

  EXPECT_FALSE(parse({"--list", "smoke"}));
  EXPECT_FALSE(parse({"--list", "--timeout", "2"}));
}

TEST(ReceiverArguments, ANameOnItsOwnOpensTheWindow) {
  const std::optional<Arguments> window = parse({"smoke"});
  ASSERT_TRUE(window);
  EXPECT_EQ(window->server, "smoke");
  EXPECT_TRUE(window->grabPath.empty());
  EXPECT_FALSE(window->list);
  EXPECT_FALSE(window->frames);
  EXPECT_FALSE(window->timeoutSeconds);
}

TEST(ReceiverArguments, TheApplicationNarrowsOneName) {
  const std::optional<Arguments> window = parse({"smoke", "--app", "Sketchbook"});
  ASSERT_TRUE(window);
  EXPECT_EQ(window->server, "smoke");
  EXPECT_EQ(window->app, "Sketchbook");
}

TEST(ReceiverArguments, AGrabNamesItsFileAndHowLongItWaits) {
  const std::optional<Arguments> grab =
      parse({"smoke", "--grab", "/tmp/received.png", "--frames", "4", "--timeout", "2.5"});
  ASSERT_TRUE(grab);
  EXPECT_EQ(grab->server, "smoke");
  EXPECT_EQ(grab->grabPath, "/tmp/received.png");
  ASSERT_TRUE(grab->frames);
  EXPECT_EQ(*grab->frames, 4);
  ASSERT_TRUE(grab->timeoutSeconds);
  EXPECT_DOUBLE_EQ(*grab->timeoutSeconds, 2.5);
}

TEST(ReceiverArguments, WhatCannotBeAnsweredIsRefused) {
  // Nothing named to subscribe to, a flag nobody here takes, a count that
  // is not a number, a wait of no time at all, and a wait asked of a
  // window, which draws for as long as it is open and waits for nothing.
  EXPECT_FALSE(parse({}));
  EXPECT_FALSE(parse({"smoke", "--sketch", "hello"}));
  EXPECT_FALSE(parse({"smoke", "--grab", "out.png", "--frames", "none"}));
  EXPECT_FALSE(parse({"smoke", "--grab", "out.png", "--timeout", "0"}));
  EXPECT_FALSE(parse({"smoke", "--frames", "2"}));
}

/** Four quadrants in the order a BGRA texture holds them — blue, green,
 *  red, white, each opaque — so a write that transposed the image,
 *  flipped it, or read the channels the other way round says which. */
constexpr uint32_t kQuadrants[4] = {
    0xff0000ffu,
    0xff00ff00u,
    0xffff0000u,
    0xffffffffu,
};

/** Whatever stands in @p path, as bytes. */
std::vector<std::byte> contentsOf(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary);
  std::vector<std::byte> bytes;
  for (char byte = 0; stream.get(byte);) bytes.push_back((std::byte)byte);
  return bytes;
}

TEST(ReceiverCapture, AFrameKeepsItsRowsAndItsChannels) {
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  if (!device) GTEST_SKIP() << "no Metal device on this machine";
  id<MTLCommandQueue> queue = [device newCommandQueue];
  ASSERT_TRUE(queue);

  MTLTextureDescriptor *description =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                         width:2
                                                        height:2
                                                     mipmapped:NO];
  description.storageMode = MTLStorageModeManaged;
  id<MTLTexture> texture = [device newTextureWithDescriptor:description];
  ASSERT_TRUE(texture);
  [texture replaceRegion:MTLRegionMake2D(0, 0, 2, 2)
             mipmapLevel:0
               withBytes:kQuadrants
             bytesPerRow:2 * 4];

  const sigil::test::ScratchDir scratch("receiver_capture");
  const std::filesystem::path out = scratch.path / "frame.png";
  ASSERT_TRUE(receiver::writeTexturePng(texture, queue, out));

  const std::vector<std::byte> written = contentsOf(out);
  ASSERT_FALSE(written.empty());
  const std::optional<sigil::image::ImageAsset> read =
      sigil::image::decodeImage(written.data(), written.size());
  ASSERT_TRUE(read);
  ASSERT_FALSE(read->frames().empty());
  const sk_sp<SkImage> &image = read->frames().front().image;
  ASSERT_EQ(image->width(), 2);
  ASSERT_EQ(image->height(), 2);

  SkBitmap pixels;
  ASSERT_TRUE(
      pixels.tryAllocPixels(SkImageInfo::Make(2, 2, kBGRA_8888_SkColorType, kPremul_SkAlphaType)));
  ASSERT_TRUE(image->readPixels(nullptr, pixels.pixmap(), 0, 0));
  for (int y = 0; y < 2; ++y)
    for (int x = 0; x < 2; ++x)
      EXPECT_EQ(*pixels.getAddr32(x, y), kQuadrants[y * 2 + x]) << "at " << x << "," << y;
}

}  // namespace
