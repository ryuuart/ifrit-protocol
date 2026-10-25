/** @file The native decoded-frame path into a Metal Graphite recorder, and
 *  the host copy a recorder that cannot sample the frame is handed. */

#import <Metal/Metal.h>

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkRect.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilgeometry/device/Device.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/video/Encoder.h>
#include <sigilmedia/video/Video.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>

namespace media = sigil::media;
using namespace std::chrono_literals;

TEST(VideoDevice, VideoToolboxFrameWrapsAsGraphiteYuvaImage) {
  constexpr int kWidth = 96;
  constexpr int kHeight = 64;
  media::Encoder encoder({.width = kWidth,
                          .height = kHeight,
                          .framesPerSecond = 10,
                          .bitRate = 500'000,
                          .hardware = media::HardwarePreference::Disabled});
  ASSERT_TRUE(encoder) << encoder.error();
  SkBitmap pixels;
  pixels.allocPixels(SkImageInfo::MakeN32Premul(kWidth, kHeight));
  pixels.eraseColor(SK_ColorMAGENTA);
  ASSERT_TRUE(append(encoder, pixels.pixmap()));
  pixels.eraseColor(SK_ColorCYAN);
  ASSERT_TRUE(append(encoder, pixels.pixmap()));
  const std::vector<std::byte> encoded = encoder.finish().value_or(std::vector<std::byte>{});
  ASSERT_FALSE(encoded.empty()) << encoder.error();

  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  if (!device) GTEST_SKIP() << "no Metal device";
  id<MTLCommandQueue> queue = [device newCommandQueue];
  ASSERT_NE(queue, nil);
  std::unique_ptr<sigil::skia::GraphiteContext> graphite =
      sigil::skia::GraphiteContext::createMetal((__bridge void*)device, (__bridge void*)queue);
  ASSERT_NE(graphite, nullptr);

  const auto clip = media::decode<media::Video>(
      encoded, {.cachedFrames = 2, .hardware = media::HardwarePreference::Preferred}, "device.mp4");
  ASSERT_NE(clip, nullptr);
  const media::Frame first = clip->frameAt(20ms);
  if (first.device.kind != media::DeviceFrame::Kind::PixelBuffer)
    GTEST_SKIP() << "VideoToolbox decoder unavailable in this session";
  const media::Frame second = clip->frameAt(120ms);
  ASSERT_EQ(second.device.kind, media::DeviceFrame::Kind::PixelBuffer);
  const media::Frame secondAgain = clip->frameAt(120ms);

  // A device frame carries no raster: it is bound for the recorder that
  // draws it, once per recorder.
  EXPECT_EQ(first.image, nullptr);
  const sk_sp<SkImage> firstImage = media::deviceImage(first, graphite->recorder());
  const sk_sp<SkImage> secondImage = media::deviceImage(second, graphite->recorder());
  ASSERT_NE(firstImage, nullptr);
  ASSERT_NE(secondImage, nullptr);
  EXPECT_TRUE(firstImage->isTextureBacked());
  EXPECT_TRUE(secondImage->isTextureBacked());
  EXPECT_NE(first.index, second.index);
  EXPECT_EQ(secondImage.get(), media::deviceImage(secondAgain, graphite->recorder()).get());
  // With no recorder the same frame reads back to the CPU.
  const sk_sp<SkImage> readBack = media::deviceImage(first, nullptr);
  ASSERT_NE(readBack, nullptr);
  EXPECT_FALSE(readBack->isTextureBacked());

  const SkImageInfo info = SkImageInfo::MakeN32Premul(kWidth * 4, kHeight * 4);
  const sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(graphite->recorder(), info);
  ASSERT_NE(surface, nullptr);
  // Both decoded frames are drawn, each scaled and offset, because a
  // device plane that only composites at its own size and origin would
  // still pass a single one-to-one blit.
  surface->getCanvas()->drawImageRect(firstImage, SkRect::MakeXYWH(0, 0, kWidth * 3, kHeight * 2),
                                      SkSamplingOptions());
  surface->getCanvas()->drawImageRect(
      secondImage, SkRect::MakeXYWH(kWidth, kHeight, kWidth * 2, kHeight * 3), SkSamplingOptions());
  std::unique_ptr<skgpu::graphite::Recording> recording = graphite->recorder()->snap();
  ASSERT_NE(recording, nullptr);
  skgpu::graphite::InsertRecordingInfo insert;
  insert.fRecording = recording.get();
  ASSERT_TRUE(graphite->context()->insertRecording(insert));
  EXPECT_TRUE(graphite->context()->submit(skgpu::graphite::SyncToCpu::kYes));
}

