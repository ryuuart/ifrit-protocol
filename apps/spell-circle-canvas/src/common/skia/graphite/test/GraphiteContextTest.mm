/** @file
 * The Graphite context on Metal: made on the system device and refused
 * over null handles, made from a device adopted from the host, every
 * recorder of it naming that device, a thread of its own recording on a
 * recorder of its own, and a signal semaphore raised once the recording
 * carrying it has run.
 */

#import <Metal/Metal.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/BackendSemaphore.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <include/gpu/graphite/mtl/MtlGraphiteTypes_cpp.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>
#include <sigilskia/graphite/PaintOrder.h>
#include <sigilskia/graphite/TextureImage.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "GraphiteReadback.h"
#include "MetalFixtures.h"

using sigil::core::hardware::FenceHandle;
using sigil::core::hardware::FenceValue;
using sigil::core::hardware::FenceWait;
using sigil::core::hardware::GpuDevice;
using sigil::core::hardware::TextureHandle;
using sigil::skia::GraphiteContext;
using sigil::skia::OffscreenSurface;
using namespace sigil::skia::test;

TEST(SigilSkiaGraphite, CreatesOnTheSystemDevice) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  EXPECT_NE(ctx->context(), nullptr);
  EXPECT_NE(ctx->recorder(), nullptr);
}

// Metal keeps depth across every pass, so nothing on it is fenced.
TEST(SigilSkiaGraphite, AMetalContextNeedsNoPaintOrderFence) {
  SKIP_WITHOUT_METAL();
  EXPECT_FALSE(sigil::skia::PaintOrderCanvas::needed(*graphite()));
}

TEST(SigilSkiaGraphite, NullHandlesMakeNoContext) {
  EXPECT_EQ(GraphiteContext::createMetal(nullptr, (__bridge void *)queue()), nullptr);
  EXPECT_EQ(GraphiteContext::createMetal((__bridge void *)device(), nullptr), nullptr);
}

// A recording inserted with a Metal signal semaphore raises the
// semaphore's event to its value once the submit that carries it has
// run. The event is the one a device fence keeps behind it, so the CPU
// wait on that fence is what reads the raise; nothing else signals the
// fence, so reaching the value is the semaphore's doing alone.
TEST(SigilSkiaGraphite, ASignalSemaphoreRaisesItsEventOnceTheRecordingHasRun) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  GpuDevice *dev = adoptedDevice();
  ASSERT_NE(dev, nullptr);

  const FenceHandle fence = dev->createFence();
  ASSERT_TRUE(dev->isValid(fence));
  void *event = dev->exportNative(fence).mtlSharedEvent;
  ASSERT_NE(event, nullptr);
  const FenceValue value = dev->completedValue(fence) + 1;

  sk_sp<SkSurface> surface =
      SkSurfaces::RenderTarget(ctx->recorder(), SkImageInfo::MakeN32Premul(8, 8));
  ASSERT_NE(surface, nullptr);
  surface->getCanvas()->clear(SkColorSetARGB(255, 0, 0, 255));
  std::unique_ptr<skgpu::graphite::Recording> recording = ctx->recorder()->snap();
  ASSERT_NE(recording, nullptr);

  skgpu::graphite::BackendSemaphore signal =
      skgpu::graphite::BackendSemaphores::MakeMetal(static_cast<CFTypeRef>(event), value);
  skgpu::graphite::InsertRecordingInfo insert;
  insert.fRecording = recording.get();
  insert.fNumSignalSemaphores = 1;
  insert.fSignalSemaphores = &signal;
  {
    const std::unique_lock<std::mutex> lock = ctx->lockContext();
    ASSERT_TRUE(ctx->context()->insertRecording(insert));
    ASSERT_TRUE(ctx->context()->submit(skgpu::graphite::SyncToCpu::kNo));
  }

  EXPECT_EQ(dev->waitCpu(fence, value), FenceWait::Reached);
  EXPECT_GE(dev->completedValue(fence), value);
  dev->destroyFence(fence);
}

TEST(SigilSkiaGraphite, AThreadOfItsOwnRecordsOnARecorderOfItsOwn) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  std::unique_ptr<skgpu::graphite::Recorder> recorder = ctx->makeRecorder();
  ASSERT_NE(recorder, nullptr);

  // The whole contract in one pass: a second thread draws on a recorder
  // of its own and inserts what it snaps under the context's lock, which
  // is the only thing keeping the two threads off the context at once.
  sk_sp<SkSurface> surface;
  bool inserted = false;
  std::thread([&] {
    surface = SkSurfaces::RenderTarget(recorder.get(), SkImageInfo::MakeN32Premul(8, 8));
    if (!surface) return;
    surface->getCanvas()->clear(SkColorSetARGB(255, 0, 0, 255));
    std::unique_ptr<skgpu::graphite::Recording> recording = recorder->snap();
    if (!recording) return;
    const std::unique_lock<std::mutex> lock = ctx->lockContext();
    skgpu::graphite::InsertRecordingInfo insert;
    insert.fRecording = recording.get();
    inserted = bool(ctx->context()->insertRecording(insert));
    ctx->context()->submit(skgpu::graphite::SubmitInfo(skgpu::graphite::SyncToCpu::kYes));
  }).join();

  ASSERT_NE(surface, nullptr);
  EXPECT_TRUE(inserted);
  const SkBitmap pixels = readGraphiteSurface(*ctx, surface.get());
  ASSERT_FALSE(pixels.empty());
  EXPECT_EQ(pixels.getColor(0, 0), SkColorSetARGB(255, 0, 0, 255));
}

TEST(SigilSkiaGraphite, EveryRecorderOfAContextNamesItsDevice) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  void *metal = (__bridge void *)device();
  EXPECT_EQ(sigil::skia::deviceOf(*ctx->recorder()), metal);
  std::unique_ptr<skgpu::graphite::Recorder> second = ctx->makeRecorder();
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(sigil::skia::deviceOf(*second), metal);
  // A texture on that device is one a second recorder draws on too.
  MTLTextureDescriptor *described =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                         width:4
                                                        height:4
                                                     mipmapped:NO];
  id<MTLTexture> texture = [device() newTextureWithDescriptor:described];
  ASSERT_NE(texture, nil);
  EXPECT_TRUE(sigil::skia::drawsOnDeviceOf(*second, (__bridge void *)texture));
  // A recorder made outside this library cannot be named.
  std::unique_ptr<skgpu::graphite::Recorder> bare = ctx->context()->makeRecorder({});
  ASSERT_NE(bare, nullptr);
  EXPECT_EQ(sigil::skia::deviceOf(*bare), nullptr);
  EXPECT_FALSE(sigil::skia::drawsOnDeviceOf(*bare, (__bridge void *)texture));
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
