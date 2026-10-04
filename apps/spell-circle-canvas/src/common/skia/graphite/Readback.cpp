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

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <thread>

namespace sigil::skia {

bool readbackPixels(GraphiteContext& context, SkSurface& surface,
                    const SkPixmap& pixels) {
  if (!context.context() || !context.recorder() || !pixels.addr() ||
      pixels.width() <= 0 || pixels.height() <= 0 ||
      !pixels.info().validRowBytes(pixels.rowBytes()))
    return false;
  auto recording = context.recorder()->snap();
  const auto lock = context.lockContext();
  if (recording) {
    skgpu::graphite::InsertRecordingInfo insert;
    insert.fRecording = recording.get();
    if (context.context()->insertRecording(insert) !=
        skgpu::graphite::InsertStatus::kSuccess)
      return false;
  }
  struct Read {
    std::unique_ptr<const SkImage::AsyncReadResult> result;
    std::atomic<bool> called{false};
  };
  const auto read = std::make_shared<Read>();
  // The callback owns a reference even if the caller's deadline expires.
  context.context()->asyncRescaleAndReadPixels(
      &surface, pixels.info(),
      SkIRect::MakeWH(surface.width(), surface.height()),
      SkImage::RescaleGamma::kSrc, SkImage::RescaleMode::kNearest,
      [](SkImage::ReadPixelsContext opaque,
         std::unique_ptr<const SkImage::AsyncReadResult> result) {
        const std::unique_ptr<std::shared_ptr<Read>> retained(
            static_cast<std::shared_ptr<Read>*>(opaque));
        (*retained)->result = std::move(result);
        (*retained)->called.store(true, std::memory_order_release);
      },
      new std::shared_ptr<Read>(read));
  if (!context.context()->submit(
          skgpu::graphite::SubmitInfo(skgpu::graphite::SyncToCpu::kYes)))
    return false;
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!read->called.load(std::memory_order_acquire) &&
         std::chrono::steady_clock::now() < deadline) {
    context.context()->checkAsyncWorkCompletion();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
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

}  // namespace sigil::skia
