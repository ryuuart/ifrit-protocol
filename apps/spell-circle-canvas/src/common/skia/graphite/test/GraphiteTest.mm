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
#include <include/core/SkSurface.h>
#include <include/core/SkImageInfo.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>
#include <sigilskia/graphite/Pixels.h>

#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using sigil::core::hardware::Backend;
using sigil::core::hardware::FenceHandle;
using sigil::core::hardware::FenceValue;
using sigil::core::hardware::FenceWait;
using sigil::core::hardware::GpuDevice;
using sigil::skia::GraphiteContext;
using sigil::core::hardware::kFenceInitialValue;
using sigil::core::hardware::NativeDevice;
using sigil::skia::OffscreenSurface;
using sigil::core::hardware::TextureDesc;
using sigil::core::hardware::TextureFormat;
using sigil::core::hardware::TextureHandle;

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
#define SKIP_WITHOUT_METAL()                                       \
  do {                                                             \
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

/** Reads a Graphite surface back to CPU pixels: snap, insert, async read,
 *  then a synchronous submit and a spin until the callback lands. */
SkBitmap readback(GraphiteContext &ctx, SkSurface *surface) {
  SkBitmap bm;
  const SkImageInfo info = surface->imageInfo();
  if (auto recording = ctx.recorder()->snap()) {
    skgpu::graphite::InsertRecordingInfo insert;
    insert.fRecording = recording.get();
    ctx.context()->insertRecording(insert);
  }
  struct Read {
    std::unique_ptr<const SkImage::AsyncReadResult> result;
    bool called = false;
  } read;
  ctx.context()->asyncRescaleAndReadPixels(
      surface, info, SkIRect::MakeWH(info.width(), info.height()), SkImage::RescaleGamma::kSrc,
      SkImage::RescaleMode::kNearest,
      [](SkImage::ReadPixelsContext c, std::unique_ptr<const SkImage::AsyncReadResult> r) {
        auto *out = static_cast<Read *>(c);
        out->result = std::move(r);
        out->called = true;
      },
      &read);
  skgpu::graphite::SubmitInfo submitInfo;
  submitInfo.fSync = skgpu::graphite::SyncToCpu::kYes;
  ctx.context()->submit(submitInfo);
  for (int spin = 0; spin < 5000 && !read.called; ++spin) ctx.context()->checkAsyncWorkCompletion();
  if (!read.result) return bm;
  bm.allocPixels(info);
  const auto *src = static_cast<const uint8_t *>(read.result->data(0));
  const size_t srcRowBytes = read.result->rowBytes(0);
  for (int y = 0; y < info.height(); ++y)
    std::memcpy(bm.pixmap().writable_addr(0, y), src + (size_t)y * srcRowBytes,
                std::min(srcRowBytes, bm.rowBytes()));
  return bm;
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
  const SkBitmap pixels = readback(*ctx, surface.get());
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



TEST(SigilSkiaGraphite, StandsOnADeviceAdoptedFromTheHost) {
  // The factory that reads a device rather than raw handles: the one
  // entry point a host holding a hardware device needs, and the same one
  // the Vulkan arms take.
  GpuDevice *dev = adoptedDevice();
  ASSERT_NE(dev, nullptr) << "no Metal device";
  std::unique_ptr<GraphiteContext> ctx = GraphiteContext::create(*dev);
  SKIP_WITHOUT_METAL();
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
