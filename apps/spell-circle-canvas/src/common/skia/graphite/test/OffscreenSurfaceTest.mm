/** @file
 * A Graphite surface over a Metal texture: the texture visible to the
 * shared queue, a texture named by a hardware handle, a half-float wrap
 * carrying the colour space it was given, a submit that signals a device
 * fence, wraps over no texture or a stale handle that hold nothing, and a
 * moved-from surface that submits nothing.
 */

#import <Metal/Metal.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSurface.h>
#include <sigilgpu/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>

#include <cstdint>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "GraphiteReadback.h"
#include "MetalFixtures.h"

using sigil::gpu::FenceHandle;
using sigil::gpu::FenceValue;
using sigil::gpu::FenceWait;
using sigil::gpu::GpuDevice;
using sigil::gpu::kFenceInitialValue;
using sigil::gpu::TextureDescription;
using sigil::gpu::TextureFormat;
using sigil::gpu::TextureHandle;
using sigil::skia::GraphiteContext;
using sigil::skia::OffscreenSurface;
using namespace sigil::skia::test;

TEST(SigilSkiaGraphite, WrappedTextureIsVisibleToTheSharedQueue) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  const int size = 8;
  MTLTextureDescriptor *desc =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                         width:size
                                                        height:size
                                                     mipmapped:NO];
  desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
  desc.storageMode = MTLStorageModeShared;
  id<MTLTexture> texture = [device() newTextureWithDescriptor:desc];
  ASSERT_NE(texture, nil);

  OffscreenSurface surface(*ctx, (__bridge void *)texture, size, size);
  ASSERT_NE(surface.canvas(), nullptr);
  surface.canvas()->clear(SkColorSetARGB(255, 255, 0, 0));
  surface.submit();

  // Nothing waits on the CPU: a command buffer committed afterwards on the
  // same queue runs after Graphite's work, so once it completes the clear
  // has landed in the texture.
  id<MTLCommandBuffer> fence = [queue() commandBuffer];
  [fence commit];
  [fence waitUntilCompleted];

  std::vector<uint8_t> bytes(size_t(size) * size * 4);
  [texture getBytes:bytes.data()
        bytesPerRow:size_t(size) * 4
         fromRegion:MTLRegionMake2D(0, 0, size, size)
        mipmapLevel:0];
  // BGRA, opaque red.
  EXPECT_EQ(bytes[0], 0);
  EXPECT_EQ(bytes[1], 0);
  EXPECT_EQ(bytes[2], 255);
  EXPECT_EQ(bytes[3], 255);
  const size_t last = bytes.size() - 4;
  EXPECT_EQ(bytes[last + 2], 255);
}

TEST(SigilSkiaGraphite, NullTextureWrapsNothing) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  OffscreenSurface surface(*ctx, nullptr, 8, 8);
  EXPECT_EQ(surface.canvas(), nullptr);
  EXPECT_EQ(surface.surface(), nullptr);
}

TEST(SigilSkiaGraphite, WrapsATextureNamedByHandle) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  GpuDevice *dev = adoptedDevice();
  ASSERT_NE(dev, nullptr);

  const TextureHandle handle = dev->createTexture(smallTarget());
  ASSERT_TRUE(dev->isValid(handle));
  OffscreenSurface surface(*ctx, *dev, handle);
  ASSERT_NE(surface.canvas(), nullptr);
  surface.canvas()->clear(SkColorSetARGB(255, 255, 0, 0));
  surface.submit();

  // BGRA, opaque red, in the texture the handle names.
  const std::vector<uint8_t> bytes = readMetalBytes(*dev, handle, 8);
  EXPECT_EQ(bytes[0], 0);
  EXPECT_EQ(bytes[1], 0);
  EXPECT_EQ(bytes[2], 255);
  EXPECT_EQ(bytes[3], 255);
  EXPECT_EQ(bytes[bytes.size() - 2], 255);
  dev->destroy(handle);
}

