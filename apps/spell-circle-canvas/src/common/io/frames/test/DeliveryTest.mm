/** @file
 * A publication carried from one end to the other on this machine: a
 * frame published once over a `syphon://` name is listed by name, and
 * every subscriber that joins after drawing has stopped receives that
 * same frame — its rows in the order the carried surface holds them,
 * the picture's bottom first, and its channels in the order the
 * surface's own pixel format names them.
 *
 * SKIPS where there is no Metal device.
 */

#import <CoreFoundation/CoreFoundation.h>
#import <Metal/Metal.h>

#include <gtest/gtest.h>
#include <sigilio/frames/Publisher.h>
#include <sigilio/frames/Subscription.h>
#include <sigilio/hub/Hub.h>

#include <unistd.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace frames = sigil::io::frames;
using namespace std::chrono_literals;

namespace {

/** One pixel as red, green, blue and alpha, whatever order the bytes
 *  under it were in. */
struct Color {
  uint8_t red = 0;
  uint8_t green = 0;
  uint8_t blue = 0;
  uint8_t alpha = 0;
  bool operator==(const Color&) const = default;
};

std::ostream& operator<<(std::ostream& stream, const Color& color) {
  return stream << "(" << int(color.red) << ", " << int(color.green) << ", "
                << int(color.blue) << ", " << int(color.alpha) << ")";
}

/** Four colours no channel order can mistake for one another: each
 *  differs from every other in the red and the blue channel both, or in
 *  green. */
constexpr Color kRed{255, 0, 0, 255};
constexpr Color kGreen{0, 255, 0, 255};
constexpr Color kBlue{0, 0, 255, 255};
constexpr Color kYellow{255, 255, 0, 255};

/** Turns this thread's run loop until @p ready holds, or gives up. The
 *  publication directory hears of a server, and a client hears of its
 *  frame, through notifications delivered on the run loop, and a test
 *  binary turns none of its own. */
bool turnUntil(const std::function<bool()>& ready) {
  const auto deadline = std::chrono::steady_clock::now() + 10s;
  while (std::chrono::steady_clock::now() < deadline) {
    if (ready()) return true;
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
  }
  return ready();
}

/** A two-by-two texture holding @p rows, first row first, in the
 *  memory order of BGRA8Unorm. */
id<MTLTexture> quadrants(id<MTLDevice> device,
                         const std::array<std::array<Color, 2>, 2>& rows) {
  MTLTextureDescriptor* described =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                         width:2
                                                        height:2
                                                     mipmapped:NO];
  described.storageMode = MTLStorageModeShared;
  described.usage = MTLTextureUsageShaderRead;
  id<MTLTexture> texture = [device newTextureWithDescriptor:described];
  std::vector<uint8_t> texels;
  for (const auto& row : rows)
    for (const Color& color : row)
      texels.insert(texels.end(), {color.blue, color.green, color.red, color.alpha});
  [texture replaceRegion:MTLRegionMake2D(0, 0, 2, 2)
             mipmapLevel:0
               withBytes:texels.data()
             bytesPerRow:2 * 4];
  return texture;
}

/** @p texture's pixels, first row first, each read in the channel order
 *  its pixel format names; nothing for a format this does not read. The
 *  arrival stands on a surface the CPU may not see, so it is copied into
 *  a buffer the CPU does first. */
