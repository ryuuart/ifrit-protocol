/** @file
 * Reading a Graphite target back on Metal: a cleared target read whole, a
 * read rescaled into a smaller pixmap that keeps its row padding, and a
 * read waiting on the GPU that leaves the context free for another thread,
 * and the device signal a read sleeps on until the device has run it.
 */

#import <Metal/Metal.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/Readback.h>
#include <sigilskia/graphite/TextureImage.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

#include <gtest/gtest.h>

#include "../Waits.h"
#include "GraphiteReadback.h"
#include "MetalFixtures.h"

using sigil::core::hardware::FenceHandle;
using sigil::core::hardware::FenceValue;
using sigil::core::hardware::GpuDevice;
using sigil::skia::GraphiteContext;
using namespace sigil::skia::test;

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

TEST(SigilSkiaGraphite, ReadbackRescalesPixelsAndPreservesRowPadding) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  const auto surface = SkSurfaces::RenderTarget(ctx->recorder(), SkImageInfo::MakeN32Premul(4, 6));
  ASSERT_NE(surface, nullptr);
  surface->getCanvas()->clear(SK_ColorGREEN);
  SkPaint paint;
  paint.setColor(SK_ColorBLUE);
  surface->getCanvas()->drawRect(SkRect::MakeXYWH(2, 0, 2, 6), paint);
  std::array<uint8_t, 48> storage;
  storage.fill(0xa7);
  const SkPixmap output(SkImageInfo::MakeN32Premul(2, 3), storage.data(), 16);
  ASSERT_TRUE(sigil::skia::readbackPixels(*ctx, *surface, output));
  for (int y = 0; y < 3; ++y) {
    EXPECT_EQ(output.getColor(0, y), SK_ColorGREEN);
    EXPECT_EQ(output.getColor(1, y), SK_ColorBLUE);
    for (int byte = 8; byte < 16; ++byte) EXPECT_EQ(storage[size_t(y) * 16 + size_t(byte)], 0xa7);
  }
  const SkPixmap invalid(output.info(), nullptr, 16);
  EXPECT_FALSE(sigil::skia::readbackPixels(*ctx, *surface, invalid));
}

// A readback's GPU work waits on an event nothing but the CPU raises, so
// the read cannot finish until a second thread raises it — and that thread
// raises it only once it holds the context. A readback that kept the
// context across its wait would starve the second thread until it gave up.
TEST(SigilSkiaGraphite, AReadbackWaitingOnTheGpuLeavesTheContextFree) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  GpuDevice *dev = adoptedDevice();
  ASSERT_NE(dev, nullptr);
  const FenceHandle fence = dev->createFence();
  ASSERT_TRUE(dev->isValid(fence));
  void *event = dev->exportNative(fence).mtlSharedEvent;
  ASSERT_NE(event, nullptr);
  const FenceValue value = dev->completedValue(fence) + 1;

  const auto surface = SkSurfaces::RenderTarget(ctx->recorder(), SkImageInfo::MakeN32Premul(4, 4));
  ASSERT_NE(surface, nullptr);
  sigil::skia::waitBeforeSampling(*ctx->recorder(), event, value);
  surface->getCanvas()->clear(SK_ColorBLUE);

  // The context's own mutex, so the second thread can try it without
  // blocking: a blocking take behind a readback that kept it would never
  // return, since the readback is waiting on that very thread.
  std::mutex *const contextMutex = ctx->lockContext().mutex();
  std::atomic<bool> heldWhileWaiting{false};
  std::thread raiser([&] {
    // Long enough for the read to be submitted and waiting on the event.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto giveUp = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < giveUp) {
      if (contextMutex->try_lock()) {
        heldWhileWaiting = true;
        contextMutex->unlock();
        break;
      }
      std::this_thread::yield();
    }
    ((__bridge id<MTLSharedEvent>)event).signaledValue = value;
  });
  std::array<uint8_t, 64> storage{};
  const SkPixmap output(SkImageInfo::MakeN32Premul(4, 4), storage.data(), 16);
  const bool read = sigil::skia::readbackPixels(*ctx, *surface, output);
  raiser.join();

  EXPECT_TRUE(heldWhileWaiting);
  EXPECT_TRUE(read);
  EXPECT_EQ(output.getColor(0, 0), SK_ColorBLUE);
  dev->destroyFence(fence);
}

// THE SIGNAL A READ SLEEPS ON is raised by the device when it reaches the
// signal, and not before: held behind GPU work that waits on an event only
// the host raises, it stays down past a deadline, and rises once the host
// lets the work run. A signal nothing submits is never raised at all.
TEST(SigilSkiaGraphite, ADeviceSignalRisesWhenTheDeviceReachesIt) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  using Clock = std::chrono::steady_clock;
  {
    sigil::skia::DeviceSignal idle(*ctx->recorder());
    ASSERT_TRUE(idle);
    EXPECT_FALSE(idle.wait(Clock::now() + std::chrono::milliseconds(20)));
  }

  GpuDevice *dev = adoptedDevice();
  ASSERT_NE(dev, nullptr);
  const FenceHandle fence = dev->createFence();
  ASSERT_TRUE(dev->isValid(fence));
  void *event = dev->exportNative(fence).mtlSharedEvent;
  ASSERT_NE(event, nullptr);
  const FenceValue value = dev->completedValue(fence) + 1;

  const auto surface = SkSurfaces::RenderTarget(ctx->recorder(), SkImageInfo::MakeN32Premul(4, 4));
  ASSERT_NE(surface, nullptr);
  sigil::skia::waitBeforeSampling(*ctx->recorder(), event, value);
  surface->getCanvas()->clear(SK_ColorBLUE);
  sigil::skia::DeviceSignal signal(*ctx->recorder());
  ASSERT_TRUE(signal);
  {
    std::unique_ptr<skgpu::graphite::Recording> recording = ctx->recorder()->snap();
    ASSERT_NE(recording, nullptr);
    const auto lock = ctx->lockContext();
    skgpu::graphite::InsertRecordingInfo insert;
    insert.fRecording = recording.get();
    sigil::skia::PendingWaits waits(ctx->recorder());
    waits.attach(insert);
    signal.attach(insert);
    ASSERT_EQ(ctx->context()->insertRecording(insert), skgpu::graphite::InsertStatus::kSuccess);
    ASSERT_TRUE(
        ctx->context()->submit(skgpu::graphite::SubmitInfo(skgpu::graphite::SyncToCpu::kNo)));
    signal.afterSubmit();
  }
  EXPECT_FALSE(signal.wait(Clock::now() + std::chrono::milliseconds(100)))
      << "raised while the device still waits on the host";
  ((__bridge id<MTLSharedEvent>)event).signaledValue = value;
  EXPECT_TRUE(signal.wait(Clock::now() + std::chrono::seconds(5)));
  {
    const auto lock = ctx->lockContext();
    ctx->context()->submit(skgpu::graphite::SubmitInfo(skgpu::graphite::SyncToCpu::kYes));
  }
  dev->destroyFence(fence);
}