// THE COLOUR MEANING IS THE CALLER'S. A texture's format says how its
// pixels are stored and nothing about which colours they are, so a
// half-float target made to hold linear light carries the colour space
// it was wrapped with, and every draw into it is converted to that.
TEST(SigilSkiaGraphite, AHalfFloatWrapCarriesTheColourSpaceItWasGiven) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  GpuDevice *dev = adoptedDevice();
  ASSERT_NE(dev, nullptr);

  TextureDescription desc;
  desc.width = 8;
  desc.height = 8;
  desc.format = TextureFormat::RGBA16Float;
  const TextureHandle handle = dev->createTexture(desc);
  ASSERT_TRUE(dev->isValid(handle));
  const sk_sp<SkColorSpace> linear = SkColorSpace::MakeSRGBLinear();
  {
    OffscreenSurface surface(*ctx, *dev, handle, linear);
    ASSERT_NE(surface.surface(), nullptr);
    const SkImageInfo info = surface.surface()->imageInfo();
    EXPECT_EQ(info.colorType(), kRGBA_F16_SkColorType);
    ASSERT_NE(info.colorSpace(), nullptr);
    EXPECT_TRUE(SkColorSpace::Equals(info.colorSpace(), linear.get()));
  }
  dev->destroy(handle);
}

TEST(SigilSkiaGraphite, SubmitSignalsAFence) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  GpuDevice *dev = adoptedDevice();
  ASSERT_NE(dev, nullptr);

  const TextureHandle handle = dev->createTexture(smallTarget());
  const FenceHandle fence = dev->createFence();
  OffscreenSurface surface(*ctx, *dev, handle);
  ASSERT_NE(surface.canvas(), nullptr);
  surface.canvas()->clear(SkColorSetARGB(255, 0, 255, 0));

  const FenceValue value = surface.submit(*dev, fence);
  EXPECT_GT(value, kFenceInitialValue);
  // The signal is queued behind the drawing on the one shared queue, so
  // reaching the value is proof the clear has landed.
  EXPECT_EQ(dev->waitCpu(fence, value), FenceWait::Reached);
  EXPECT_GE(dev->completedValue(fence), value);
  const std::vector<uint8_t> bytes = readMetalBytes(*dev, handle, 8);
  EXPECT_EQ(bytes[1], 255);
  EXPECT_EQ(bytes[2], 0);

  dev->destroyFence(fence);
  dev->destroy(handle);
}

TEST(SigilSkiaGraphite, StaleHandleWrapsNothing) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  GpuDevice *dev = adoptedDevice();
  ASSERT_NE(dev, nullptr);

  const TextureHandle handle = dev->createTexture(smallTarget());
  dev->destroy(handle);
  OffscreenSurface surface(*ctx, *dev, handle);
  EXPECT_EQ(surface.canvas(), nullptr);
  EXPECT_EQ(surface.surface(), nullptr);
  // A fence handle that names nothing signals nothing, and says so.
  EXPECT_EQ(surface.submit(*dev, FenceHandle{}), kFenceInitialValue);
}

TEST(SigilSkiaGraphite, AMovedFromSurfaceHoldsNothingAndSubmitsNothing) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  GpuDevice *dev = adoptedDevice();
  ASSERT_NE(dev, nullptr);

  const TextureHandle handle = dev->createTexture(smallTarget());
  ASSERT_TRUE(dev->isValid(handle));
  OffscreenSurface source(*ctx, *dev, handle);
  ASSERT_NE(source.canvas(), nullptr);

  OffscreenSurface moved(std::move(source));
  EXPECT_NE(moved.canvas(), nullptr);
  EXPECT_EQ(source.canvas(), nullptr);
  EXPECT_EQ(source.surface(), nullptr);

  // The wrap that was moved out of submits nothing: it holds no context,
  // so there is no recording of anyone else's work for it to insert and
  // no submission for a fence to stand behind.
  const FenceHandle fence = dev->createFence();
  EXPECT_EQ(source.submit(*dev, fence), kFenceInitialValue);
  EXPECT_EQ(dev->completedValue(fence), kFenceInitialValue);
  source.submit();

  // The wrap that was moved into is the whole surface, and draws.
  moved.canvas()->clear(SkColorSetARGB(255, 0, 255, 0));
  const FenceValue value = moved.submit(*dev, fence);
  EXPECT_GT(value, kFenceInitialValue);
  ASSERT_EQ(dev->waitCpu(fence, value), FenceWait::Reached);
  const std::vector<uint8_t> bytes = readMetalBytes(*dev, handle, 8);
  EXPECT_EQ(bytes[1], 255);

  dev->destroyFence(fence);
  dev->destroy(handle);
}
