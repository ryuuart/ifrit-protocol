#pragma once

/** @file
 * What a GPU test in this library needs before it can assert a colour:
 * the one device and Graphite context a process may own, and the
 * asynchronous read that turns a Graphite surface back into pixels the
 * CPU can look at.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkRect.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>

namespace sigil::scry::test {

/** The device this process owns, shared by whatever is under test and
 *  every case in the binary. */
inline core::hardware::GpuDevice* sharedDevice() {
  static std::unique_ptr<core::hardware::GpuDevice> device =
      core::hardware::GpuDevice::createOwned();
  return device.get();
}

/** The Graphite context the cases draw with and the subject shares: the
 *  web thread records on its own recorder over it, so every context call
 *  here holds lockContext(). */
inline skia::GraphiteContext* sharedGraphite() {
  static std::unique_ptr<skia::GraphiteContext> graphite =
      sharedDevice() ? skia::GraphiteContext::create(*sharedDevice()) : nullptr;
  return graphite.get();
}

/** Renders the Graphite surface's pending work and reads @p rect back as
 *  premultiplied N32 pixels. A Graphite read is asynchronous, so the
 *  context is pumped until the result lands or the deadline passes; an
 *  empty bitmap says it never landed. */
inline SkBitmap readback(skia::GraphiteContext& graphite, SkSurface* surface,
                         const SkIRect& rect) {
  std::unique_ptr<skgpu::graphite::Recording> recording =
      graphite.recorder()->snap();
  const std::unique_lock<std::mutex> lock = graphite.lockContext();
  if (recording) {
    skgpu::graphite::InsertRecordingInfo info;
    info.fRecording = recording.get();
    graphite.context()->insertRecording(info);
  }

  struct ReadContext {
    std::unique_ptr<const SkImage::AsyncReadResult> result;
    bool called = false;
  } readContext;

  const SkImageInfo info =
      SkImageInfo::MakeN32Premul(rect.width(), rect.height());
  graphite.context()->asyncRescaleAndReadPixels(
      surface, info, rect, SkImage::RescaleGamma::kSrc,
      SkImage::RescaleMode::kNearest,
      [](SkImage::ReadPixelsContext context,
         std::unique_ptr<const SkImage::AsyncReadResult> result) {
        auto* read = static_cast<ReadContext*>(context);
        read->result = std::move(result);
        read->called = true;
      },
      &readContext);

  skgpu::graphite::SubmitInfo submitInfo;
  submitInfo.fSync = skgpu::graphite::SyncToCpu::kYes;
  graphite.context()->submit(submitInfo);

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!readContext.called &&
         std::chrono::steady_clock::now() < deadline) {
    graphite.context()->checkAsyncWorkCompletion();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  SkBitmap pixels;
  if (!readContext.result) return pixels;
  if (!pixels.tryAllocPixels(info)) return SkBitmap();
  const auto* source =
      static_cast<const uint8_t*>(readContext.result->data(0));
  const size_t sourceRow = readContext.result->rowBytes(0);
  const size_t row = (size_t)rect.width() * 4;
  for (int y = 0; y < rect.height(); ++y)
    std::memcpy(pixels.getAddr(0, y), source + (size_t)y * sourceRow, row);
  return pixels;
}

/** The same read narrowed to one pixel, unpremultiplied. Transparent
 *  when the read did not land. */
inline SkColor readbackPixel(skia::GraphiteContext& graphite,
                             SkSurface* surface, int x, int y) {
  const SkBitmap pixels =
      readback(graphite, surface, SkIRect::MakeXYWH(x, y, 1, 1));
  return pixels.isNull() ? SK_ColorTRANSPARENT : pixels.getColor(0, 0);
}

}  // namespace sigil::scry::test
