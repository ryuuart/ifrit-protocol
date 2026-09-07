#pragma once

/** @file
 * Reading a Graphite surface back to CPU pixels, for any test binary that
 * draws on one. SigilSkiaGraphite owns the context, so the one spelling of
 * the snap-insert-read-submit-spin sequence lives beside it and every
 * consumer that links the target reads it here.
 */

#include <gpu/graphite/Context.h>
#include <gpu/graphite/Recorder.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSurface.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>

namespace sigil::skia::test {

/** The pixels of a Graphite surface, read back through the context that
 *  drew it: snap and insert whatever the recorder still holds, ask for the
 *  read, submit synchronously, then poll until the callback lands. The
 *  submit is synchronous, so the poll is bounded by a count rather than by
 *  a clock; an empty bitmap says the read never completed. */
inline SkBitmap readGraphiteSurface(GraphiteContext& ctx, SkSurface* surface) {
  SkBitmap bitmap;
  const SkImageInfo info = surface->imageInfo();
  std::unique_ptr<skgpu::graphite::Recording> recording =
      ctx.recorder()->snap();
  // Every call on the context below is under its own lock: the subject
  // may be recording on a thread of its own over this very context, and a
  // caller of this read never holds the lock itself.
  const std::unique_lock<std::mutex> lock = ctx.lockContext();
  if (recording) {
    skgpu::graphite::InsertRecordingInfo insert;
    insert.fRecording = recording.get();
    ctx.context()->insertRecording(insert);
  }
  struct Read {
    std::unique_ptr<const SkImage::AsyncReadResult> result;
    bool called = false;
  } read;
  ctx.context()->asyncRescaleAndReadPixels(
      surface, info, SkIRect::MakeWH(info.width(), info.height()),
      SkImage::RescaleGamma::kSrc, SkImage::RescaleMode::kNearest,
      [](SkImage::ReadPixelsContext c,
         std::unique_ptr<const SkImage::AsyncReadResult> r) {
        auto* out = static_cast<Read*>(c);
        out->result = std::move(r);
        out->called = true;
      },
      &read);
  skgpu::graphite::SubmitInfo submitInfo;
  submitInfo.fSync = skgpu::graphite::SyncToCpu::kYes;
  ctx.context()->submit(submitInfo);
  for (int spin = 0; spin < 5000 && !read.called; ++spin)
    ctx.context()->checkAsyncWorkCompletion();
  if (!read.result) return bitmap;
  // An allocation the machine cannot make is the case's to report, not
  // the process's to die on, and a row is as wide as the surface's colour
  // type makes it — four bytes is N32 alone.
  if (!bitmap.tryAllocPixels(info)) return SkBitmap();
  const auto* src = static_cast<const uint8_t*>(read.result->data(0));
  const size_t rowBytes = read.result->rowBytes(0);
  const size_t copyBytes = info.minRowBytes();
  for (int y = 0; y < info.height(); ++y)
    std::memcpy(bitmap.pixmap().writable_addr(0, y), src + (size_t)y * rowBytes,
                copyBytes);
  return bitmap;
}

/** The same read narrowed to the one pixel at @p x, @p y, unpremultiplied.
 *  Transparent when the read never completed. */
inline SkColor readGraphitePixel(GraphiteContext& ctx, SkSurface* surface,
                                 int x, int y) {
  const SkBitmap pixels = readGraphiteSurface(ctx, surface);
  return pixels.isNull() ? SK_ColorTRANSPARENT : pixels.getColor(x, y);
}

}  // namespace sigil::skia::test
