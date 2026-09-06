// Metal bring-up, end to end: a context on a device this process owns, a
// surface over a texture it owns, a clear, and the pixels read back —
// once through Skia's own readback and once through the Metal queue the
// context shares, which is the ordering the asynchronous submit relies on.
// Then the same wrap without naming an API: a GpuDevice over those very
// objects, a texture it created, the surface built from the handle, and a
// fence signalled by the submit. The same three arms on a Vulkan device
// live beside the feature that creates one — a Vulkan device is only ever
// adopted, never made here.

#import <Metal/Metal.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSurface.h>
#include <include/core/SkYUVAInfo.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>
#include <sigilskia/graphite/TextureImage.h>

#include <array>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "GraphiteReadback.h"

using sigil::core::hardware::Backend;
using sigil::core::hardware::FenceHandle;
using sigil::core::hardware::FenceValue;
using sigil::core::hardware::FenceWait;
using sigil::core::hardware::GpuDevice;
using sigil::core::hardware::kFenceInitialValue;
using sigil::core::hardware::NativeDevice;
using sigil::core::hardware::TextureDesc;
using sigil::core::hardware::TextureFormat;
using sigil::core::hardware::TextureHandle;
using sigil::skia::GraphiteContext;
using sigil::skia::OffscreenSurface;
using sigil::skia::test::readGraphiteSurface;

namespace {

id<MTLDevice> device() {
  static id<MTLDevice> d = MTLCreateSystemDefaultDevice();
  return d;
}

id<MTLCommandQueue> queue() {
  static id<MTLCommandQueue> q = [device() newCommandQueue];
  return q;
}

GraphiteContext *graphite() {
  static std::unique_ptr<GraphiteContext> ctx =
      GraphiteContext::createMetal((__bridge void *)device(), (__bridge void *)queue());
  return ctx.get();
}

/** A machine with no Metal device cannot answer anything this file
 *  asks, so a case there reports that it was not run rather than
 *  reporting that the library is broken. The binary carries the `gpu`
 *  label for the same reason. */
#define SKIP_WITHOUT_METAL()                                      \
  do {                                                            \
    if (graphite() == nullptr) GTEST_SKIP() << "no Metal device"; \
  } while (0)

/** A GpuDevice over the very device and queue the context above was
 *  stood up on, so a texture it names is drawn into by that context and
 *  ordered by that one queue. Adopted, so it frees neither. */
GpuDevice *adoptedDevice() {
  static std::unique_ptr<GpuDevice> d = [] {
    NativeDevice native;
    native.backend = Backend::Metal;
    native.mtlDevice = (__bridge void *)device();
    native.mtlCommandQueue = (__bridge void *)queue();
    return GpuDevice::adopt(native);
  }();
  return d.get();
}

/** An 8x8 BGRA render target the device owns, readable by the CPU so the
 *  Metal arms can check the bytes without a copy. */
TextureDesc smallTarget() {
  TextureDesc desc;
  desc.width = 8;
  desc.height = 8;
  desc.format = TextureFormat::BGRA8Unorm;
  desc.cpuAccessible = true;
  return desc;
}

/** An 8x8 BGRA texture no shader may sample: a render target and nothing
 *  else, which is what a wrap refuses after it has taken the release on. */
MTLTextureDescriptor *sampleFreeDescriptor() {
  MTLTextureDescriptor *desc =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                         width:8
                                                        height:8
                                                     mipmapped:NO];
  desc.usage = MTLTextureUsageRenderTarget;
  desc.storageMode = MTLStorageModePrivate;
  return desc;
}

/** The chroma plane beside it: half the size, two channels, and equally
 *  unsampleable. */
MTLTextureDescriptor *sampleFreeChromaDescriptor() {
  MTLTextureDescriptor *desc =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRG8Unorm
                                                         width:4
                                                        height:4
                                                     mipmapped:NO];
  desc.usage = MTLTextureUsageRenderTarget;
  desc.storageMode = MTLStorageModePrivate;
  return desc;
}

/** The bytes of a Metal texture the device names, after a command buffer
 *  committed behind Graphite's own work has completed — the queue
 *  ordering an asynchronous submit relies on, and nothing more. */
std::vector<uint8_t> readMetalBytes(GpuDevice &dev, TextureHandle handle, int size) {
  id<MTLCommandBuffer> barrier = [queue() commandBuffer];
  [barrier commit];
  [barrier waitUntilCompleted];
  std::vector<uint8_t> bytes(size_t(size) * size * 4);
  id<MTLTexture> texture = (__bridge id<MTLTexture>)dev.exportNative(handle).mtlTexture;
  if (!texture) return bytes;
  [texture getBytes:bytes.data()
        bytesPerRow:size_t(size) * 4
         fromRegion:MTLRegionMake2D(0, 0, size, size)
        mipmapLevel:0];
  return bytes;
}

}  // namespace

