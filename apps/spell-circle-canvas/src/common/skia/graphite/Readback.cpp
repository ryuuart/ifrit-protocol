/** @file
 * The synchronous host-memory read over Graphite's asynchronous callback.
 */

#include <include/core/SkImage.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/Readback.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <thread>

#include "Waits.h"

namespace sigil::skia {

namespace {

/** Asks the context to read what it holds into a result handed to
 *  @p callback with @p opaque: the surface's pixels or an image's. */
using ReadRequest =
    std::function<void(skgpu::graphite::Context&, SkImage::ReadPixelsCallback,
                       SkImage::ReadPixelsContext)>;

bool readThrough(GraphiteContext& context, skgpu::graphite::Recorder* recorder,
                 const SkPixmap& pixels, const ReadRequest& request) {
  if (!context.context() || !recorder || !pixels.addr() ||
      pixels.width() <= 0 || pixels.height() <= 0 ||
      !pixels.info().validRowBytes(pixels.rowBytes()))
    return false;
  struct Read {
    std::unique_ptr<const SkImage::AsyncReadResult> result;
    std::atomic<bool> called{false};
  };
  const auto read = std::make_shared<Read>();
  DeviceSignal signal(*recorder);
  bool signalled = false;
  auto recording = recorder->snap();
  {
    // The context is held to insert, queue the read and submit, and not
    // across the wait: the submit does not block, so another thread's
    // insert or submit proceeds while this one's work runs on the GPU.
    const auto lock = context.lockContext();
    if (recording) {
      skgpu::graphite::InsertRecordingInfo insert;
      insert.fRecording = recording.get();
      PendingWaits waits(recorder);
      waits.attach(insert);
      if (context.context()->insertRecording(insert) !=
          skgpu::graphite::InsertStatus::kSuccess)
        return false;
    }
    // The callback owns a reference even if the caller's deadline expires.
    request(
        *context.context(),
        [](SkImage::ReadPixelsContext opaque,
           std::unique_ptr<const SkImage::AsyncReadResult> result) {
          const std::unique_ptr<std::shared_ptr<Read>> retained(
              static_cast<std::shared_ptr<Read>*>(opaque));
          (*retained)->result = std::move(result);
          (*retained)->called.store(true, std::memory_order_release);
        },
        new std::shared_ptr<Read>(read));
    // THE SIGNAL GOES LAST, on an insert of its own after the read is
    // queued, so the device raises it once the copy the read made has
    // run: the submit commits everything inserted before it together.
    if (signal) {
      std::unique_ptr<skgpu::graphite::Recording> marker = recorder->snap();
      skgpu::graphite::InsertRecordingInfo insert;
      insert.fRecording = marker.get();
      signal.attach(insert);
      signalled = marker && context.context()->insertRecording(insert) ==
                                skgpu::graphite::InsertStatus::kSuccess;
    }
    if (!context.context()->submit(
            skgpu::graphite::SubmitInfo(skgpu::graphite::SyncToCpu::kNo)))
      return false;
    if (signalled) signal.afterSubmit();
  }
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(5);
  // THE WAIT IS THE DEVICE'S OWN where it raises a signal the host can
  // wait on: this thread sleeps until the copy has run and holds nothing
  // meanwhile. Completion is then asked under the lock, a call at a time,
  // and the wait between two asks holds nothing; the pause between asks
  // starts short, so a read the device has finished returns at once, and
  // doubles to a millisecond, so a long one wakes this thread rarely
  // where there is no signal to sleep on.
  if (signalled) signal.wait(deadline);
  std::chrono::microseconds pause(signalled ? 5 : 50);
  while (std::chrono::steady_clock::now() < deadline) {
    {
      const auto lock = context.lockContext();
      context.context()->checkAsyncWorkCompletion();
    }
    if (read->called.load(std::memory_order_acquire)) break;
    std::this_thread::sleep_for(pause);
    pause = std::min(pause * 2, std::chrono::microseconds(1000));
  }
  if (!read->called.load(std::memory_order_acquire) || !read->result ||
      read->result->count() != 1 || !read->result->data(0))
    return false;
  const size_t copyBytes = pixels.info().minRowBytes();
  const size_t sourceStride = read->result->rowBytes(0);
  if (sourceStride < copyBytes) return false;
  const auto* source = static_cast<const uint8_t*>(read->result->data(0));
  for (int y = 0; y < pixels.height(); ++y)
    std::memcpy(pixels.writable_addr(0, y), source + size_t(y) * sourceStride,
                copyBytes);
  return true;
}

}  // namespace

bool readbackPixels(GraphiteContext& context, SkSurface& surface,
                    const SkPixmap& pixels) {
  return readThrough(context, context.recorder(), pixels,
                     [&](skgpu::graphite::Context& device,
                         SkImage::ReadPixelsCallback callback,
                         SkImage::ReadPixelsContext opaque) {
                       device.asyncRescaleAndReadPixels(
                           &surface, pixels.info(),
                           SkIRect::MakeWH(surface.width(), surface.height()),
                           SkImage::RescaleGamma::kSrc,
                           SkImage::RescaleMode::kNearest, callback, opaque);
                     });
}

bool readbackPixels(GraphiteContext& context,
                    skgpu::graphite::Recorder& recorder, const SkImage& image,
                    const SkPixmap& pixels) {
  return readThrough(context, &recorder, pixels,
                     [&](skgpu::graphite::Context& device,
                         SkImage::ReadPixelsCallback callback,
                         SkImage::ReadPixelsContext opaque) {
                       device.asyncRescaleAndReadPixels(
                           &image, pixels.info(),
                           SkIRect::MakeWH(image.width(), image.height()),
                           SkImage::RescaleGamma::kSrc,
                           SkImage::RescaleMode::kNearest, callback, opaque);
                     });
}

}  // namespace sigil::skia