std::optional<std::vector<std::vector<Color>>> pixelsOf(id<MTLDevice> device,
                                                        id<MTLCommandQueue> queue,
                                                        id<MTLTexture> texture) {
  const bool blueFirst = texture.pixelFormat == MTLPixelFormatBGRA8Unorm ||
                         texture.pixelFormat == MTLPixelFormatBGRA8Unorm_sRGB;
  const bool redFirst = texture.pixelFormat == MTLPixelFormatRGBA8Unorm ||
                        texture.pixelFormat == MTLPixelFormatRGBA8Unorm_sRGB;
  if (!blueFirst && !redFirst) return std::nullopt;
  const NSUInteger width = texture.width;
  const NSUInteger height = texture.height;
  const NSUInteger bytesPerRow = width * 4;
  id<MTLBuffer> buffer = [device newBufferWithLength:bytesPerRow * height
                                             options:MTLResourceStorageModeShared];
  id<MTLCommandBuffer> commands = [queue commandBuffer];
  id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
  [blit copyFromTexture:texture
                   sourceSlice:0
                   sourceLevel:0
                  sourceOrigin:MTLOriginMake(0, 0, 0)
                    sourceSize:MTLSizeMake(width, height, 1)
                      toBuffer:buffer
             destinationOffset:0
        destinationBytesPerRow:bytesPerRow
      destinationBytesPerImage:bytesPerRow * height];
  [blit endEncoding];
  [commands commit];
  [commands waitUntilCompleted];

  const auto* bytes = static_cast<const uint8_t*>(buffer.contents);
  std::vector<std::vector<Color>> rows(height);
  for (NSUInteger row = 0; row < height; ++row)
    for (NSUInteger column = 0; column < width; ++column) {
      const uint8_t* pixel = bytes + row * bytesPerRow + column * 4;
      rows[row].push_back(blueFirst ? Color{pixel[2], pixel[1], pixel[0], pixel[3]}
                                    : Color{pixel[0], pixel[1], pixel[2], pixel[3]});
    }
  return rows;
}

TEST(FramesDelivery, SubscribersThatJoinAfterDrawingStopsEachReceiveTheStaticFrame) {
  id<MTLDevice> device = (__bridge id<MTLDevice>)frames::defaultMetalDevice();
  if (!device) GTEST_SKIP() << "no Metal device";
  id<MTLCommandQueue> queue = [device newCommandQueue];
  ASSERT_NE(queue, nil);

  // A name no other run on this machine publishes under.
  const std::string name = "io_test quadrants " + std::to_string(getpid());
  const std::string uri = "syphon://" + name;
  sigil::io::Hub hub;
  const frames::Publisher publisher =
      hub.publish(uri, {.device = {.handle = (__bridge void*)device}});
  ASSERT_TRUE(publisher);

  // Drawn top-first, as a canvas draws: red and green along the top,
  // blue and yellow along the bottom. Published once, and then nothing
  // more is drawn.
  id<MTLTexture> drawn = quadrants(device, {{{kRed, kGreen}, {kBlue, kYellow}}});
  ASSERT_NE(drawn, nil);
  id<MTLCommandBuffer> commands = [queue commandBuffer];
  ASSERT_TRUE(publisher.send({.texture = (__bridge void*)drawn,
                              .commandBuffer = (__bridge void*)commands,
                              .width = 2,
                              .height = 2}));
  [commands commit];
  [commands waitUntilCompleted];

  // Found by its name through the directory.
  const auto listed = [&] {
    for (const frames::Publication& publication : frames::publications())
      if (publication.name == name) return true;
    return false;
  };
  ASSERT_TRUE(turnUntil(listed)) << name << " was never listed";

  // Two subscribers join now, after the drawing has stopped.
  std::array<frames::Subscription, 2> subscribers = {
      hub.subscribe(uri, {.device = {.handle = (__bridge void*)device}}),
      hub.subscribe(uri, {.device = {.handle = (__bridge void*)device}})};
  for (size_t index = 0; index < subscribers.size(); ++index) {
    SCOPED_TRACE("subscriber " + std::to_string(index));
    const frames::Subscription& subscriber = subscribers[index];
    ASSERT_TRUE(subscriber);
    std::optional<frames::Frame> frame;
    ASSERT_TRUE(turnUntil([&] {
      frame = subscriber.latest();
      return frame.has_value();
    })) << "no frame arrived";
    EXPECT_TRUE(subscriber.state().isOpen());
    ASSERT_EQ(frame->width, 2);
    ASSERT_EQ(frame->height, 2);

    const std::optional<std::vector<std::vector<Color>>> pixels =
        pixelsOf(device, queue, (__bridge id<MTLTexture>)frame->texture);
    ASSERT_TRUE(pixels.has_value())
        << "a pixel format this test does not read: "
        << ((__bridge id<MTLTexture>)frame->texture).pixelFormat;
    // The carried surface holds the picture's BOTTOM row first, and
    // every channel where the picture put it.
    EXPECT_EQ((*pixels)[0], (std::vector<Color>{kBlue, kYellow}));
    EXPECT_EQ((*pixels)[1], (std::vector<Color>{kRed, kGreen}));
  }
}

}  // namespace
