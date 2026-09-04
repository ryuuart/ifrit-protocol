/** @file The native decoded-frame path into a Metal Graphite recorder. */

#import <Metal/Metal.h>

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <include/core/SkRect.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilvideo/decode/Decode.h>
#include <sigilvideo/encode/Encode.h>

#include <array>
#include <cstddef>

TEST(VideoDevice, VideoToolboxFrameWrapsAsGraphiteYuvaImage) {
  constexpr int kWidth = 96;
  constexpr int kHeight = 64;
  auto encoder = sigil::video::Encoder::make(
      sigil::video::Format::Mp4, {.width = kWidth,
                                  .height = kHeight,
                                  .framesPerSecond = 10,
                                  .bitRate = 500'000,
                                  .hardware = sigil::video::HardwarePreference::Disabled});
  ASSERT_NE(encoder, nullptr);
  SkBitmap pixels;
  pixels.allocPixels(SkImageInfo::MakeN32Premul(kWidth, kHeight));
  pixels.eraseColor(SK_ColorMAGENTA);
  ASSERT_TRUE(encoder->append(pixels.pixmap()));
  pixels.eraseColor(SK_ColorCYAN);
  ASSERT_TRUE(encoder->append(pixels.pixmap()));
  const sk_sp<SkData> encoded = encoder->finish();
  ASSERT_NE(encoded, nullptr) << encoder->error();

  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  if (!device) GTEST_SKIP() << "no Metal device";
  id<MTLCommandQueue> queue = [device newCommandQueue];
  ASSERT_NE(queue, nil);
  std::unique_ptr<sigil::skia::GraphiteContext> graphite =
      sigil::skia::GraphiteContext::createMetal((__bridge void*)device, (__bridge void*)queue);
  ASSERT_NE(graphite, nullptr);

  const sigil::video::DecodeOptions options{.hardware = sigil::video::HardwarePreference::Preferred,
                                            .cachedFrames = 2,
                                            .metalDevice = (__bridge void*)device};
  const auto* bytes = static_cast<const std::byte*>(encoded->data());
  const std::shared_ptr<sigil::video::Video> clip =
      sigil::video::decodeVideo(bytes, encoded->size(), options, "device.mp4");
  ASSERT_NE(clip, nullptr);
  const sigil::video::VideoFrame first = clip->frameAt(0.02, graphite->recorder());
  if (!first.hardwareDecoded) GTEST_SKIP() << "VideoToolbox decoder unavailable in this session";
  const sigil::video::VideoFrame second = clip->frameAt(0.12, graphite->recorder());
  ASSERT_TRUE(second.hardwareDecoded);
  const sigil::video::VideoFrame secondAgain = clip->frameAt(0.12, graphite->recorder());

  EXPECT_EQ(first.native.kind, sigil::video::NativeFrame::Kind::VideoToolboxPixelBuffer);
  EXPECT_EQ(second.native.kind, sigil::video::NativeFrame::Kind::VideoToolboxPixelBuffer);
  ASSERT_NE(first.image, nullptr);
  ASSERT_NE(second.image, nullptr);
  EXPECT_TRUE(first.image->isTextureBacked());
  EXPECT_TRUE(second.image->isTextureBacked());
  EXPECT_NE(first.index, second.index);
  EXPECT_EQ(second.image.get(), secondAgain.image.get());

  const SkImageInfo info = SkImageInfo::MakeN32Premul(kWidth * 4, kHeight * 4);
  const sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(graphite->recorder(), info);
  ASSERT_NE(surface, nullptr);
  constexpr std::array<SkRect, 6> destinations = {
      SkRect::MakeXYWH(0, 0, 192, 128),   SkRect::MakeXYWH(128, 32, 192, 128),
      SkRect::MakeXYWH(32, 96, 192, 128), SkRect::MakeXYWH(160, 112, 192, 128),
      SkRect::MakeXYWH(64, 176, 144, 96), SkRect::MakeXYWH(208, 192, 144, 96),
  };
  for (size_t i = 0; i < destinations.size(); ++i) {
    const sk_sp<SkImage>& image = i % 2 == 0 ? first.image : second.image;
    surface->getCanvas()->drawImageRect(image, destinations[i], SkSamplingOptions());
  }
  std::unique_ptr<skgpu::graphite::Recording> recording = graphite->recorder()->snap();
  ASSERT_NE(recording, nullptr);
  skgpu::graphite::InsertRecordingInfo insert;
  insert.fRecording = recording.get();
  ASSERT_TRUE(graphite->context()->insertRecording(insert));
  EXPECT_TRUE(graphite->context()->submit(skgpu::graphite::SyncToCpu::kYes));
}