TEST(SigilSkiaGraphite, CreatesOnTheSystemDevice) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  EXPECT_NE(ctx->context(), nullptr);
  EXPECT_NE(ctx->recorder(), nullptr);
}

TEST(SigilSkiaGraphite, NullHandlesMakeNoContext) {
  EXPECT_EQ(GraphiteContext::createMetal(nullptr, (__bridge void *)queue()), nullptr);
  EXPECT_EQ(GraphiteContext::createMetal((__bridge void *)device(), nullptr), nullptr);
}

TEST(SigilSkiaGraphite, RenderTargetClearsAndReadsBack) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  const SkImageInfo info = SkImageInfo::MakeN32Premul(8, 8);
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(ctx->recorder(), info);
  ASSERT_NE(surface, nullptr);
  surface->getCanvas()->clear(SkColorSetARGB(255, 0, 255, 0));
  const SkBitmap pixels = readGraphiteSurface(*ctx, surface.get());
  ASSERT_FALSE(pixels.empty());
  EXPECT_EQ(pixels.getColor(0, 0), SkColorSetARGB(255, 0, 255, 0));
  EXPECT_EQ(pixels.getColor(7, 7), SkColorSetARGB(255, 0, 255, 0));
}

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

// THE OTHER DIRECTION of the same texture: painted through the surface
// wrap, then read back as an image and drawn onto a second surface. The
// pixels arrive without a copy, and the image outlives every reference
// the test still holds to the texture, because the wrap retains it.
TEST(SigilSkiaGraphite, WrapsATexturePaintedOnItAsAnImage) {
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
  {
    OffscreenSurface painted(*ctx, (__bridge void *)texture, size, size);
    ASSERT_NE(painted.canvas(), nullptr);
    painted.canvas()->clear(SkColorSetARGB(255, 0, 0, 255));
    painted.submit();
  }

  sk_sp<SkImage> image =
      sigil::skia::wrapImage(*ctx->recorder(), (__bridge void *)texture, size, size);
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->width(), size);
  EXPECT_EQ(image->height(), size);

  const SkImageInfo info = SkImageInfo::MakeN32Premul(size, size);
  sk_sp<SkSurface> target = SkSurfaces::RenderTarget(ctx->recorder(), info);
  ASSERT_NE(target, nullptr);
  target->getCanvas()->drawImage(image, 0, 0);
  const SkBitmap pixels = readGraphiteSurface(*ctx, target.get());
  ASSERT_FALSE(pixels.empty());
  EXPECT_EQ(pixels.getColor(0, 0), SkColorSetARGB(255, 0, 0, 255));
  EXPECT_EQ(pixels.getColor(size - 1, size - 1), SkColorSetARGB(255, 0, 0, 255));
}

TEST(SigilSkiaGraphite, WrapsNoImageWithoutATexture) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  EXPECT_EQ(sigil::skia::wrapImage(*ctx->recorder(), nullptr, 8, 8), nullptr);
  EXPECT_EQ(sigil::skia::wrapImage(*ctx->recorder(), (__bridge void *)device(), 0, 8), nullptr);
}

// A REFUSED WRAP OWES THE TEXTURE NOTHING: the wrap binds its release to
// the retained texture before it validates anything, and runs it itself
// on the way out, so a texture a refusal touched is left exactly as
// retained as it was handed over. The guard retain keeps a second release
// from freeing the texture under the case.
TEST(SigilSkiaGraphite, ARefusedWrapLeavesTheRetainCountWhereItWas) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  id<MTLTexture> texture = [device() newTextureWithDescriptor:sampleFreeDescriptor()];
  ASSERT_NE(texture, nil);
  CFTypeRef guard = CFRetain((__bridge CFTypeRef)texture);
  const CFIndex before = CFGetRetainCount(guard);

  // Not sampleable, so the wrap refuses after it has taken the release on.
  EXPECT_EQ(sigil::skia::wrapImage(*ctx->recorder(), (__bridge void *)texture, 8, 8), nullptr);

  EXPECT_EQ(CFGetRetainCount(guard), before);
  CFRelease(guard);
}

