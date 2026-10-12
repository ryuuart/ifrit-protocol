/** @file
 * An arrived frame bound for a recorder that cannot sample its Metal
 * texture — Graphite on a Vulkan device — is drawn from its host copy,
 * turned the right way up, rather than not at all.
 *
 * SKIPS where there is no Metal device or no Vulkan runtime.
 */

#import <Metal/Metal.h>

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/gpu/graphite/Recorder.h>
#include <sigilgeometry/device/Device.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../Received.h"

namespace frames = sigil::io::frames;
namespace media = sigil::media;

TEST(ReceivedFrame, ARecorderThatCannotSampleTheArrivalIsHandedItsHostCopy) {
  constexpr int kWidth = 8;
  constexpr int kHeight = 4;
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  if (!device) GTEST_SKIP() << "no Metal device";
  // An arrival as a publisher writes it, first row at the BOTTOM of the
  // picture: the texture's first two rows red, its last two blue.
  MTLTextureDescriptor* described =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                         width:kWidth
                                                        height:kHeight
                                                     mipmapped:NO];
  described.storageMode = MTLStorageModeShared;
  described.usage = MTLTextureUsageShaderRead;
  id<MTLTexture> texture = [device newTextureWithDescriptor:described];
  ASSERT_NE(texture, nil);
  std::vector<uint32_t> texels((size_t)kWidth * kHeight);
  for (int row = 0; row < kHeight; ++row)
    for (int column = 0; column < kWidth; ++column)
      // BGRA in memory: blue, green, red, alpha from the low byte up.
      texels[(size_t)row * kWidth + column] = row < kHeight / 2 ? 0xFFFF0000u : 0xFF0000FFu;
  [texture replaceRegion:MTLRegionMake2D(0, 0, kWidth, kHeight)
             mipmapLevel:0
               withBytes:texels.data()
             bytesPerRow:kWidth * 4];
  const std::shared_ptr<media::DeviceBinding> binding =
      frames::detail::bindArrival(frames::detail::retainTexture((__bridge void*)texture));
  ASSERT_NE(binding, nullptr);

  std::string error;
  static const std::unique_ptr<sigil::geometry::device::Device> vulkan =
      sigil::geometry::device::Device::create({}, &error);
  if (!vulkan || !vulkan->graphite()) GTEST_SKIP() << "no Vulkan Graphite: " << error;
  skgpu::graphite::Recorder* recorder = vulkan->graphite()->recorder();
  ASSERT_EQ(recorder->backend(), skgpu::BackendApi::kVulkan);

  const sk_sp<SkImage> image = binding->image(recorder);
  ASSERT_NE(image, nullptr) << "an arrival the recorder cannot sample is drawn from its host copy";
  EXPECT_FALSE(image->isTextureBacked());
  ASSERT_EQ(image->width(), kWidth);
  ASSERT_EQ(image->height(), kHeight);
  SkBitmap read;
  ASSERT_TRUE(read.tryAllocPixels(SkImageInfo::MakeN32Premul(kWidth, kHeight)));
  ASSERT_TRUE(image->readPixels(nullptr, read.pixmap(), 0, 0));
  // Turned the right way up: the texture's last rows are the picture's top.
  EXPECT_EQ(read.getColor(0, 0), SK_ColorBLUE);
  EXPECT_EQ(read.getColor(0, kHeight - 1), SK_ColorRED);
  // Read back once: a caller with no recorder is handed the same copy.
  EXPECT_EQ(binding->image(nullptr).get(), image.get());
}