TEST(VideoDevice, ARecorderThatCannotSampleTheFrameIsHandedItsHostCopy) {
  constexpr int kWidth = 96;
  constexpr int kHeight = 64;
  media::Encoder encoder({.width = kWidth,
                          .height = kHeight,
                          .framesPerSecond = 10,
                          .bitRate = 500'000,
                          .hardware = media::HardwarePreference::Disabled});
  ASSERT_TRUE(encoder) << encoder.error();
  SkBitmap pixels;
  pixels.allocPixels(SkImageInfo::MakeN32Premul(kWidth, kHeight));
  pixels.eraseColor(SK_ColorMAGENTA);
  ASSERT_TRUE(append(encoder, pixels.pixmap()));
  ASSERT_TRUE(append(encoder, pixels.pixmap()));
  const std::vector<std::byte> encoded = encoder.finish().value_or(std::vector<std::byte>{});
  ASSERT_FALSE(encoded.empty()) << encoder.error();

  const auto clip = media::decode<media::Video>(
      encoded, {.cachedFrames = 2, .hardware = media::HardwarePreference::Preferred}, "device.mp4");
  ASSERT_NE(clip, nullptr);
  const media::Frame frame = clip->frameAt(20ms);
  if (frame.device.kind != media::DeviceFrame::Kind::PixelBuffer)
    GTEST_SKIP() << "VideoToolbox decoder unavailable in this session";

  // Graphite on a Vulkan device: a recorder that cannot sample the Metal
  // planes the decoder holds.
  std::string error;
  static const std::unique_ptr<sigil::geometry::device::Device> vulkan =
      [&error] {
        auto created = sigil::geometry::device::Device::create({});
        if (!created) error = created.error().message;
        return std::move(created).value_or(nullptr);
      }();
  if (!vulkan || !vulkan->gpu()) GTEST_SKIP() << "no Vulkan device: " << error;
  // A Vulkan Graphite made over the device: the recorder that cannot
  // sample a Metal plane, whatever context the device paints with.
  static const std::unique_ptr<sigil::skia::GraphiteContext> graphiteOverVulkan =
      sigil::skia::GraphiteContext::create(*vulkan->gpu());
  if (!graphiteOverVulkan) GTEST_SKIP() << "no Vulkan Graphite over the device";
  skgpu::graphite::Recorder* recorder = graphiteOverVulkan->recorder();
  ASSERT_EQ(recorder->backend(), skgpu::BackendApi::kVulkan);

  const sk_sp<SkImage> image = media::deviceImage(frame, recorder);
  ASSERT_NE(image, nullptr) << "a frame the recorder cannot sample is drawn from its host copy";
  EXPECT_FALSE(image->isTextureBacked());
  EXPECT_EQ(image->width(), kWidth);
  EXPECT_EQ(image->height(), kHeight);
  // The host copy is the frame: magenta through the codec's rounding.
  SkBitmap read;
  ASSERT_TRUE(read.tryAllocPixels(SkImageInfo::MakeN32Premul(kWidth, kHeight)));
  ASSERT_TRUE(image->readPixels(nullptr, read.pixmap(), 0, 0));
  const SkColor middle = read.getColor(kWidth / 2, kHeight / 2);
  EXPECT_GT(SkColorGetR(middle), 200u);
  EXPECT_LT(SkColorGetG(middle), 60u);
  EXPECT_GT(SkColorGetB(middle), 200u);
  // Read back once: the caller with no recorder is handed the same copy.
  EXPECT_EQ(media::deviceImage(frame, nullptr).get(), image.get());
}
