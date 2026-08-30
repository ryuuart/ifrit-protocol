// Metal bring-up, end to end: a context on a device this process owns, a
// surface over a texture it owns, a clear, and the pixels read back —
// once through Skia's own readback and once through the Metal queue the
// context shares, which is the ordering the asynchronous submit relies on.

#import <Metal/Metal.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>

#include <cstring>
#include <vector>

#include <gtest/gtest.h>

using sigil::skia::GraphiteContext;
using sigil::skia::OffscreenSurface;

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
  GraphiteContext *ctx = graphite();
  ASSERT_NE(ctx, nullptr) << "no Metal device";
  EXPECT_NE(ctx->context(), nullptr);
  EXPECT_NE(ctx->recorder(), nullptr);
}

TEST(SigilSkiaGraphite, NullHandlesMakeNoContext) {
  EXPECT_EQ(GraphiteContext::createMetal(nullptr, (__bridge void *)queue()), nullptr);
  EXPECT_EQ(GraphiteContext::createMetal((__bridge void *)device(), nullptr), nullptr);
}

TEST(SigilSkiaGraphite, RenderTargetClearsAndReadsBack) {
  GraphiteContext *ctx = graphite();
  ASSERT_NE(ctx, nullptr);
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
  GraphiteContext *ctx = graphite();
  ASSERT_NE(ctx, nullptr);
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
  GraphiteContext *ctx = graphite();
  ASSERT_NE(ctx, nullptr);
  OffscreenSurface surface(*ctx, nullptr, 8, 8);
  EXPECT_EQ(surface.canvas(), nullptr);
  EXPECT_EQ(surface.surface(), nullptr);
}
