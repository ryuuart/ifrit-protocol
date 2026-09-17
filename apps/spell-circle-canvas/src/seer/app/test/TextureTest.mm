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
#include <sigilio/publish/Publisher.h>
#include <sigilio/publish/Subscription.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

#include "ScratchDir.h"
#include "texture/Capture.h"

namespace {

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
std::vector<std::byte> contentsOf(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  std::vector<std::byte> bytes;
  for (char byte = 0; stream.get(byte);) bytes.push_back((std::byte)byte);
  return bytes;
}

TEST(SeerTextureCapture, AFrameKeepsItsRowsAndItsChannels) {
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  if (!device) GTEST_SKIP() << "no Metal device on this machine";
  id<MTLCommandQueue> queue = [device newCommandQueue];
  ASSERT_TRUE(queue);

  MTLTextureDescriptor* description =
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
  ASSERT_TRUE(seer::texture::writeTexturePng(texture, queue, out));

  const std::vector<std::byte> written = contentsOf(out);
  ASSERT_FALSE(written.empty());
  const std::optional<sigil::image::ImageAsset> read =
      sigil::image::decodeImage(written.data(), written.size());
  ASSERT_TRUE(read);
  ASSERT_FALSE(read->frames().empty());
  const sk_sp<SkImage>& image = read->frames().front().image;
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

TEST(SeerTextureDelivery, AStaticFrameReachesClientsThatSubscribeAfterDrawingStops) {
  @autoreleasepool {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) GTEST_SKIP() << "no Metal device on this machine";
    id<MTLCommandQueue> queue = [device newCommandQueue];
    ASSERT_TRUE(queue);
    const std::string name = NSUUID.UUID.UUIDString.UTF8String;
    auto publisher = sigil::io::publish::createPublisher(name, sigil::io::publish::Backend::Metal,
                                                         (__bridge void*)device);
    ASSERT_TRUE(publisher);
    EXPECT_EQ(publisher->name(), name);

    MTLTextureDescriptor* description =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                           width:2
                                                          height:2
                                                       mipmapped:NO];
    description.storageMode = MTLStorageModeShared;
    id<MTLTexture> texture = [device newTextureWithDescriptor:description];
    ASSERT_TRUE(texture);
    [texture replaceRegion:MTLRegionMake2D(0, 0, 2, 2)
               mipmapLevel:0
                 withBytes:kQuadrants
               bytesPerRow:8];
    id<MTLCommandBuffer> commands = [queue commandBuffer];
    publisher->publishFrame((__bridge void*)texture, (__bridge void*)commands, 2, 2);
    [commands commit];
    [commands waitUntilCompleted];
    ASSERT_EQ(commands.status, MTLCommandBufferStatusCompleted);
    commands = nil;
    texture = nil;

    const sigil::test::ScratchDir scratch("publish_static_clients");
    const auto discoveryDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    bool listed = false;
    do {
      const auto offered = sigil::io::publish::publications();
      listed = std::any_of(offered.begin(), offered.end(),
                           [&](const auto& source) { return source.name == name; });
      if (listed) break;
      [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
    } while (std::chrono::steady_clock::now() < discoveryDeadline);
    ASSERT_TRUE(listed);
    for (int client = 0; client < 2; ++client) {
      auto incoming = sigil::io::publish::subscribe(name, "", (__bridge void*)device);
      ASSERT_TRUE(incoming);
      id<MTLTexture> received = nil;
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
      while (!received && std::chrono::steady_clock::now() < deadline) {
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
        received = (__bridge id<MTLTexture>)incoming->newestFrame();
      }
      ASSERT_TRUE(received) << "late client " << client;
      EXPECT_TRUE(incoming->standing());
      const auto out = scratch.path / (std::to_string(client) + ".png");
      ASSERT_TRUE(seer::texture::writeTexturePng(received, queue, out));
      const auto written = contentsOf(out);
      const auto decoded = sigil::image::decodeImage(written.data(), written.size());
      ASSERT_TRUE(decoded);
      const auto& image = decoded->frames().front().image;
      ASSERT_EQ(image->width(), 2);
      ASSERT_EQ(image->height(), 2);
      SkBitmap pixels;
      ASSERT_TRUE(pixels.tryAllocPixels(
          SkImageInfo::Make(2, 2, kBGRA_8888_SkColorType, kPremul_SkAlphaType)));
      ASSERT_TRUE(image->readPixels(nullptr, pixels.pixmap(), 0, 0));
      for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x) EXPECT_EQ(*pixels.getAddr32(x, y), kQuadrants[y * 2 + x]);
    }
  }
}

}  // namespace