// THE PLANAR WRAP HANDS ITS PLANES OVER EXACTLY ONCE, whichever way it
// ends: a refusal it makes itself, a refusal the wrap makes, and a wrap
// that succeeds and holds them until the image is gone.
TEST(SigilSkiaGraphite, PlanarWrapReleasesThePlanesOnce) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  const auto count = [](void *context) { ++*static_cast<int *>(context); };

  int released = 0;
  const SkYUVAInfo info({8, 8}, SkYUVAInfo::PlaneConfig::kY_UV, SkYUVAInfo::Subsampling::k420,
                        kRec709_Limited_SkYUVColorSpace);
  EXPECT_EQ(sigil::skia::wrapImage(*ctx->recorder(), {}, info, nullptr, count, &released), nullptr);
  EXPECT_EQ(released, 1);

  int missing = 0;
  const std::array<sigil::skia::TexturePlane, 2> noTexture{
      sigil::skia::TexturePlane{nullptr, 8, 8}, sigil::skia::TexturePlane{nullptr, 4, 4}};
  EXPECT_EQ(sigil::skia::wrapImage(*ctx->recorder(), noTexture, info, nullptr, count, &missing),
            nullptr);
  EXPECT_EQ(missing, 1);

  // Planes of the right shape that no shader may sample: the refusal is
  // the wrap's own, and it runs the release on its way out.
  int unsampleable = 0;
  id<MTLTexture> luma = [device() newTextureWithDescriptor:sampleFreeDescriptor()];
  id<MTLTexture> chroma = [device() newTextureWithDescriptor:sampleFreeChromaDescriptor()];
  ASSERT_NE(luma, nil);
  ASSERT_NE(chroma, nil);
  const std::array<sigil::skia::TexturePlane, 2> planes{
      sigil::skia::TexturePlane{(__bridge void *)luma, 8, 8},
      sigil::skia::TexturePlane{(__bridge void *)chroma, 4, 4}};
  EXPECT_EQ(sigil::skia::wrapImage(*ctx->recorder(), planes, info, nullptr, count, &unsampleable),
            nullptr);
  EXPECT_EQ(unsampleable, 1);
}

TEST(SigilSkiaGraphite, WrapsPlanesAsOneImage) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  const auto count = [](void *context) { ++*static_cast<int *>(context); };

  MTLTextureDescriptor *lumaDesc =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                         width:8
                                                        height:8
                                                     mipmapped:NO];
  lumaDesc.usage = MTLTextureUsageShaderRead;
  lumaDesc.storageMode = MTLStorageModeShared;
  MTLTextureDescriptor *chromaDesc =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRG8Unorm
                                                         width:4
                                                        height:4
                                                     mipmapped:NO];
  chromaDesc.usage = MTLTextureUsageShaderRead;
  chromaDesc.storageMode = MTLStorageModeShared;
  id<MTLTexture> luma = [device() newTextureWithDescriptor:lumaDesc];
  id<MTLTexture> chroma = [device() newTextureWithDescriptor:chromaDesc];
  ASSERT_NE(luma, nil);
  ASSERT_NE(chroma, nil);

  const SkYUVAInfo info({8, 8}, SkYUVAInfo::PlaneConfig::kY_UV, SkYUVAInfo::Subsampling::k420,
                        kRec709_Limited_SkYUVColorSpace);
  const std::array<sigil::skia::TexturePlane, 2> planes{
      sigil::skia::TexturePlane{(__bridge void *)luma, 8, 8},
      sigil::skia::TexturePlane{(__bridge void *)chroma, 4, 4}};
  int released = 0;
  sk_sp<SkImage> image =
      sigil::skia::wrapImage(*ctx->recorder(), planes, info, nullptr, count, &released);
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->width(), 8);
  EXPECT_EQ(image->height(), 8);
  // The planes are the image's for as long as it lives.
  EXPECT_EQ(released, 0);
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

TEST(SigilSkiaGraphite, StandsOnADeviceAdoptedFromTheHost) {
  // The factory that reads a device rather than raw handles: the one
  // entry point a host holding a hardware device needs, and the same one
  // the Vulkan arms take.
  GpuDevice *dev = adoptedDevice();
  ASSERT_NE(dev, nullptr) << "no Metal device";
  SKIP_WITHOUT_METAL();
  std::unique_ptr<GraphiteContext> ctx = GraphiteContext::create(*dev);
  ASSERT_NE(ctx, nullptr);
  EXPECT_EQ(dev->native().mtlDevice, (__bridge void *)device());

  const TextureHandle handle = dev->createTexture(smallTarget());
  ASSERT_TRUE(dev->isValid(handle));
  OffscreenSurface surface(*ctx, *dev, handle);
  ASSERT_NE(surface.canvas(), nullptr);
  surface.canvas()->clear(SkColorSetARGB(255, 0, 0, 255));

  // The fence is on the same queue as Graphite's submission, so reaching
  // it means the clear has landed.
  const FenceHandle fence = dev->createFence();
  const FenceValue done = surface.submit(*dev, fence);
  ASSERT_EQ(dev->waitCpu(fence, done), FenceWait::Reached);

  // BGRA, opaque blue.
  const std::vector<uint8_t> bytes = readMetalBytes(*dev, handle, 8);
  EXPECT_EQ(bytes[0], 255);
  EXPECT_EQ(bytes[1], 0);
  EXPECT_EQ(bytes[2], 0);
  EXPECT_EQ(bytes[3], 255);

  dev->destroyFence(fence);
  dev->destroy(handle);
}
